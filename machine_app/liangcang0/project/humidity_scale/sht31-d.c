/*
 *
 * https://www.kernel.org/doc/Documentation/i2c/dev-interface
 * https://github.com/adafruit/Adafruit_SHT31
 * https://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/Humidity_and_Temperature_Sensors/Sensirion_Humidity_and_Temperature_Sensors_SHT3x_Datasheet_digital.pdf
 *
 * 本代码依赖 i2c 设备库
 * sudo apt-get install libi2c-dev
 *
 * 另外也建议安装下面这个工具，但要注意：其中的 i2cdump 可能导致 sht31 接口不稳定，
 * 并且需要硬复位才能正确恢复。
 * sudo apt-get install i2c-tools
 *
 * 在树莓派上，请确认 /boot/config.txt 中包含以下 2 行：
 * dtparam=i2c_arm=on
 * dtparam=i2c1_baudrate=10000
 * 我知道这会把波特率从理论最优值降低，但在我的测试中这是最稳定的设置。
 * 若要使用最大设置，可在上面波特率后再加一个 0，例如：dtparam=i2c1_baudrate=100000
 */
 
#include <stdint.h>
#include <linux/i2c-dev.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <elf.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "sht31-d.h"


/*
 * delay:
 *	等待指定毫秒数
 *********************************************************************************
 */

void delay (unsigned int howLong)
{
  struct timespec sleeper, dummy ;

  sleeper.tv_sec  = (time_t)(howLong / 1000) ;
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;

  nanosleep (&sleeper, &dummy) ;
}

/*
*
* CRC-8 公式来自 SHT 规格书 PDF 第 14 页
*
* 测试数据 0xBE、0xEF 的计算结果应为 0x92
*
* 初始值 0xFF
* 多项式 0x31 (x8 + x5 +x4 +1)
* 最终异或值 0x00
*/
uint8_t crc8(const uint8_t *data, int len)
{
  const uint8_t POLYNOMIAL = 0x31;
  uint8_t crc = 0xFF;
  int j;
  int i;
  
  for (j = len; j; --j ) {
    crc ^= *data++;

    for ( i = 8; i; --i ) {
      crc = ( crc & 0x80 )
            ? (crc << 1) ^ POLYNOMIAL
            : (crc << 1);
    }
  }
  return crc;
}

/*
 * sht31_open:
 *	打开与对应 I2C 总线及 sht31 地址关联的文件句柄
 *********************************************************************************
 */
int sht31_open(int i2c_address, uint8_t sht31_address)
{
  char filename[20];
  int fp;
  
  snprintf(filename, 19, "/dev/i2c-%d", i2c_address);
  fp = open(filename, O_RDWR);
  if (fp < 0) {
    return fp;
  }

  if (ioctl(fp, I2C_SLAVE, sht31_address) < 0) {
    close(fp);
    return -1;
  }
  
  // 先做一次几乎无意义的初始读写。
  // 这部分仍在调试；若省略，第一条下发命令会失败，而这两次操作本身通常也会失败
#ifdef SHT31D_FIX_INITIAL_FAIL
  uint8_t buf = 0x00;
  if (write(fp, &buf, 1) != 1) {}
  if ( read(fp, &buf, 1) != 1) {} 
#endif
  
  return fp;
}

int sht31_close(int fp)
{
  return close(fp);
}

/*
 * writeandread:
 *	写入 I2C 命令并读取返回值。若只写不读，请将 readsize 设为 0
 *********************************************************************************
 */

sht31rtn writeandread(int fp, uint16_t sndword, uint8_t *buffer, int readsize)
{
  int rtn;
  int sendsize = 2;
  uint8_t snd[sendsize];
  
  // 大端序：把 16 位数据拆分为两个按高字节在前顺序的 8 位数据。
  snd[0]=(sndword >> 8) & 0xff;
  snd[1]=sndword & 0xff;

  rtn = write(fp, snd, sendsize);
  if ( rtn != sendsize ) {
    //printf("ERROR sending command %d :- %s\n",rtn, strerror (errno));
    return SHT31_WRITE_FAILED;
  } 

  if (readsize > 0) {
    delay(10);
    rtn = read(fp, buffer, readsize);
    if ( rtn < readsize) {
      return SHT31_READ_FAILED;
    }
  }
  
  return SHT31_OK;
}

/*
 * getserialnum:
 *	获取 sht31 的序列号
 *********************************************************************************
 */
sht31rtn getserialnum(int file, uint32_t *serialNo)
{
  uint8_t buf[10];
  int rtn;

  rtn = writeandread(file, SHT31_READ_SERIALNO, buf, 6);
  if (rtn != SHT31_OK)
    return rtn;
  else {
    *serialNo = ((uint32_t)buf[0] << 24)
              | ((uint32_t)buf[1] << 16)
              | ((uint32_t)buf[3] << 8)
              | (uint32_t)buf[4];
    if (buf[2] != crc8(buf, 2) || buf[5] != crc8(buf+3, 2))
      return SHT31_CRC_CHECK_FAILED;
  }
  
  return SHT31_OK;
}

/*
 * getserialnum:
 *	获取温度和湿度数值
 *********************************************************************************
 */
sht31rtn gettempandhumidity(int file, float *temp, float *hum)
{
  uint8_t buf[10];
  int rtn;
  
  rtn = writeandread(file, SHT32_DEFAULT_READ, buf, 6);
  
  if (rtn != SHT31_OK)
    return rtn;
  else {
    uint16_t ST, SRH;
    ST = buf[0];
    ST <<= 8;
    ST |= buf[1];
    
    SRH = buf[3];
    SRH <<= 8;
    SRH |= buf[4];

    *temp = -45.0 + (175.0 * ((float) ST / (float) 0xFFFF));
    *hum = 100.0 * ((float) SRH / (float) 0xFFFF);
 
    if ( buf[2] != crc8(buf, 2) || buf[5] != crc8(buf+3, 2))
      return SHT31_CRC_CHECK_FAILED;
  }
  
  return SHT31_OK;
}

/*
 * getserialnum:
 *	获取状态寄存器值
 *********************************************************************************
 */
sht31rtn getstatus(int file, uint16_t *rtnbuf)
{
  uint8_t buf[10];
  int rtn = writeandread(file, SHT31_READSTATUS, buf, 3);
 
  if (rtn != SHT31_OK)
    return rtn;
  else { 
    *rtnbuf = buf[0];
    *rtnbuf <<= 8;
    *rtnbuf |= buf[1];
   
    if ( buf[2] != crc8(buf, 2) )
      return SHT31_CRC_CHECK_FAILED;
  }
  
  return SHT31_OK;
}

/*
 * clearstatus:
 *	清除 sht31 的所有状态。
 *********************************************************************************
 */
sht31rtn clearstatus(int file)
{
  if( writeandread(file, SHT31_CLEARSTATUS, NULL, 0) != 0)
    return SHT31_BAD;
  else 
    return SHT31_OK;
}

/*
 * softreset:
 *	重置 sht31。
 *********************************************************************************
 */
sht31rtn softreset(int file)
{
  if( writeandread(file, SHT31_SOFTRESET, NULL, 0) != 0)
    return SHT31_BAD;
  else 
    return SHT31_OK;
}

/*
 * enableheater:
 *	开启加热器。
 *********************************************************************************
 */
sht31rtn enableheater(int file)
{
  if( writeandread(file, SHT31_HEATER_ENABLE, NULL, 0) != 0)
    return SHT31_BAD;
  else 
    return SHT31_OK;
}

/*
 * disableheater:
 *	关闭加热器。
 *********************************************************************************
 */
sht31rtn disableheater(int file)
{
  if( writeandread(file, SHT31_HEATER_DISABLE, NULL, 0) != 0)
    return SHT31_BAD;
  else 
    return SHT31_OK;
}



