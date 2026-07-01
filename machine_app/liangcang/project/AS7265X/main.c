/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
//in AS7265X.h,change ii2 device node to match your system,default is "/dev/i2c-3" for RK3568
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "AS7265X.h"

static void sleep_ms(unsigned int ms)
{
	usleep(ms * 1000U);
}


int main()
{
	sleep_ms(10000);
	printf("boot......\r\n");
	if (AS7265X_Init() == false)
	{
		printf("AS7265X initialize error.\r\n");
		while (1);
	}
	else
		printf("AS7265X initialize register finished.\r\n");

	// enable interrupt
	AS7265X_Set_Interrupt(true);
	// set gain to 3.7x
	AS7265X_Set_Gain(AS7265X_GAIN_1X);
	// use all channels
	AS7265X_Set_Measurement(AS7265X_MEASUREMENT_MODE2);
	// set integration time to 50ms
	uint8_t integration = 0x12;
	AS7265X_Set_Integration(integration);

		/*
	 * rtrobot model have 4 led,we will disable all LED ,
	 * AS72652 and AS72653 ind led is null.
	 * 1.IND LED
	 * 2.white LED AS72651
	 * 3.IR	AS72652
	 * 4.UV	AS72653
	 * disable all led
	 */
	AS7265X_Set_LED(AS7265X_SELECT_AS72651, AS7265X_LED_IND_DISABLE | AS7265X_LED_DRV_ENABLE | AS7265X_LED_DRV_100MA);
	AS7265X_Set_LED(AS7265X_SELECT_AS72652, AS7265X_LED_IND_DISABLE | AS7265X_LED_DRV_DISABLE);
	AS7265X_Set_LED(AS7265X_SELECT_AS72653, AS7265X_LED_IND_DISABLE | AS7265X_LED_DRV_DISABLE );

	// Get AS7265X temperature
	float temp[3] = {0};
	temp[0] = AS7265X_Get_Temperature(AS7265X_SELECT_AS72651);
	temp[1] = AS7265X_Get_Temperature(AS7265X_SELECT_AS72652);
	temp[2] = AS7265X_Get_Temperature(AS7265X_SELECT_AS72653);
	printf("%.2f,%.2f,%.2f\r\n", temp[0], temp[1], temp[2]);

	while (true)
	{
		// trigger and read each device sequentially
		uint8_t devices[3] = {AS7265X_SELECT_AS72651, AS7265X_SELECT_AS72652, AS7265X_SELECT_AS72653};
		bool all_ready = true;
		for (int i = 0; i < 3; ++i)
		{
			AS7265X_Set_Measurement_Device(devices[i], AS7265X_MEASUREMENT_MODE3);
			if (!AS7265X_Wait_DataReady(devices[i], (unsigned int)(integration * 10)))
			{
				uint8_t status[3] = {0};
				AS7265X_Get_DataStatus_Debug(status, 3);
				printf("not ready: dev=0x%02x status: 0x%02x 0x%02x 0x%02x\n",
				       devices[i], status[0], status[1], status[2]);
				all_ready = false;
				break;
			}
		}
		if (all_ready)
		{
			// read as7265x raw value
			// uint16_t value[18]={0x00};
			// AS7265X_Get_Channel_Raw(value);

			// read as7265x calibrated value
			float value[18];
			AS7265X_Get_Calibrated(value);

			// 410nm,4350nm,460nm,485nm,510nm,535nm,560nm,585nm,610nm,
			// 645nm,680nm,705nm,730nm,760nm,810nm,860nm,900nm,940nm
			printf("410nm\t4350nm\t460nm\t485nm\t510nm\t535nm\t560nm\t585nm\t610nm"
				   "\t645nm\t680nm\t705nm\t730nm\t760nm\t810nm\t860nm\t900nm\t940nm\r\n");
			for (int i = 0; i < 18; ++i)
			{
				printf("%f\t", value[i]);
			}
			printf("\r\n");
		}		
		sleep_ms(100);
	}
	return 0;
}
