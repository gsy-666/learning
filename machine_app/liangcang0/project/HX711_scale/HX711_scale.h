#ifndef __HX711_SCALE_H__
#define __HX711_SCALE_H__



#define HX711_SCK 111
#define HX711_DOUT 112

int init_hx711_gpio() ;
void fast_write_sck(int val);
int fast_read_dout();
uint32_t Read_HX711(void);
void close_hx711(void);

#endif 