#include "pwm_app.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define PWM_CHIP_PATH        "/sys/class/pwm/pwmchip3"
#define PWM_CHANNEL          0
#define PWM_PERIOD           20000000
#define PWM_DUTY_DEFAULT     500000

static uint32_t current_pwm_duty = PWM_DUTY_DEFAULT;

static int write_text_file(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        return -1;
    }

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
    if (access(path, F_OK) == 0) {
        return 0;
    }

    snprintf(value, sizeof(value), "%d", PWM_CHANNEL);

    char export_path[128];
    snprintf(export_path, sizeof(export_path), "%s/export", PWM_CHIP_PATH);
    return write_text_file(export_path, value);
}

int pwm_app_init(void) {
    char path[128];
    char value[32];

    if (export_pwm_channel() < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/period", PWM_CHIP_PATH, PWM_CHANNEL);
    snprintf(value, sizeof(value), "%d", PWM_PERIOD);
    if (write_text_file(path, value) < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/duty_cycle", PWM_CHIP_PATH, PWM_CHANNEL);
    snprintf(value, sizeof(value), "%d", PWM_DUTY_DEFAULT);
    if (write_text_file(path, value) < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/polarity", PWM_CHIP_PATH, PWM_CHANNEL);
    if (write_text_file(path, "normal") < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, PWM_CHANNEL);
    if (write_text_file(path, "1") < 0) {
        return -1;
    }

    current_pwm_duty = PWM_DUTY_DEFAULT;
    return 0;
}

int pwm_app_set_duty(uint32_t duty) {
    char path[128];
    char value[32];

    if (duty == current_pwm_duty) {
        return 0;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/duty_cycle", PWM_CHIP_PATH, PWM_CHANNEL);
    snprintf(value, sizeof(value), "%u", duty);
    if (write_text_file(path, value) < 0) {
        return -1;
    }

    current_pwm_duty = duty;
    return 0;
}

void pwm_app_close(void) {
    char path[128];
    char value[16];

    snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, PWM_CHANNEL);
    (void)write_text_file(path, "0");

    snprintf(path, sizeof(path), "%s/unexport", PWM_CHIP_PATH);
    snprintf(value, sizeof(value), "%d", PWM_CHANNEL);
    (void)write_text_file(path, value);
}
