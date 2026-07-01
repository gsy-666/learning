#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>

#define SGP30_I2C_ADDR 0x58 // SGP30 的固定 I2C 地址

// SGP30 常用命令
#define SGP30_CMD_INIT_AIR_QUALITY    0x2003
#define SGP30_CMD_MEASURE_AIR_QUALITY 0x2008

// CRC-8 校验函数 (根据数据手册提供的多项式 0x31 编写)
uint8_t check_crc(uint8_t data[], uint8_t count, uint8_t checksum) {
    uint8_t crc = 0xFF;
    uint8_t i, j;
    for (i = 0; i < count; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return (crc == checksum);
}

// 向 SGP30 发送 16 位命令
int sgp30_send_cmd(int fd, uint16_t cmd) {
    uint8_t buf[2];
    buf[0] = (cmd >> 8) & 0xFF; // 命令高字节
    buf[1] = cmd & 0xFF;        // 命令低字节
    if (write(fd, buf, 2) != 2) {
        perror("发送命令失败");
        return -1;
    }
    return 0;
}

int main(void) {
    int fd;
    // 注意：这里需要替换为你实际连接的 I2C 节点，比如 /dev/i2c-1, /dev/i2c-3 等
    char *filename = "/dev/i2c-3"; 
    
    // 1. 打开 I2C 设备文件
    if ((fd = open(filename, O_RDWR)) < 0) {
        printf("无法打开 I2C 总线 %s\n", filename);
        return 1;
    }

    // 2. 设置 I2C 从机地址为 SGP30 的地址
    if (ioctl(fd, I2C_SLAVE, SGP30_I2C_ADDR) < 0) {
        printf("无法设置 I2C 从机地址\n");
        return 1;
    }

    printf("SGP30 I2C 连通成功！\n");

    // 3. 发送初始化命令 (Init_air_quality)
    if (sgp30_send_cmd(fd, SGP30_CMD_INIT_AIR_QUALITY) < 0) {
        return 1;
    }
    printf("发送初始化命令成功，SGP30 开始预热 (前15秒数据固定)...\n");

    // 4. 进入严格的 1Hz 读取循环
    int count = 0;
    while (1) {
        // 4.1 发送测量命令
        if (sgp30_send_cmd(fd, SGP30_CMD_MEASURE_AIR_QUALITY) < 0) {
            continue;
        }

        // 4.2 延时等待传感器测量完成 (数据手册规定最大 12ms，这里给 15ms 更稳妥)
        usleep(15000); 

        // 4.3 读取 6 个字节的数据
        // [CO2_MSB, CO2_LSB, CRC_CO2, TVOC_MSB, TVOC_LSB, CRC_TVOC]
        uint8_t data[6] = {0};
        if (read(fd, data, 6) != 6) {
            printf("读取数据失败\n");
        } else {
            // 4.4 校验 CO2 和 TVOC 的 CRC
            int co2_crc_ok = check_crc(&data[0], 2, data[2]);
            int tvoc_crc_ok = check_crc(&data[3], 2, data[5]);

            if (co2_crc_ok && tvoc_crc_ok) {
                uint16_t co2_eq = (data[0] << 8) | data[1];
                uint16_t tvoc = (data[3] << 8) | data[4];

                count++;
                
                // 严格通过前 15 次循环来判断预热期 (15秒)
                if (count <= 15) {
                    printf("[%03d] 传感器预热中... (固定输出 -> CO2: %d ppm, TVOC: %d ppb)\n", count, co2_eq, tvoc);
                } else {
                    // 15秒后，不管数值是多少，都是真实测量的结果
                    if (co2_eq == 400 && tvoc == 0) {
                        printf("[%03d] 测量成功 -> CO2: %d ppm, TVOC: %d ppb (空气质量极佳！)\n", count, co2_eq, tvoc);
                    } else {
                        printf("[%03d] 测量成功 -> CO2: %d ppm, TVOC: %d ppb\n", count, co2_eq, tvoc);
                    }
                }
            } else {
                printf("警告：CRC 校验失败！这条数据存在干扰，已丢弃。\n");
            }
        }

        // 4.5 严格遵守 1Hz 的测量频率 (剩下的时间睡大约 1 秒 - 15ms)
        usleep(985000); 
    }

    close(fd);
    return 0;
}