#ifndef __HX711_APP_H__
#define __HX711_APP_H__

#include <stdint.h>

int hx711_app_init(void);
void hx711_app_close(void);

void hx711_app_warmup(void);
void hx711_app_tare(void);

uint32_t hx711_app_get_tare_base(void);
uint32_t hx711_app_read_weight_g(void);
uint32_t hx711_app_read_weight_g_median5(void);

#endif
