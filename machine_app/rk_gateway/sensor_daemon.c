#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <linux/i2c-dev.h>

#include "sht31-d.h"

#define DEFAULT_PORT 6667
#define BACKLOG 4

static volatile sig_atomic_t g_should_exit = 0;

static void on_sigint(int signo) {
  (void)signo;
  g_should_exit = 1;
}

static void trim(char *s) {
  if (!s) return;
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
    s[--n] = '\0';
  }
  size_t i = 0;
  while (s[i] == ' ' || s[i] == '\t') i++;
  if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static int ieq(const char *a, const char *b) {
  for (; *a && *b; ++a, ++b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  }
  return *a == '\0' && *b == '\0';
}

static void send_line(int fd, const char *line) {
  if (!line) return;
  (void)write(fd, line, strlen(line));
  (void)write(fd, "\n", 1);
}

static int open_i2c_bus(int bus) {
  char path[32];
  snprintf(path, sizeof(path), "/dev/i2c-%d", bus);
  return open(path, O_RDWR);
}

static int i2c_set_addr(int fd, int addr) {
  return ioctl(fd, I2C_SLAVE, addr);
}

static int sgp30_init_air_quality(int fd) {
  uint8_t buf[2] = {0x20, 0x03};
  return (write(fd, buf, 2) == 2) ? 0 : -1;
}

static int sgp30_measure_air_quality(int fd, int *out_ppm, int *out_tvoc) {
  if (!out_ppm || !out_tvoc) return -1;

  uint8_t cmd[2] = {0x20, 0x08};
  if (write(fd, cmd, 2) != 2) return -1;

  usleep(15000);

  uint8_t data[6] = {0};
  if (read(fd, data, 6) != 6) return -1;

  int co2 = ((int)data[0] << 8) | (int)data[1];
  int tvoc = ((int)data[3] << 8) | (int)data[4];

  *out_ppm = co2;
  *out_tvoc = tvoc;
  return 0;
}

typedef struct {
  float temp_c;
  float hum;
  int co2_ppm;
  int tvoc_ppb;
  unsigned long seq;
  int ok_sht31;
  int ok_sgp30;
} sensor_state_t;

static void sleep_ms(int ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

typedef struct {
  int i2c_bus;
  sensor_state_t *st;
  pthread_mutex_t *mu;
} updater_args_t;

static void *updater_thread(void *arg) {
  updater_args_t *a = (updater_args_t *)arg;

  int i2c_fd = open_i2c_bus(a->i2c_bus);
  if (i2c_fd < 0) {
    perror("open i2c");
    while (!g_should_exit) {
      pthread_mutex_lock(a->mu);
      a->st->ok_sht31 = 0;
      a->st->ok_sgp30 = 0;
      pthread_mutex_unlock(a->mu);
      sleep_ms(1000);
    }
    return NULL;
  }

  int sht_fd = sht31_open(a->i2c_bus, SHT31_DEFAULT_ADDR);

  int sgp_inited = 0;

  while (!g_should_exit) {
    float t = 0.0f, h = 0.0f;
    int co2 = -1, tvoc = -1;
    int ok_sht = 0;
    int ok_sgp = 0;

    if (sht_fd >= 0) {
      int rtn = gettempandhumidity(sht_fd, &t, &h);
      ok_sht = (rtn == SHT31_OK || rtn == SHT31_CRC_CHECK_FAILED) ? 1 : 0;
    }

    if (i2c_set_addr(i2c_fd, 0x58) == 0) {
      if (!sgp_inited) {
        if (sgp30_init_air_quality(i2c_fd) == 0) {
          sgp_inited = 1;
        }
      }
      if (sgp_inited && sgp30_measure_air_quality(i2c_fd, &co2, &tvoc) == 0) {
        ok_sgp = 1;
      }
    }

    pthread_mutex_lock(a->mu);
    a->st->ok_sht31 = ok_sht;
    if (ok_sht) {
      a->st->temp_c = t;
      a->st->hum = h;
    }

    a->st->ok_sgp30 = ok_sgp;
    if (ok_sgp) {
      a->st->co2_ppm = co2;
      a->st->tvoc_ppb = tvoc;
    }

    a->st->seq++;
    pthread_mutex_unlock(a->mu);

    sleep_ms(1000);
  }

  if (sht_fd >= 0) (void)sht31_close(sht_fd);
  close(i2c_fd);
  return NULL;
}

static void serve_loop(int port, sensor_state_t *st, pthread_mutex_t *mu) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return;
  }

  int opt = 1;
  (void)setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons((uint16_t)port);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    perror("bind");
    close(server_fd);
    return;
  }

  if (listen(server_fd, BACKLOG) != 0) {
    perror("listen");
    close(server_fd);
    return;
  }

  fprintf(stderr, "sensor_daemon listening on 127.0.0.1:%d (i2c=%d)\n", port, SHT31_INTERFACE_ADDR);

  while (!g_should_exit) {
    struct sockaddr_in cli;
    socklen_t cli_len = sizeof(cli);
    int client_fd = accept(server_fd, (struct sockaddr *)&cli, &cli_len);
    if (client_fd < 0) {
      if (errno == EINTR) continue;
      perror("accept");
      break;
    }

    FILE *fp = fdopen(client_fd, "r+");
    if (!fp) {
      close(client_fd);
      continue;
    }

    char line[128];
    while (!g_should_exit && fgets(line, sizeof(line), fp) != NULL) {
      trim(line);
      if (line[0] == '\0') continue;

      if (ieq(line, "GET") || ieq(line, "STATUS")) {
        sensor_state_t snap;
        pthread_mutex_lock(mu);
        snap = *st;
        pthread_mutex_unlock(mu);

        char out[192];
        snprintf(out, sizeof(out),
                 "{\"temperature\":%.2f,\"humidity\":%.2f,\"co2\":%d,\"tvoc\":%d,\"ok_sht31\":%d,\"ok_sgp30\":%d,\"seq\":%lu}",
                 snap.temp_c,
                 snap.hum,
                 snap.co2_ppm,
                 snap.tvoc_ppb,
                 snap.ok_sht31,
                 snap.ok_sgp30,
                 snap.seq);
        send_line(client_fd, out);
        continue;
      }

      send_line(client_fd, "ERR unknown_cmd");
    }

    fclose(fp);
  }

  close(server_fd);
}

int main(int argc, char **argv) {
  int port = DEFAULT_PORT;
  int i2c_bus = SHT31_INTERFACE_ADDR;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      port = atoi(argv[++i]);
      continue;
    }
    if (strcmp(argv[i], "--i2c") == 0 && i + 1 < argc) {
      i2c_bus = atoi(argv[++i]);
      continue;
    }
  }

  signal(SIGINT, on_sigint);
  signal(SIGTERM, on_sigint);

  static sensor_state_t st;
  st.temp_c = 0.0f;
  st.hum = 0.0f;
  st.co2_ppm = -1;
  st.tvoc_ppb = -1;
  st.seq = 0;
  st.ok_sht31 = 0;
  st.ok_sgp30 = 0;

  static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

  updater_args_t args;
  args.i2c_bus = i2c_bus;
  args.st = &st;
  args.mu = &mu;

  pthread_t th;
  if (pthread_create(&th, NULL, updater_thread, &args) != 0) {
    perror("pthread_create");
    return 1;
  }

  serve_loop(port, &st, &mu);
  g_should_exit = 1;
  (void)pthread_join(th, NULL);
  return 0;
}
