#ifndef __PWM_APP_H__
#define __PWM_APP_H__

#include <stdint.h>

int pwm_app_init(void);
int pwm_app_set_duty(uint32_t duty);
void pwm_app_close(void);

#endif
