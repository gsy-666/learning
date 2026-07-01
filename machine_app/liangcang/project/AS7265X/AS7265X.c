#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "AS7265X.h"

static int g_as7265x_fd = -1;

static const uint8_t k_as7265x_devices[] = {
	AS7265X_SELECT_AS72651,
	AS7265X_SELECT_AS72652,
	AS7265X_SELECT_AS72653,
};

static void sleep_ms(unsigned int ms);
uint16_t AS7265X_ReadCommand(uint8_t reg_addr, uint8_t *rev_data, size_t length);
void AS7265X_WriteCommand(uint8_t reg_addr, uint8_t *send_data, uint16_t length);

#define AS7265X_POLL_DELAY_MS 2
#define AS7265X_POLL_MAX_TRIES 200
#define AS7265X_READ_RETRIES 3

static bool AS7265X_Read_Virtual(uint8_t virtualReg, uint8_t *out)
{
	uint8_t status = 0;
	uint8_t d = 0;
	bool tx_ready = false;
	for (int tries = 0; tries < AS7265X_POLL_MAX_TRIES; ++tries)
	{
		if (AS7265X_ReadCommand(I2C_AS72XX_SLAVE_STATUS_REG, &status, 1) != 0)
		{
			sleep_ms(AS7265X_POLL_DELAY_MS);
			continue;
		}
		if ((status & I2C_AS72XX_SLAVE_TX_VALID) == 0)
		{
			tx_ready = true;
			break;
		}
		sleep_ms(AS7265X_POLL_DELAY_MS);
	}
	if (!tx_ready)
		return false;

	AS7265X_WriteCommand(I2C_AS72XX_SLAVE_WRITE_REG, &virtualReg, 1);
	bool rx_ready = false;
	for (int tries = 0; tries < AS7265X_POLL_MAX_TRIES; ++tries)
	{
		if (AS7265X_ReadCommand(I2C_AS72XX_SLAVE_STATUS_REG, &status, 1) != 0)
		{
			sleep_ms(AS7265X_POLL_DELAY_MS);
			continue;
		}
		if ((status & I2C_AS72XX_SLAVE_RX_VALID) != 0)
		{
			rx_ready = true;
			break;
		}
		sleep_ms(AS7265X_POLL_DELAY_MS);
	}
	if (!rx_ready)
		return false;

	AS7265X_ReadCommand(I2C_AS72XX_SLAVE_READ_REG, &d, 1);
	*out = d;
	return true;
}

static bool AS7265X_Read_Virtual_Block(uint8_t baseReg, uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		bool ok = false;
		for (int r = 0; r < AS7265X_READ_RETRIES; ++r)
		{
			if (AS7265X_Read_Virtual((uint8_t)(baseReg + i), &buf[i]))
			{
				ok = true;
				break;
			}
		}
		if (!ok)
			return false;
	}
	return true;
}

static uint16_t AS7265X_Read_Raw(uint8_t devsel, uint8_t reg_h)
{
	uint8_t rev_data[2] = {0};
	AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, devsel);
	if (!AS7265X_Read_Virtual_Block(reg_h, rev_data, 2))
		return 0;
	return (uint16_t)rev_data[0] << 8 | rev_data[1];
}

static float AS7265X_Read_Calibrated(uint8_t devsel, uint8_t reg_c0)
{
	uint8_t rev_data[4] = {0};
	AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, devsel);
	if (!AS7265X_Read_Virtual_Block(reg_c0, rev_data, 4))
		return 0.0f;
	uint32_t tmp_u32 = ((uint32_t)rev_data[0] << 24) | ((uint32_t)rev_data[1] << 16) |
			       ((uint32_t)rev_data[2] << 8) | ((uint32_t)rev_data[3]);
	float tmp_float = 0.0f;
	memcpy(&tmp_float, &tmp_u32, sizeof(float));
	return tmp_float;
}

static void sleep_ms(unsigned int ms)
{
	usleep(ms * 1000U);
}

static int i2c_write_reg(int fd, uint8_t addr, uint8_t reg, const uint8_t *data, size_t len)
{
	if (len > 255)
		return -1;
	unsigned char buf[256];
	buf[0] = reg;
	if (len > 0)
		memcpy(buf + 1, data, len);

	struct i2c_msg msgs[1] = {
		{
			.addr = addr,
			.flags = 0,
			.len = (uint16_t)(len + 1),
			.buf = buf,
		}
	};

	struct i2c_rdwr_ioctl_data i2c_msgs = {
		.msgs = msgs,
		.nmsgs = 1,
	};

	return ioctl(fd, I2C_RDWR, &i2c_msgs);
}

static int i2c_read_reg(int fd, uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
	struct i2c_msg msgs[2] = {
		{
			.addr = addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = addr,
			.flags = I2C_M_RD,
			.len = (uint16_t)len,
			.buf = data,
		}
	};

	struct i2c_rdwr_ioctl_data i2c_msgs = {
		.msgs = msgs,
		.nmsgs = 2,
	};

	return ioctl(fd, I2C_RDWR, &i2c_msgs);
}

/***************************************************************************************************************
AS7265X Read Command
****************************************************************************************************************/
uint16_t AS7265X_ReadCommand(uint8_t reg_addr, uint8_t *rev_data, size_t length)
{
	if (g_as7265x_fd < 0)
		return 1;
	if (i2c_read_reg(g_as7265x_fd, AS7265X_I2CADDR, reg_addr, rev_data, length) < 0)
		return 1;
	return 0;
}

/***************************************************************************************************************
AS7265X Write Command
****************************************************************************************************************/
void AS7265X_WriteCommand(uint8_t reg_addr, uint8_t *send_data, uint16_t length)
{
	if (g_as7265x_fd < 0)
		return;
	(void)i2c_write_reg(g_as7265x_fd, AS7265X_I2CADDR, reg_addr, send_data, length);
}

uint8_t AS7265X_i2cm_AS72xx_read(uint8_t virtualReg)
{
	uint8_t d = 0;
	if (!AS7265X_Read_Virtual(virtualReg, &d))
		return 0;
	return d;
}

void AS7265X_i2cm_AS72xx_write(uint8_t virtualReg, uint8_t d)
{
	uint8_t status;
	bool tx_ready = false;
	for (int tries = 0; tries < AS7265X_POLL_MAX_TRIES; ++tries)
	{
		// Read slave I2C status to see if we can write the reg address.
		if (AS7265X_ReadCommand(I2C_AS72XX_SLAVE_STATUS_REG, &status, 1) != 0)
		{
			sleep_ms(AS7265X_POLL_DELAY_MS);
			continue;
		}
		if ((status & I2C_AS72XX_SLAVE_TX_VALID) == 0)
			// No inbound TX pending at slave. Okay to write now.
		{
			tx_ready = true;
			break;
		}
		sleep_ms(AS7265X_POLL_DELAY_MS);
	}
	if (!tx_ready)
		return;

	// Send the virtual register address
	// (setting bit 7 to indicate a pending write).
	virtualReg = (virtualReg | 0x80);
	AS7265X_WriteCommand(I2C_AS72XX_SLAVE_WRITE_REG, &virtualReg, 1);
	tx_ready = false;
	for (int tries = 0; tries < AS7265X_POLL_MAX_TRIES; ++tries)
	{
		// Read the slave I2C status to see if we can write the data byte.
		if (AS7265X_ReadCommand(I2C_AS72XX_SLAVE_STATUS_REG, &status, 1) != 0)
		{
			sleep_ms(AS7265X_POLL_DELAY_MS);
			continue;
		}
		if ((status & I2C_AS72XX_SLAVE_TX_VALID) == 0)
			// No inbound TX pending at slave. Okay to write data now.
		{
			tx_ready = true;
			break;
		}
		sleep_ms(AS7265X_POLL_DELAY_MS);
	}
	if (!tx_ready)
		return;

	// Send the data to complete the operation.
	AS7265X_WriteCommand(I2C_AS72XX_SLAVE_WRITE_REG, &d, 1);
}


/***************************************************************************************************************
AS7265X Get ID
****************************************************************************************************************/
bool AS7265X_Get_ID(void)
{
	uint8_t hw_ver_h = AS7265X_i2cm_AS72xx_read(AS7265X_REG_HWVERSION_H);
	uint8_t hw_ver_l = AS7265X_i2cm_AS72xx_read(AS7265X_REG_HWVERSION_L);
	printf("AS7265X hw_ver_h=0x%02x hw_ver_l=0x%02x\n", hw_ver_h, hw_ver_l);
	if (hw_ver_h != 0x40 || hw_ver_l != 0x41)
		return false;
	return true;
}

/***************************************************************************************************************
AS7265X Get Temperature
****************************************************************************************************************/
float AS7265X_Get_Temperature(uint8_t hw)
{
	AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, hw);
	uint8_t temp = AS7265X_i2cm_AS72xx_read(AS7265X_REG_TEMPERATURE);
	return (float)temp;
}

/***************************************************************************************************************
AS7265X Set LED
****************************************************************************************************************/
void AS7265X_Set_LED(uint8_t hw, uint8_t value)
{
	AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, hw);
	AS7265X_i2cm_AS72xx_write(AS7265X_REG_LEDCONFIGURATION, value);
}

/***************************************************************************************************************
AS7265X Set Interrupt
****************************************************************************************************************/
void AS7265X_Set_Interrupt(bool mode)
{
	for (size_t i = 0; i < sizeof(k_as7265x_devices); ++i)
	{
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, k_as7265x_devices[i]);
		uint8_t rev_data = AS7265X_i2cm_AS72xx_read(AS7265X_REG_CONFIGURATION);
		if (mode == true)
			rev_data |= 0x40;
		else
			rev_data &= 0xBF;
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_CONFIGURATION, rev_data);
	}
}

/***************************************************************************************************************
AS7265X Set Gain
****************************************************************************************************************/
void AS7265X_Set_Gain(uint8_t value)
{
	for (size_t i = 0; i < sizeof(k_as7265x_devices); ++i)
	{
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, k_as7265x_devices[i]);
		uint8_t rev_data = AS7265X_i2cm_AS72xx_read(AS7265X_REG_CONFIGURATION);
		rev_data &= ~(0x30);
		rev_data |= value;
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_CONFIGURATION, rev_data);
	}
}

/***************************************************************************************************************
AS7265X Set Measurement
Measurement mode:
	Mode 0: 4 channels
	Mode 1: 4 channels
	Mode 2: All 6 channels
	Mode 3: One-Shot operation of mode 2
****************************************************************************************************************/
void AS7265X_Set_Measurement(uint8_t value)
{
	for (size_t i = 0; i < sizeof(k_as7265x_devices); ++i)
	{
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, k_as7265x_devices[i]);
		uint8_t rev_data = AS7265X_i2cm_AS72xx_read(AS7265X_REG_CONFIGURATION);
		rev_data &= ~(0x0c);
		rev_data |= value;
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_CONFIGURATION, rev_data);
	}
}

/***************************************************************************************************************
AS7265X Set Integration
 Integration time = <value> * 2.8ms (applies to all channels); value: 1-255
****************************************************************************************************************/
void AS7265X_Set_Integration(uint8_t value)
{
	for (size_t i = 0; i < sizeof(k_as7265x_devices); ++i)
	{
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, k_as7265x_devices[i]);
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_INTEGRATIONTIME, value);
	}
}

/***************************************************************************************************************
AS7265X Get data status
****************************************************************************************************************/
bool AS7265X_Get_DataStatus(void)
{
	for (size_t i = 0; i < sizeof(k_as7265x_devices); ++i)
	{
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, k_as7265x_devices[i]);
		uint8_t status = AS7265X_i2cm_AS72xx_read(AS7265X_REG_CONFIGURATION);
		if ((status & 0x02) != 0x02)
			return false;
	}
	return true;
}

bool AS7265X_Get_DataStatus_Device(uint8_t devsel)
{
	AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, devsel);
	uint8_t status = AS7265X_i2cm_AS72xx_read(AS7265X_REG_CONFIGURATION);
	return ((status & 0x02) == 0x02);
}

void AS7265X_Set_Measurement_Device(uint8_t devsel, uint8_t value)
{
	AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, devsel);
	uint8_t rev_data = AS7265X_i2cm_AS72xx_read(AS7265X_REG_CONFIGURATION);
	rev_data &= ~(0x0c);
	rev_data |= value;
	AS7265X_i2cm_AS72xx_write(AS7265X_REG_CONFIGURATION, rev_data);
}

bool AS7265X_Wait_DataReady(uint8_t devsel, unsigned int timeout_ms)
{
	unsigned int waited = 0;
	while (waited < timeout_ms)
	{
		if (AS7265X_Get_DataStatus_Device(devsel))
			return true;
		sleep_ms(AS7265X_POLL_DELAY_MS);
		waited += AS7265X_POLL_DELAY_MS;
	}
	return false;
}

bool AS7265X_Get_DataStatus_Debug(uint8_t *status_out, size_t len)
{
	if (len < sizeof(k_as7265x_devices))
		return false;
	for (size_t i = 0; i < sizeof(k_as7265x_devices); ++i)
	{
		AS7265X_i2cm_AS72xx_write(AS7265X_REG_DEVSEL, k_as7265x_devices[i]);
		status_out[i] = AS7265X_i2cm_AS72xx_read(AS7265X_REG_CONFIGURATION);
	}
	return true;
}


/***************************************************************************************************************
AS7265X Get raw value
****************************************************************************************************************/
//A: 410nm
uint16_t AS7265X_Get_Channel_RawA()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72653, AS7265X_REG_RAW_RGA_H);
}
//B: 435nm
uint16_t AS7265X_Get_Channel_RawB()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72653, AS7265X_REG_RAW_SHB_H);
}
//C: 460nm
uint16_t AS7265X_Get_Channel_RawC()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72653, AS7265X_REG_RAW_TIC_H);
}
//D: 485nm
uint16_t AS7265X_Get_Channel_RawD()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72653, AS7265X_REG_RAW_UJD_H);
}
//E: 510nm
uint16_t AS7265X_Get_Channel_RawE()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72653, AS7265X_REG_RAW_VKE_H);
}
//F: 535nm
uint16_t AS7265X_Get_Channel_RawF()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72653, AS7265X_REG_RAW_WLF_H);
}
//G: 560nm
uint16_t AS7265X_Get_Channel_RawG()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72652, AS7265X_REG_RAW_RGA_H);
}
//H: 585nm
uint16_t AS7265X_Get_Channel_RawH()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72652, AS7265X_REG_RAW_SHB_H);
}
//I: 645nm
uint16_t AS7265X_Get_Channel_RawI()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72652, AS7265X_REG_RAW_TIC_H);
}
//J: 705nm
uint16_t AS7265X_Get_Channel_RawJ()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72652, AS7265X_REG_RAW_UJD_H);
}
//K: 900nm
uint16_t AS7265X_Get_Channel_RawK()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72652, AS7265X_REG_RAW_VKE_H);
}
//L: 940nm
uint16_t AS7265X_Get_Channel_RawL()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72652, AS7265X_REG_RAW_WLF_H);
}
//R: 610nm
uint16_t AS7265X_Get_Channel_RawR()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72651, AS7265X_REG_RAW_RGA_H);
}
//S: 680nm
uint16_t AS7265X_Get_Channel_RawS()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72651, AS7265X_REG_RAW_SHB_H);
}
//T: 730nm
uint16_t AS7265X_Get_Channel_RawT()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72651, AS7265X_REG_RAW_TIC_H);
}
//U: 760nm
uint16_t AS7265X_Get_Channel_RawU()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72651, AS7265X_REG_RAW_UJD_H);
}
//V: 810nm
uint16_t AS7265X_Get_Channel_RawV()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72651, AS7265X_REG_RAW_VKE_H);
}
//W: 860nm
uint16_t AS7265X_Get_Channel_RawW()
{
	return AS7265X_Read_Raw(AS7265X_SELECT_AS72651, AS7265X_REG_RAW_WLF_H);
}

//410nm,4350nm,460nm,485nm,510nm,535nm,560nm,585nm,610nm,645nm,680nm,705nm,730nm,760nm,810nm,860nm,900nm,940nm
void AS7265X_Get_Channel_Raw(uint16_t* value)
{
	//410nm
	value[0] = AS7265X_Get_Channel_RawA();
	//4350nm
	value[1] = AS7265X_Get_Channel_RawB();
	//460nm
	value[2] = AS7265X_Get_Channel_RawC();
	//485nm
	value[3] = AS7265X_Get_Channel_RawD();
	//510nm
	value[4] = AS7265X_Get_Channel_RawE();
	//535nm
	value[5] = AS7265X_Get_Channel_RawF();
	//560nm
	value[6] = AS7265X_Get_Channel_RawG();
	//585nm
	value[7] = AS7265X_Get_Channel_RawH();
	//610nm
	value[8] = AS7265X_Get_Channel_RawR();
	//645nm
	value[9] = AS7265X_Get_Channel_RawI();
	//680nm
	value[10] = AS7265X_Get_Channel_RawS();
	//705nm
	value[11] = AS7265X_Get_Channel_RawJ();
	//730nm
	value[12] = AS7265X_Get_Channel_RawT();
	//760nm
	value[13] = AS7265X_Get_Channel_RawU();
	//810nm
	value[14] = AS7265X_Get_Channel_RawV();
	//860nm
	value[15] = AS7265X_Get_Channel_RawW();
	//900nm
	value[16] = AS7265X_Get_Channel_RawK();
	//940nm
	value[17] = AS7265X_Get_Channel_RawL();
}

/***************************************************************************************************************
AS7265X Get Calibrated value
****************************************************************************************************************/
//A: 410nm
float AS7265X_Get_CalA()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72653, AS7265X_REG_CAL_RGA_C0);
}
//B: 435nm
float AS7265X_Get_CalB()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72653, AS7265X_REG_CAL_SHB_C0);
}
//C: 460nm
float AS7265X_Get_CalC()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72653, AS7265X_REG_CAL_TIC_C0);
}
//D: 485nm
float AS7265X_Get_CalD()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72653, AS7265X_REG_CAL_UJD_C0);
}
//E: 510nm
float AS7265X_Get_CalE()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72653, AS7265X_REG_CAL_VKE_C0);
}
//F: 535nm
float AS7265X_Get_CalF()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72653, AS7265X_REG_CAL_WLF_C0);
}
//G: 560nm
float AS7265X_Get_CalG()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72652, AS7265X_REG_CAL_RGA_C0);
}
//H: 585nm
float AS7265X_Get_CalH()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72652, AS7265X_REG_CAL_SHB_C0);
}
//I: 645nm
float AS7265X_Get_CalI()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72652, AS7265X_REG_CAL_TIC_C0);
}
//J: 705nm
float AS7265X_Get_CalJ()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72652, AS7265X_REG_CAL_UJD_C0);
}
//K: 900nm
float AS7265X_Get_CalK()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72652, AS7265X_REG_CAL_VKE_C0);
}
//L: 940nm
float AS7265X_Get_CalL()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72652, AS7265X_REG_CAL_WLF_C0);
}
//R: 610nm
float AS7265X_Get_CalR()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72651, AS7265X_REG_CAL_RGA_C0);
}
//S: 680nm
float AS7265X_Get_CalS()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72651, AS7265X_REG_CAL_SHB_C0);
}
//T: 730nm
float AS7265X_Get_CalT()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72651, AS7265X_REG_CAL_TIC_C0);
}
//U: 760nm
float AS7265X_Get_CalU()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72651, AS7265X_REG_CAL_UJD_C0);
}
//V: 810nm
float AS7265X_Get_CalV()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72651, AS7265X_REG_CAL_VKE_C0);
}
//W: 860nm
float AS7265X_Get_CalW()
{
	return AS7265X_Read_Calibrated(AS7265X_SELECT_AS72651, AS7265X_REG_CAL_WLF_C0);
}

//410nm,4350nm,460nm,485nm,510nm,535nm,560nm,585nm,610nm,645nm,680nm,705nm,730nm,760nm,810nm,860nm,900nm,940nm
void AS7265X_Get_Calibrated(float* value)
{
	//410nm
	value[0] = AS7265X_Get_CalA();
	//4350nm
	value[1] = AS7265X_Get_CalB();
	//460nm
	value[2] = AS7265X_Get_CalC();
	//485nm
	value[3] = AS7265X_Get_CalD();
	//510nm
	value[4] = AS7265X_Get_CalE();
	//535nm
	value[5] = AS7265X_Get_CalF();
	//560nm
	value[6] = AS7265X_Get_CalG();
	//585nm
	value[7] = AS7265X_Get_CalH();
	//610nm
	value[8] = AS7265X_Get_CalR();
	//645nm
	value[9] = AS7265X_Get_CalI();
	//680nm
	value[10] = AS7265X_Get_CalS();
	//705nm
	value[11] = AS7265X_Get_CalJ();
	//730nm
	value[12] = AS7265X_Get_CalT();
	//760nm
	value[13] = AS7265X_Get_CalU();
	//810nm
	value[14] = AS7265X_Get_CalV();
	//860nm
	value[15] = AS7265X_Get_CalW();
	//900nm
	value[16] = AS7265X_Get_CalK();
	//940nm
	value[17] = AS7265X_Get_CalL();
}
/***************************************************************************************************************
AS7265X Initialization
****************************************************************************************************************/
bool AS7265X_Init(void)
{
	if (g_as7265x_fd < 0)
		g_as7265x_fd = open(AS7265X_I2C_DEV, O_RDWR);
	if (g_as7265x_fd < 0)
	{
		perror("AS7265X open i2c device failed");
		return false;
	}
	if (AS7265X_Get_ID() == false)
		return false;

	return true;
}
