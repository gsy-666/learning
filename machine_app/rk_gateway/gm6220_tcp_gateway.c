#define _GNU_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "dm6220_linux.h"

// Scale (HX711)
#include "HX711_scale.h"

// Sensors (I2C)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "sht31-d.h"

#define DEFAULT_PORT 6666
#define BACKLOG 4

// PWM for dispensing servo
#define PWM_CHIP_PATH        "/sys/class/pwm/pwmchip3"
#define PWM_CHANNEL          0
#define PWM_PERIOD_NS        20000000
#define PWM_DUTY_STOP_NS     500000
#define PWM_DUTY_RUN_NS      1000000

#define HX711_MEDIAN_LEN     5
#define HX711_MEDIAN_IDX     2

#define HX711_COEF_DEFAULT   24578
#define PICK_TOLERANCE_G     5
#define PICK_TIMEOUT_MS      60000

#define WEIGHT_TTL_MS        300000

#define SENSOR_DAEMON_HOST "127.0.0.1"
#define SENSOR_DAEMON_PORT 6667

static volatile sig_atomic_t g_should_exit = 0;

static pthread_mutex_t g_scale_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_scale_ready = 0;
static uint32_t g_pi_weight = 0;
static uint32_t g_coef = HX711_COEF_DEFAULT;
static uint64_t g_tare_at_ms = 0;

static pthread_mutex_t g_status_mu = PTHREAD_MUTEX_INITIALIZER;
static char g_status_cache[384] = "{\"ok\":1}";
static uint64_t g_status_cache_at_ms = 0;

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
  while (s[i] == ' ' || s[i] == '\t') {
    i++;
  }
  if (i > 0) {
    memmove(s, s + i, strlen(s + i) + 1);
  }
}

static int ieq(const char *a, const char *b) {
  for (; *a && *b; ++a, ++b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  }
  return *a == '\0' && *b == '\0';
}

static int parse_u16(const char *s, uint16_t *out) {
  if (!s || !out) return -1;
  char *end = NULL;
  long v = strtol(s, &end, 0);
  if (end == s || *end != '\0' || v < 0 || v > 0xFFFF) return -1;
  *out = (uint16_t)v;
  return 0;
}

static int parse_i32(const char *s, int *out) {
  if (!s || !out) return -1;
  char *end = NULL;
  long v = strtol(s, &end, 10);
  if (end == s || *end != '\0') return -1;
  *out = (int)v;
  return 0;
}

static int parse_f32(const char *s, float *out) {
  if (!s || !out) return -1;
  char *end = NULL;
  float v = strtof(s, &end);
  if (end == s || *end != '\0') return -1;
  *out = v;
  return 0;
}

static int sensor_daemon_get_json(char *out, size_t out_sz) {
  if (!out || out_sz == 0) return -1;

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SENSOR_DAEMON_PORT);
  if (inet_pton(AF_INET, SENSOR_DAEMON_HOST, &addr.sin_addr) != 1) {
    close(fd);
    return -1;
  }

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }

  (void)write(fd, "GET\n", 4);

  size_t n = 0;
  while (n + 1 < out_sz) {
    char c;
    ssize_t r = read(fd, &c, 1);
    if (r <= 0) break;
    if (c == '\n') break;
    out[n++] = c;
  }
  out[n] = '\0';
  close(fd);

  if (n == 0) return -1;
  return 0;
}

static int write_text_file(const char *path, const char *value) {
  int fd = open(path, O_WRONLY);
  if (fd < 0) return -1;
  size_t len = strlen(value);
  if (write(fd, value, len) != (ssize_t)len) {
    close(fd);
    return -1;
  }
  close(fd);
  return 0;
}

static int export_pwm_channel(void) {
  char path[128];
  char value[16];
  snprintf(path, sizeof(path), "%s/pwm%d", PWM_CHIP_PATH, PWM_CHANNEL);
  if (access(path, F_OK) == 0) return 0;
  snprintf(value, sizeof(value), "%d", PWM_CHANNEL);
  char exp_path[128];
  snprintf(exp_path, sizeof(exp_path), "%s/export", PWM_CHIP_PATH);
  return write_text_file(exp_path, value);
}

static int init_pwm(void) {
  char path[128];
  char value[32];

  if (export_pwm_channel() < 0) return -1;

  snprintf(path, sizeof(path), "%s/pwm%d/period", PWM_CHIP_PATH, PWM_CHANNEL);
  snprintf(value, sizeof(value), "%d", PWM_PERIOD_NS);
  if (write_text_file(path, value) < 0) return -1;

  snprintf(path, sizeof(path), "%s/pwm%d/polarity", PWM_CHIP_PATH, PWM_CHANNEL);
  if (write_text_file(path, "normal") < 0) return -1;

  snprintf(path, sizeof(path), "%s/pwm%d/duty_cycle", PWM_CHIP_PATH, PWM_CHANNEL);
  snprintf(value, sizeof(value), "%d", PWM_DUTY_STOP_NS);
  if (write_text_file(path, value) < 0) return -1;

  snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, PWM_CHANNEL);
  if (write_text_file(path, "1") < 0) return -1;

  return 0;
}

static int set_pwm_duty(uint32_t duty_ns) {
  char path[128];
  char value[32];
  snprintf(path, sizeof(path), "%s/pwm%d/duty_cycle", PWM_CHIP_PATH, PWM_CHANNEL);
  snprintf(value, sizeof(value), "%u", duty_ns);
  return write_text_file(path, value);
}

static void close_pwm(void) {
  char path[128];
  char value[16];

  snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, PWM_CHANNEL);
  (void)write_text_file(path, "0");

  snprintf(path, sizeof(path), "%s/unexport", PWM_CHIP_PATH);
  snprintf(value, sizeof(value), "%d", PWM_CHANNEL);
  (void)write_text_file(path, value);
}

static uint64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static uint32_t median_u32_5(const uint32_t v[HX711_MEDIAN_LEN]) {
  uint32_t a[HX711_MEDIAN_LEN];
  for (int i = 0; i < HX711_MEDIAN_LEN; i++) a[i] = v[i];
  for (int i = 1; i < HX711_MEDIAN_LEN; i++) {
    uint32_t key = a[i];
    int j = i - 1;
    while (j >= 0 && a[j] > key) {
      a[j + 1] = a[j];
      j--;
    }
    a[j + 1] = key;
  }
  return a[HX711_MEDIAN_IDX];
}

static volatile int g_pick_running = 0;
static volatile uint32_t g_pick_target_g = 0;
static volatile uint32_t g_pick_start_weight_g = 0;
static volatile uint32_t g_pick_weight_g = 0;
static volatile int g_pick_ok = 0;
static char g_pick_err[32] = "";

typedef struct {
  uint32_t target_g;
} pick_worker_args_t;

static void send_line_fp(FILE *fp, const char *line);
static int dispense_to_grams(uint32_t target_g, FILE *fp);

static int ensure_scale_ready(void);
static int read_weight_now(uint32_t *out_g);

static void *pick_worker(void *arg) {
  pick_worker_args_t *a = (pick_worker_args_t *)arg;
  uint32_t target_g = a ? a->target_g : 0;
  free(a);

  g_pick_running = 1;
  g_pick_ok = 0;
  g_pick_err[0] = '\0';
  g_pick_target_g = target_g;
  g_pick_start_weight_g = 0;
  g_pick_weight_g = 0;

  // Force a fresh tare for each PICK, matching main_pwm behavior.
  pthread_mutex_lock(&g_scale_mu);
  g_scale_ready = 0;
  g_tare_at_ms = 0;
  pthread_mutex_unlock(&g_scale_mu);

  uint32_t start_g = 0;
  if (read_weight_now(&start_g) == 0) {
    g_pick_start_weight_g = start_g;
  }

  int rc = dispense_to_grams(target_g, NULL);
  if (rc == 0) {
    g_pick_ok = 1;
  } else {
    g_pick_ok = 0;
    if (g_pick_err[0] == '\0') snprintf(g_pick_err, sizeof(g_pick_err), "dispense");
  }
  g_pick_running = 0;
  return NULL;
}

static int hx711_read_tare_and_coef(uint32_t *out_pi_weight, uint32_t *out_coef);
static int hx711_read_weight_g(uint32_t pi_weight, uint32_t coef, uint32_t *out_weight_g);

static int ensure_scale_ready(void) {
  pthread_mutex_lock(&g_scale_mu);

  uint64_t ms = now_ms();
  int need_tare = (!g_scale_ready) || (ms - g_tare_at_ms > WEIGHT_TTL_MS);
  if (!need_tare) {
    pthread_mutex_unlock(&g_scale_mu);
    return 0;
  }

  g_scale_ready = 0;

  if (init_hx711_gpio() < 0) {
    pthread_mutex_unlock(&g_scale_mu);
    return -1;
  }

  Read_HX711(); usleep(100000);
  Read_HX711(); usleep(100000);

  if (hx711_read_tare_and_coef(&g_pi_weight, &g_coef) != 0) {
    close_hx711();
    pthread_mutex_unlock(&g_scale_mu);
    return -1;
  }

  g_tare_at_ms = ms;
  g_scale_ready = 1;
  pthread_mutex_unlock(&g_scale_mu);
  return 0;
}

static int read_weight_now(uint32_t *out_g) {
  if (!out_g) return -1;

  if (ensure_scale_ready() != 0) return -1;

  pthread_mutex_lock(&g_scale_mu);
  uint32_t wbuf[HX711_MEDIAN_LEN] = {0};
  for (int i = 0; i < HX711_MEDIAN_LEN; i++) {
    uint32_t w = 0;
    (void)hx711_read_weight_g(g_pi_weight, g_coef, &w);
    wbuf[i] = w;
    usleep(20000);
  }
  uint32_t w = median_u32_5(wbuf);
  pthread_mutex_unlock(&g_scale_mu);

  *out_g = w;
  return 0;
}

static int hx711_read_tare_and_coef(uint32_t *out_pi_weight, uint32_t *out_coef) {
  if (!out_pi_weight || !out_coef) return -1;

  uint32_t buf[HX711_MEDIAN_LEN] = {0};
  for (int i = 0; i < HX711_MEDIAN_LEN; i++) {
    buf[i] = Read_HX711();
    usleep(100000);
  }

  uint32_t hx711_dat = median_u32_5(buf);
  *out_pi_weight = (uint32_t)(hx711_dat * 0.01);
  *out_coef = HX711_COEF_DEFAULT;
  return 0;
}

static int hx711_read_weight_g(uint32_t pi_weight, uint32_t coef, uint32_t *out_weight_g) {
  if (!out_weight_g) return -1;
  uint32_t hx711_data = Read_HX711();
  uint32_t get = (uint32_t)(hx711_data * 0.01);
  if (get <= pi_weight) {
    *out_weight_g = 0;
    return 0;
  }
  uint32_t aa = get - pi_weight;
  *out_weight_g = (uint32_t)((float)aa * 0.00001f * (float)coef);
  return 0;
}

static int dispense_to_grams(uint32_t target_g, FILE *fp) {
  if (target_g == 0) return -1;

  if (ensure_scale_ready() != 0) {
    snprintf(g_pick_err, sizeof(g_pick_err), "tare");
    return -1;
  }

  if (init_pwm() < 0) {
    snprintf(g_pick_err, sizeof(g_pick_err), "pwm_init");
    return -1;
  }

  if (set_pwm_duty(PWM_DUTY_RUN_NS) != 0) {
    snprintf(g_pick_err, sizeof(g_pick_err), "pwm_run");
    close_pwm();
    return -1;
  }

  uint64_t start_ms = now_ms();
  uint64_t last_push_ms = 0;

  while (1) {
    uint32_t wbuf[HX711_MEDIAN_LEN] = {0};

    pthread_mutex_lock(&g_scale_mu);
    uint32_t pi_weight = g_pi_weight;
    uint32_t coef = g_coef;
    pthread_mutex_unlock(&g_scale_mu);

    for (int i = 0; i < HX711_MEDIAN_LEN; i++) {
      uint32_t cur_abs = 0;
      (void)hx711_read_weight_g(pi_weight, coef, &cur_abs);
      wbuf[i] = cur_abs;

      uint32_t delta = 0;
      uint32_t start_abs = g_pick_start_weight_g;
      if (cur_abs > start_abs) delta = cur_abs - start_abs;

      g_pick_weight_g = delta;

      uint64_t cur_ms = now_ms();
      if (fp && (last_push_ms == 0 || cur_ms - last_push_ms >= 80)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "{\"weight_g\":%u,\"target_g\":%u}", delta, target_g);
        send_line_fp(fp, msg);
        last_push_ms = cur_ms;
      }

      usleep(20000);
    }

    uint32_t abs_w = median_u32_5(wbuf);

    uint32_t delta_w = 0;
    uint32_t start_abs = g_pick_start_weight_g;
    if (abs_w > start_abs) delta_w = abs_w - start_abs;

    g_pick_weight_g = delta_w;

    uint64_t cur_ms = now_ms();

    if (delta_w + PICK_TOLERANCE_G >= target_g) {
      (void)set_pwm_duty(PWM_DUTY_STOP_NS);
      close_pwm();
      return 0;
    }

    if (cur_ms - start_ms > PICK_TIMEOUT_MS) {
      snprintf(g_pick_err, sizeof(g_pick_err), "timeout");
      (void)set_pwm_duty(PWM_DUTY_STOP_NS);
      close_pwm();
      return -1;
    }
  }
}


static int sgp30_read_co2_ppm(int *out_ppm) {
  if (!out_ppm) return -1;

  int fd = open("/dev/i2c-3", O_RDWR);
  if (fd < 0) return -1;

  if (ioctl(fd, I2C_SLAVE, 0x58) < 0) {
    close(fd);
    return -1;
  }

  // Init_air_quality (0x2003)
  {
    uint8_t buf[2] = {0x20, 0x03};
    if (write(fd, buf, 2) != 2) {
      close(fd);
      return -1;
    }
  }

  // Measure_air_quality (0x2008)
  {
    uint8_t buf[2] = {0x20, 0x08};
    if (write(fd, buf, 2) != 2) {
      close(fd);
      return -1;
    }
  }

  usleep(15000);

  uint8_t data[6] = {0};
  if (read(fd, data, 6) != 6) {
    close(fd);
    return -1;
  }

  close(fd);

  int ppm = ((int)data[0] << 8) | (int)data[1];
  *out_ppm = ppm;
  return 0;
}

static int sht31_read_temp_hum(float *out_temp_c, float *out_hum) {
  if (!out_temp_c || !out_hum) return -1;

  int file = sht31_open(SHT31_INTERFACE_ADDR, SHT31_DEFAULT_ADDR);
  if (file < 0) return -1;

  float t = 0.0f, h = 0.0f;
  int rtn = gettempandhumidity(file, &t, &h);
  (void)sht31_close(file);

  if (rtn != SHT31_OK && rtn != SHT31_CRC_CHECK_FAILED) return -1;
  *out_temp_c = t;
  *out_hum = h;
  return 0;
}

static int handle_command(dm6220_device_t *dev, pthread_mutex_t *dev_mu, FILE *fp, char *line) {
  trim(line);
  if (line[0] == '\0') return 0;

  char *save = NULL;
  char *cmd = strtok_r(line, " \t", &save);
  if (!cmd) return 0;

  if (ieq(cmd, "STATUS") || ieq(cmd, "SENSOR")) {
    // Return sensor + pick state, but throttle to avoid UI/log churn.
    const uint64_t THROTTLE_MS = 500;

    uint64_t now = now_ms();
    pthread_mutex_lock(&g_status_mu);
    if (g_status_cache_at_ms != 0 && now - g_status_cache_at_ms < THROTTLE_MS) {
      char cached[384];
      strncpy(cached, g_status_cache, sizeof(cached));
      cached[sizeof(cached) - 1] = '\0';
      pthread_mutex_unlock(&g_status_mu);
      send_line_fp(fp, cached);
      return 0;
    }
    pthread_mutex_unlock(&g_status_mu);

    // sensor payload (original keys expected by app)
    char base[256];
    if (sensor_daemon_get_json(base, sizeof(base)) != 0) {
      snprintf(base, sizeof(base), "{\"temperature\":-1,\"humidity\":-1,\"co2\":-1,\"tvoc\":-1,\"ok_sht31\":0,\"ok_sgp30\":0,\"seq\":0}");
    }

    size_t n = strlen(base);
    if (n > 0 && base[n - 1] == '}') base[n - 1] = '\0';

    char out[384];
    snprintf(out, sizeof(out),
             "%s,\"pick_running\":%d,\"pick_target_g\":%u,\"pick_weight_g\":%u,\"pick_ok\":%d,\"pick_err\":\"%s\"}",
             base, g_pick_running, g_pick_target_g, g_pick_weight_g, g_pick_ok, g_pick_err);

    pthread_mutex_lock(&g_status_mu);
    strncpy(g_status_cache, out, sizeof(g_status_cache));
    g_status_cache[sizeof(g_status_cache) - 1] = '\0';
    g_status_cache_at_ms = now;
    pthread_mutex_unlock(&g_status_mu);

    send_line_fp(fp, out);
    return 0;
  }

  if (ieq(cmd, "WEIGHT")) {
    uint32_t w = 0;
    if (read_weight_now(&w) != 0) {
      send_line_fp(fp, "ERR weight");
      return -1;
    }
    char out[160];
    snprintf(out, sizeof(out),
             "{\"weight_g\":%u,\"pick_running\":%d,\"pick_target_g\":%u,\"pick_weight_g\":%u,\"pick_ok\":%d,\"pick_err\":\"%s\"}",
             w, g_pick_running, g_pick_target_g, g_pick_weight_g, g_pick_ok, g_pick_err);
    send_line_fp(fp, out);
    return 0;
  }

  if (ieq(cmd, "PICK")) {
    char *arg1 = strtok_r(NULL, " \t", &save);
    char *arg2 = strtok_r(NULL, " \t", &save);
    if (!arg1 || !arg2) {
      send_line_fp(fp, "ERR usage: PICK <bin_id> <grams>");
      return -1;
    }

    int bin = -1;
    if (parse_i32(arg1, &bin) != 0 || bin < 0 || bin > 3) {
      send_line_fp(fp, "ERR bad_bin");
      return -1;
    }

    float grams_f = 0.0f;
    if (parse_f32(arg2, &grams_f) != 0 || grams_f <= 0.0f) {
      send_line_fp(fp, "ERR bad_grams");
      return -1;
    }

    // move to bin position first (same mapping as BIN)
    float position = 0.0f;
    if (bin == 0) {
      position = 0.0f;
    } else if (bin == 1) {
      position = (float)M_PI * 2.0f / 3.0f;
    } else if (bin == 2) {
      position = (float)M_PI * 4.0f / 3.0f;
    } else {
      position = (float)M_PI * 2.0f;
    }

    pthread_mutex_lock(dev_mu);
    int rc = dm6220_send_position_velocity(dev, position, 0.5f);
    pthread_mutex_unlock(dev_mu);
    if (rc != 0) {
      send_line_fp(fp, "ERR bin_move");
      return -1;
    }

    if (g_pick_running) {
      send_line_fp(fp, "{\"ok\":0,\"cmd\":\"PICK\",\"err\":\"busy\"}");
      return -1;
    }

    pick_worker_args_t *args = (pick_worker_args_t *)calloc(1, sizeof(*args));
    if (!args) {
      send_line_fp(fp, "{\"ok\":0,\"cmd\":\"PICK\",\"err\":\"oom\"}");
      return -1;
    }
    args->target_g = (uint32_t)lroundf(grams_f);

    pthread_t th;
    if (pthread_create(&th, NULL, pick_worker, args) != 0) {
      free(args);
      send_line_fp(fp, "{\"ok\":0,\"cmd\":\"PICK\",\"err\":\"thread\"}");
      return -1;
    }
    pthread_detach(th);

    send_line_fp(fp, "{\"ok\":1,\"cmd\":\"PICK\",\"running\":1}");
    return 0;
  }

  if (ieq(cmd, "BIN")) {
    char *arg1 = strtok_r(NULL, " \t", &save);
    if (!arg1) {
      send_line_fp(fp, "ERR usage: BIN <0|1|2>");
      return -1;
    }

    int bin = -1;
    if (parse_i32(arg1, &bin) != 0 || bin < 0 || bin > 3) {
      send_line_fp(fp, "ERR bad_bin");
      return -1;
    }

    float position = 0.0f;
    if (bin == 0) {
      position = 0.0f;
    } else if (bin == 1) {
      position = (float)M_PI * 2.0f / 3.0f;
    } else if (bin == 2) {
      position = (float)M_PI * 4.0f / 3.0f;
    } else {
      position = (float)M_PI * 2.0f;
    }

    pthread_mutex_lock(dev_mu);
    int rc = dm6220_send_position_velocity(dev, position, 0.5f);
    pthread_mutex_unlock(dev_mu);

    if (rc != 0) {
      send_line_fp(fp, "ERR bin_move");
      return -1;
    }
    send_line_fp(fp, "OK");
    return 0;
  }

  if (ieq(cmd, "PING")) {
    send_line_fp(fp, "PONG");
    return 0;
  }

  if (ieq(cmd, "ENABLE")) {
    pthread_mutex_lock(dev_mu);
    int rc = dm6220_send_command(dev, DM_MOTOR_CMD_ENABLE);
    pthread_mutex_unlock(dev_mu);

    if (rc != 0) {
      send_line_fp(fp, "ERR enable");
      return -1;
    }
    send_line_fp(fp, "OK");
    return 0;
  }

  if (ieq(cmd, "DISABLE")) {
    pthread_mutex_lock(dev_mu);
    int rc = dm6220_send_command(dev, DM_MOTOR_CMD_DISABLE);
    pthread_mutex_unlock(dev_mu);

    if (rc != 0) {
      send_line_fp(fp, "ERR disable");
      return -1;
    }
    send_line_fp(fp, "OK");
    return 0;
  }

  if (ieq(cmd, "SAVE_ZERO")) {
    pthread_mutex_lock(dev_mu);
    int rc = dm6220_send_command(dev, DM_MOTOR_CMD_SAVE_ZERO);
    pthread_mutex_unlock(dev_mu);

    if (rc != 0) {
      send_line_fp(fp, "ERR save_zero");
      return -1;
    }
    send_line_fp(fp, "OK");
    return 0;
  }

  if (ieq(cmd, "VEL")) {
    char *arg1 = strtok_r(NULL, " \t", &save);
    if (!arg1) {
      send_line_fp(fp, "ERR usage: VEL <float>");
      return -1;
    }
    float v = 0.0f;
    if (parse_f32(arg1, &v) != 0) {
      send_line_fp(fp, "ERR bad_float");
      return -1;
    }

    pthread_mutex_lock(dev_mu);
    int rc = dm6220_send_velocity(dev, v);
    pthread_mutex_unlock(dev_mu);

    if (rc != 0) {
      send_line_fp(fp, "ERR vel");
      return -1;
    }
    send_line_fp(fp, "OK");
    return 0;
  }

  if (ieq(cmd, "POSVEL")) {
    char *arg1 = strtok_r(NULL, " \t", &save);
    char *arg2 = strtok_r(NULL, " \t", &save);
    if (!arg1 || !arg2) {
      send_line_fp(fp, "ERR usage: POSVEL <pos_rad> <vel>");
      return -1;
    }
    float pos = 0.0f, vel = 0.0f;
    if (parse_f32(arg1, &pos) != 0 || parse_f32(arg2, &vel) != 0) {
      send_line_fp(fp, "ERR bad_float");
      return -1;
    }

    pthread_mutex_lock(dev_mu);
    int rc = dm6220_send_position_velocity(dev, pos, vel);
    pthread_mutex_unlock(dev_mu);

    if (rc != 0) {
      send_line_fp(fp, "ERR posvel");
      return -1;
    }
    send_line_fp(fp, "OK");
    return 0;
  }

  if (ieq(cmd, "QUIT") || ieq(cmd, "EXIT")) {
    send_line_fp(fp, "BYE");
    return 1;
  }

  send_line_fp(fp, "ERR unknown_cmd");
  return -1;
}

static void send_line_fp(FILE *fp, const char *line) {
  if (!fp || !line) return;
  fputs(line, fp);
  fputc('\n', fp);
  fflush(fp);
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--port N] [--ifname can0] [--motor-id 0x01] [--master-id 0x00]\n",
          argv0);
}

typedef struct {
  int client_fd;
  dm6220_device_t *dev;
  pthread_mutex_t *dev_mu;
} client_args_t;

static void *client_thread(void *arg) {
  client_args_t *a = (client_args_t *)arg;
  int client_fd = a->client_fd;
  dm6220_device_t *dev = a->dev;
  pthread_mutex_t *dev_mu = a->dev_mu;
  free(a);

  FILE *fp = fdopen(client_fd, "r+");
  if (!fp) {
    close(client_fd);
    return NULL;
  }

  send_line_fp(fp, "OK gm6220_tcp_gateway");

  char line[256];
  while (!g_should_exit && fgets(line, sizeof(line), fp) != NULL) {
    int rc = handle_command(dev, dev_mu, fp, line);
    if (rc == 1) break;
  }

  fclose(fp);
  return NULL;
}

int main(int argc, char **argv) {
  int port = DEFAULT_PORT;
  const char *ifname = "can0";
  uint16_t motor_id = 0x01;
  uint16_t master_id = 0x00;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      if (parse_i32(argv[++i], &port) != 0 || port <= 0 || port > 65535) {
        usage(argv[0]);
        return 2;
      }
      continue;
    }
    if (strcmp(argv[i], "--ifname") == 0 && i + 1 < argc) {
      ifname = argv[++i];
      continue;
    }
    if (strcmp(argv[i], "--motor-id") == 0 && i + 1 < argc) {
      if (parse_u16(argv[++i], &motor_id) != 0) {
        usage(argv[0]);
        return 2;
      }
      continue;
    }
    if (strcmp(argv[i], "--master-id") == 0 && i + 1 < argc) {
      if (parse_u16(argv[++i], &master_id) != 0) {
        usage(argv[0]);
        return 2;
      }
      continue;
    }

    usage(argv[0]);
    return 2;
  }

  signal(SIGINT, on_sigint);
  signal(SIGTERM, on_sigint);
  signal(SIGPIPE, SIG_IGN);

  dm6220_device_t dev;
  dev.socket_fd = -1;

  pthread_mutex_t dev_mu = PTHREAD_MUTEX_INITIALIZER;

  if (dm6220_open(&dev, ifname, motor_id, master_id) != 0) {
    perror("dm6220_open");
    return 1;
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    dm6220_close(&dev);
    return 1;
  }

  int opt = 1;
  (void)setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons((uint16_t)port);

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    perror("bind");
    close(server_fd);
    dm6220_close(&dev);
    return 1;
  }

  if (listen(server_fd, BACKLOG) != 0) {
    perror("listen");
    close(server_fd);
    dm6220_close(&dev);
    return 1;
  }

  fprintf(stderr, "gm6220_tcp_gateway listening on 0.0.0.0:%d (ifname=%s motor_id=0x%X master_id=0x%X)\n",
          port, ifname, motor_id, master_id);

  while (!g_should_exit) {
    struct sockaddr_in cli;
    socklen_t cli_len = sizeof(cli);
    int client_fd = accept(server_fd, (struct sockaddr *)&cli, &cli_len);
    if (client_fd < 0) {
      if (errno == EINTR) continue;
      perror("accept");
      break;
    }

    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
    fprintf(stderr, "client connected: %s:%u\n", ip, ntohs(cli.sin_port));

    client_args_t *args = (client_args_t *)calloc(1, sizeof(*args));
    if (!args) {
      close(client_fd);
      continue;
    }
    args->client_fd = client_fd;
    args->dev = &dev;
    args->dev_mu = &dev_mu;

    pthread_t th;
    if (pthread_create(&th, NULL, client_thread, args) != 0) {
      free(args);
      close(client_fd);
      continue;
    }
    pthread_detach(th);
  }

  close(server_fd);
  dm6220_close(&dev);
  return 0;
}
