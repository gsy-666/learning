#define _XOPEN_SOURCE 600

#include "hx711_app.h"

#include <unistd.h>
#include <stdio.h>

#include "HX711_scale.h"

#define MEDIAN_LEN 5
#define MEDIAN     2

static uint32_t buffer[MEDIAN_LEN];
static int medleng = 0;
static uint32_t xd;

static uint32_t pi_weight = 0;
static uint32_t weight = 0;
static uint32_t hx711_xishu = 24578;

int hx711_app_init(void) {
    if (init_hx711_gpio() < 0) {
        return -1;
    }
    return 0;
}

void hx711_app_close(void) {
    close_hx711();
}

void hx711_app_warmup(void) {
    Read_HX711(); usleep(100000);
    Read_HX711(); usleep(100000);
}

void hx711_app_tare(void) {
    uint32_t hx711_dat;

    medleng = 0;
    for (int i = 0; i < MEDIAN_LEN; i++) {
        hx711_dat = Read_HX711();

        if (medleng == 0) {
            buffer[0] = hx711_dat;
            medleng = 1;
        } else {
            for (int j = 0; j < medleng; j++) {
                if (buffer[j] > hx711_dat) {
                    xd = hx711_dat;
                    hx711_dat = buffer[j];
                    buffer[j] = xd;
                }
            }
            buffer[medleng] = hx711_dat;
            medleng++;
        }

        usleep(100000);
    }

    hx711_dat = buffer[MEDIAN];
    pi_weight = (uint32_t)(hx711_dat * 0.01);
}

uint32_t hx711_app_get_tare_base(void) {
    return pi_weight;
}

uint32_t hx711_app_read_weight_g(void) {
    uint32_t hx711_data;
    uint32_t get, aa;

    hx711_data = Read_HX711();
    printf("raw=0x%08X (%u)\n", hx711_data, hx711_data);
    get = (uint32_t)(hx711_data * 0.01);

    if (get > pi_weight) {
        aa = get - pi_weight;
        weight = (uint32_t)((float)aa * 0.00001f * (float)hx711_xishu);
    } else {
        weight = 0;
    }

    return weight;
}

uint32_t hx711_app_read_weight_g_median5(void) {
    uint32_t w = hx711_app_read_weight_g();

    if (medleng == 0) {
        buffer[0] = w;
        medleng = 1;
    } else {
        for (int j = 0; j < medleng; j++) {
            if (buffer[j] > w) {
                xd = w;
                w = buffer[j];
                buffer[j] = xd;
            }
        }
        buffer[medleng] = w;
        medleng++;
    }

    if (medleng >= MEDIAN_LEN) {
        uint32_t final_weight = buffer[MEDIAN];
        medleng = 0;
        return final_weight;
    }

    return w;
}
