#include "dm6220_linux.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

static void sleep_ms(long ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

static int run_cmd(const char *cmd) {
  int rc = system(cmd);
  if (rc == -1) {
    return -1;
  }
  if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
    return 0;
  }
  return -1;
}

static int init_can_interface(const char *ifname) {
  char cmd[128];

  snprintf(cmd, sizeof(cmd), "ip link set %s down", ifname);
  if (run_cmd(cmd) != 0) {
    return -1;
  }

  snprintf(cmd, sizeof(cmd), "ip link set %s type can bitrate 1000000 restart-ms 100", ifname);
  if (run_cmd(cmd) != 0) {
    return -1;
  }

  snprintf(cmd, sizeof(cmd), "ip link set %s up", ifname);
  if (run_cmd(cmd) != 0) {
    return -1;
  }

  return 0;
}

static int parse_command(const char *line, int *value) {
  char *end = NULL;
  long v = strtol(line, &end, 10);
  if (end == line) {
    return -1;
  }
  while (*end == ' ' || *end == '\t' || *end == '\n') {
    ++end;
  }
  if (*end != '\0') {
    return -1;
  }
  *value = (int)v;
  return 0;
}

static float float_from_u32(uint32_t bits) {
  union {
    uint32_t u;
    float f;
  } value;
  value.u = bits;
  return value.f;
}

static float deg_to_rad(float deg) {
  return deg * ((float)M_PI / 180.0f);
}

int main(void) {
  dm6220_device_t dev;
  dev.socket_fd = -1;

  if (init_can_interface("can0") != 0) {
    perror("init_can_interface");
    return 1;
  }

  if (dm6220_open(&dev, "can0", 0x01, 0x00) != 0) {
    perror("dm6220_open");
    return 1;
  }

  if (dm6220_send_command(&dev, DM_MOTOR_CMD_ENABLE) != 0) {
    perror("dm6220_send_command enable");
    dm6220_close(&dev);
    return 1;
  }

  printf("Enter command (0/1/2/3, q=quit):\n");

  char line[64];
  while (fgets(line, sizeof(line), stdin) != NULL) {
    if (line[0] == 'q' || line[0] == 'Q') {
      break;
    }

    int cmd = -1;
    if (parse_command(line, &cmd) != 0) {
      printf("Invalid command. Use 0, 1, 2, 3, or q to quit.\n");
      continue;
    }

    float position = 0.0f;
    if (cmd == 0) {
      position = deg_to_rad(0.0f);
    } else if (cmd == 1) {
      position = deg_to_rad(120.0f);
    } else if (cmd == 2) {
      position = deg_to_rad(240.0f);
    } else if (cmd == 3) {
      position = deg_to_rad(360.0f);
    } else {
      printf("Unknown command: %d. Use 0, 1, 2, 3, or q to quit.\n", cmd);
      continue;
    }

    if (dm6220_send_position_velocity(&dev, position, 0.5f) != 0) {
      perror("dm6220_send_position_velocity");
    }

    sleep_ms(50);
  }

  if (dm6220_send_command(&dev, DM_MOTOR_CMD_DISABLE) != 0) {
    perror("dm6220_send_command disable");
  }

  dm6220_close(&dev);
  return 0;
}
