#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#include "HX711_scale.h"
#include "hx711_app.h"
#include "pwm_app.h"

#define PWM_DUTY_DEFAULT     500000
#define PWM_DUTY_ACTIVE      1000000

static uint32_t read_target_weight(void)
{
    uint32_t value = 0;

    printf("请输入目标重量(g): ");
    fflush(stdout);

    if (scanf("%u", &value) != 1) {
        printf("输入无效，目标重量默认为 0\n");
        return 0;
    }

    return value;
}

void handle_sigint(int sig) {
    (void)sig;
    printf("\n\n程序退出，已释放 GPIO 资源。\n");
    exit(0);
}

int main(void)
{
    signal(SIGINT, handle_sigint);

    printf("--- HX711 高精度电子秤测试 ---\n");

    if (hx711_app_init() < 0) {
        printf("初始化 GPIO 失败！\n");
        return -1;
    }
    atexit(hx711_app_close);

    if (pwm_app_init() < 0) {
        printf("初始化 PWM 失败！\n");
        return -1;
    }
    atexit(pwm_app_close);

    printf("传感器预热中...\n");
    hx711_app_warmup();

    printf("请清空秤台，正在获取皮重...\n");
    hx711_app_tare();
    printf("去皮完成！当前皮重基数: %u\n", hx711_app_get_tare_base());
    printf("====================================\n\n");

    while (1)
    {
        uint32_t target_weight = read_target_weight();
        printf("目标重量: %u g\n", target_weight);

        if (pwm_app_set_duty(PWM_DUTY_ACTIVE) < 0) {
            printf("设置 PWM 占空比失败！\n");
            return -1;
        }

        while (1)
        {
            uint32_t final_weight = hx711_app_read_weight_g_median5();

            if (final_weight >= target_weight) {
                pwm_app_set_duty(PWM_DUTY_DEFAULT);
                printf("\r目标: %4u g  实测重量: %4u g  PWM: %u   \n", target_weight, final_weight, PWM_DUTY_DEFAULT);
                fflush(stdout);
                break;
            } else {
                pwm_app_set_duty(PWM_DUTY_ACTIVE);
            }

            printf("\r目标: %4u g  实测重量: %4u g  PWM: %u   ", target_weight, final_weight, PWM_DUTY_ACTIVE);
            fflush(stdout);
            usleep(20000);
        }
    }

    return 0;
}
