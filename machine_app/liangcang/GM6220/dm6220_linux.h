#ifndef DM6220_LINUX_H
#define DM6220_LINUX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  DM_MOTOR_CMD_ENABLE = 0,
  DM_MOTOR_CMD_DISABLE,
  DM_MOTOR_CMD_SAVE_ZERO,
} dm_motor_cmd_t;

typedef enum {
  DM_MODE_MIT = 0,
  DM_MODE_POSITION_VELOCITY,
  DM_MODE_VELOCITY,
} dm_mode_t;

typedef struct {
  int socket_fd;
  uint16_t motor_id;
  uint16_t master_id;
} dm6220_device_t;

int dm6220_open(dm6220_device_t *dev, const char *ifname, uint16_t motor_id, uint16_t master_id);
void dm6220_close(dm6220_device_t *dev);

int dm6220_send_command(dm6220_device_t *dev, dm_motor_cmd_t cmd);
int dm6220_send_mit(dm6220_device_t *dev, float position, float velocity, float kp, float kd, float torque);
int dm6220_send_position_velocity(dm6220_device_t *dev, float position, float velocity);
int dm6220_send_velocity(dm6220_device_t *dev, float velocity);

#ifdef __cplusplus
}
#endif

#endif
