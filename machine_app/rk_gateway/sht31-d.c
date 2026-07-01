// Minimal SHT31-D implementation, adapted from liangcang/project/humidity_scale/sht31-d.c

#include "sht31-d.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static void delay_ms(unsigned int howLong) {
  struct timespec sleeper, dummy;
  sleeper.tv_sec = (time_t)(howLong / 1000);
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000;
  nanosleep(&sleeper, &dummy);
}

void delay(unsigned int howLong) { delay_ms(howLong); }

uint8_t crc8(const uint8_t *data, int len) {
  const uint8_t POLYNOMIAL = 0x31;
  uint8_t crc = 0xFF;
  for (int j = len; j; --j) {
    crc ^= *data++;
    for (int i = 8; i; --i) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ POLYNOMIAL) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

int sht31_open(int i2c_address, uint8_t sht31_address) {
  char filename[32];
  snprintf(filename, sizeof(filename), "/dev/i2c-%d", i2c_address);
  int fp = open(filename, O_RDWR);
  if (fp < 0) return fp;

  if (ioctl(fp, I2C_SLAVE, sht31_address) < 0) {
    close(fp);
    return -1;
  }

#ifdef SHT31D_FIX_INITIAL_FAIL
  uint8_t buf = 0x00;
  (void)write(fp, &buf, 1);
  (void)read(fp, &buf, 1);
#endif

  return fp;
}

int sht31_close(int fp) { return close(fp); }

sht31rtn writeandread(int fd, uint16_t sndword, uint8_t *buffer, int readsize) {
  uint8_t cmd[2];
  cmd[0] = (uint8_t)((sndword >> 8) & 0xFF);
  cmd[1] = (uint8_t)(sndword & 0xFF);

  if (write(fd, cmd, 2) != 2) return SHT31_WRITE_FAILED;

  if (readsize <= 0) return SHT31_OK;

  // datasheet: up to ~15ms for measurement; keep it simple.
  usleep(15000);

  int r = read(fd, buffer, (size_t)readsize);
  if (r != readsize) return SHT31_READ_FAILED;

  return SHT31_OK;
}

static int check_word_crc(uint8_t msb, uint8_t lsb, uint8_t crc) {
  uint8_t data[2] = {msb, lsb};
  return crc8(data, 2) == crc;
}

sht31rtn gettempandhumidity(int file, float *temp, float *hum) {
#if SHT31_USE_TEST_LOOP_STYLE
  // Match liangcang/project/humidity_scale/test.c behavior: keep fd open and read once per second.
#endif
  uint8_t buf[6];
  sht31rtn rtn = writeandread(file, SHT32_DEFAULT_READ, buf, 6);
  if (rtn != SHT31_OK) return rtn;

  if (!check_word_crc(buf[0], buf[1], buf[2]) || !check_word_crc(buf[3], buf[4], buf[5])) {
    // still compute but report CRC fail
    uint16_t rawT = (uint16_t)((buf[0] << 8) | buf[1]);
    uint16_t rawH = (uint16_t)((buf[3] << 8) | buf[4]);
    *temp = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    *hum = 100.0f * ((float)rawH / 65535.0f);
    return SHT31_CRC_CHECK_FAILED;
  }

  uint16_t rawT = (uint16_t)((buf[0] << 8) | buf[1]);
  uint16_t rawH = (uint16_t)((buf[3] << 8) | buf[4]);

  *temp = -45.0f + 175.0f * ((float)rawT / 65535.0f);
  *hum = 100.0f * ((float)rawH / 65535.0f);
  return SHT31_OK;
}

sht31rtn getstatus(int file, uint16_t *rtnbuf) {
  uint8_t buf[3];
  sht31rtn rtn = writeandread(file, SHT31_READSTATUS, buf, 3);
  if (rtn != SHT31_OK) return rtn;

  if (!check_word_crc(buf[0], buf[1], buf[2])) {
    *rtnbuf = (uint16_t)((buf[0] << 8) | buf[1]);
    return SHT31_CRC_CHECK_FAILED;
  }

  *rtnbuf = (uint16_t)((buf[0] << 8) | buf[1]);
  return SHT31_OK;
}

sht31rtn getserialnum(int file, uint32_t *serialNo) {
  uint8_t buf[6];
  sht31rtn rtn = writeandread(file, SHT31_READ_SERIALNO, buf, 6);
  if (rtn != SHT31_OK) return rtn;

  // skip CRC checks for brevity
  *serialNo = (uint32_t)((buf[0] << 24) | (buf[1] << 16) | (buf[3] << 8) | buf[4]);
  return SHT31_OK;
}

sht31rtn clearstatus(int file) { return writeandread(file, SHT31_CLEARSTATUS, NULL, 0); }

sht31rtn softreset(int file) { return writeandread(file, SHT31_SOFTRESET, NULL, 0); }

sht31rtn enableheater(int file) { return writeandread(file, SHT31_HEATER_ENABLE, NULL, 0); }

sht31rtn disableheater(int file) { return writeandread(file, SHT31_HEATER_DISABLE, NULL, 0); }
