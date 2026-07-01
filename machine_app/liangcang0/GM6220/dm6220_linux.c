#include "dm6220_linux.h"

#include <errno.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define P_MAX 12.5f
#define V_MAX 45.0f
#define T_MAX 10.0f

static float clampf(float v, float vmin, float vmax) {
  if (v < vmin) {
    return vmin;
  }
  if (v > vmax) {
    return vmax;
  }
  return v;
}

static int float_to_uint(float x, float x_min, float x_max, int bits) {
  float span = x_max - x_min;
  float offset = x_min;
  return (int)((x - offset) * ((float)((1 << bits) - 1)) / span);
}

static int can_send_frame(int sock, const struct can_frame *frame) {
  ssize_t n = write(sock, frame, sizeof(*frame));
  if (n != (ssize_t)sizeof(*frame)) {
    return -1;
  }
  return 0;
}

int dm6220_open(dm6220_device_t *dev, const char *ifname, uint16_t motor_id, uint16_t master_id) {
  if (!dev || !ifname) {
    return -1;
  }

  int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  if (sock < 0) {
    return -1;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
  if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
    close(sock);
    return -1;
  }

  struct sockaddr_can addr;
  memset(&addr, 0, sizeof(addr));
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return -1;
  }

  dev->socket_fd = sock;
  dev->motor_id = motor_id;
  dev->master_id = master_id;
  return 0;
}

void dm6220_close(dm6220_device_t *dev) {
  if (!dev) {
    return;
  }
  if (dev->socket_fd >= 0) {
    close(dev->socket_fd);
  }
  dev->socket_fd = -1;
}

int dm6220_send_command(dm6220_device_t *dev, dm_motor_cmd_t cmd) {
  if (!dev || dev->socket_fd < 0) {
    return -1;
  }

  struct can_frame frame;
  memset(&frame, 0, sizeof(frame));
  frame.can_id = dev->motor_id+ 0x100;
  frame.can_dlc = 8;

  for (int i = 0; i < 7; ++i) {
    frame.data[i] = 0xFF;
  }

  switch (cmd) {
    case DM_MOTOR_CMD_ENABLE:
      frame.data[7] = 0xFC;
      break;
    case DM_MOTOR_CMD_DISABLE:
      frame.data[7] = 0xFD;
      break;
    case DM_MOTOR_CMD_SAVE_ZERO:
      frame.data[7] = 0xFE;
      break;
    default:
      return -1;
  }

  return can_send_frame(dev->socket_fd, &frame);
}

int dm6220_send_mit(dm6220_device_t *dev, float position, float velocity, float kp, float kd, float torque) {
  if (!dev || dev->socket_fd < 0) {
    return -1;
  }

  position = clampf(position, -P_MAX, P_MAX);
  velocity = clampf(velocity, -V_MAX, V_MAX);
  torque = clampf(torque, -T_MAX, T_MAX);
  kp = clampf(kp, 0.0f, 500.0f);
  kd = clampf(kd, 0.0f, 5.0f);

  uint16_t p_tmp = (uint16_t)float_to_uint(position, -P_MAX, P_MAX, 16);
  uint16_t v_tmp = (uint16_t)float_to_uint(velocity, -V_MAX, V_MAX, 12);
  uint16_t t_tmp = (uint16_t)float_to_uint(torque, -T_MAX, T_MAX, 12);
  uint16_t kp_tmp = (uint16_t)float_to_uint(kp, 0.0f, 500.0f, 12);
  uint16_t kd_tmp = (uint16_t)float_to_uint(kd, 0.0f, 5.0f, 12);

  struct can_frame frame;
  memset(&frame, 0, sizeof(frame));
  frame.can_id = dev->motor_id+ 0x100;
  frame.can_dlc = 8;

  frame.data[0] = (uint8_t)(p_tmp >> 8);
  frame.data[1] = (uint8_t)(p_tmp);
  frame.data[2] = (uint8_t)(v_tmp >> 4);
  frame.data[3] = (uint8_t)((v_tmp & 0x0F) << 4) | (uint8_t)(kp_tmp >> 8);
  frame.data[4] = (uint8_t)(kp_tmp);
  frame.data[5] = (uint8_t)(kd_tmp >> 4);
  frame.data[6] = (uint8_t)((kd_tmp & 0x0F) << 4) | (uint8_t)(t_tmp >> 8);
  frame.data[7] = (uint8_t)(t_tmp);

  return can_send_frame(dev->socket_fd, &frame);
}

int dm6220_send_position_velocity(dm6220_device_t *dev, float position, float velocity) {
  if (!dev || dev->socket_fd < 0) {
    return -1;
  }

  struct can_frame frame;
  memset(&frame, 0, sizeof(frame));
  frame.can_id = dev->motor_id + 0x100;
  frame.can_dlc = 8;

  uint8_t *p = (uint8_t *)&position;
  uint8_t *v = (uint8_t *)&velocity;

  frame.data[0] = p[0];
  frame.data[1] = p[1];
  frame.data[2] = p[2];
  frame.data[3] = p[3];
  frame.data[4] = v[0];
  frame.data[5] = v[1];
  frame.data[6] = v[2];
  frame.data[7] = v[3];

  return can_send_frame(dev->socket_fd, &frame);
}

int dm6220_send_velocity(dm6220_device_t *dev, float velocity) {
  if (!dev || dev->socket_fd < 0) {
    return -1;
  }

  struct can_frame frame;
  memset(&frame, 0, sizeof(frame));
  frame.can_id = dev->motor_id + 0x200;
  frame.can_dlc = 8;

  uint8_t *v = (uint8_t *)&velocity;
  frame.data[0] = v[0];
  frame.data[1] = v[1];
  frame.data[2] = v[2];
  frame.data[3] = v[3];

  return can_send_frame(dev->socket_fd, &frame);
}
