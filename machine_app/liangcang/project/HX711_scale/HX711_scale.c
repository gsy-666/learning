#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdint.h>
#include "HX711_scale.h"
// 全局变量保存两个重要文件的句柄 (fd)
int fd_sck = -1;
int fd_dout = -1;

static int export_gpio(int gpio) {
    char path[50];
    int fd;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", gpio);
    if (access(path, F_OK) == 0) {
        return 0; // already exported
    }

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) return -1;

    char buf[12];
    int len = snprintf(buf, sizeof(buf), "%d", gpio);
    if (write(fd, buf, len) != len) {
        close(fd);
        return -1;
    }
    close(fd);

    // wait a moment for sysfs to create gpioN entries
    usleep(100000);
    return 0;
}

// 1. 初始化方向，并“永久”打开 value 文件
int init_hx711_gpio() {
    char path[50];
    int temp_fd;

    if (export_gpio(HX711_SCK) < 0 || export_gpio(HX711_DOUT) < 0) {
        return -1;
    }

    // --- 设置 SCK 为输出 ---
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", HX711_SCK);
    temp_fd = open(path, O_WRONLY);
    if (temp_fd >= 0) { write(temp_fd, "out", 3); close(temp_fd); }

    // --- 设置 DOUT 为输入 ---
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", HX711_DOUT);
    temp_fd = open(path, O_WRONLY);
    if (temp_fd >= 0) { write(temp_fd, "in", 2); close(temp_fd); }

    // --- 打开 SCK 的 value 文件并保存 fd ---
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", HX711_SCK);
    fd_sck = open(path, O_WRONLY);

    // --- 打开 DOUT 的 value 文件并保存 fd ---
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", HX711_DOUT);
    fd_dout = open(path, O_RDONLY);

    if (fd_sck < 0 || fd_dout < 0) return -1;
    return 0; // 初始化成功
}

// 2. 极速写 SCK 函数 (不包含 open/close)
// 参数传 0 或 1，内部转成字符 "0" 或 "1"
void fast_write_sck(int val) {
    if (val == 1) write(fd_sck, "1", 1);
    else          write(fd_sck, "0", 1);
}

// 3. 极速读 DOUT 函数 (不包含 open/close)
int fast_read_dout() {
    char val;
    // 关键：因为文件一直开着，每次读之前必须把“光标”移回文件开头
    lseek(fd_dout, 0, SEEK_SET); 
    read(fd_dout, &val, 1);
    return (val == '1') ? 1 : 0;
}

// 4. 读数据函数
uint32_t Read_HX711(void)
{   
    uint8_t i;
    uint32_t value = 0;
    
    // 初始状态 SCK 拉低
    fast_write_sck(0);
    
    // 极速读取 DOUT，死等变低 (注意这里可以直接判断整形 1 或 0)
    while (fast_read_dout() == 1) {
        // 如果想避免完全死机，可以在这里加个超时计数器
    }
    
    for(i=0; i<24; i++) 
    {
        fast_write_sck(1);
        // 此处不需要延时，Linux 调用的速度刚好产生合适的脉冲宽度
        
        value = value << 1;
        fast_write_sck(0);
        
        if(fast_read_dout() == 1) {
            value |= 0x01;
        }
    }   
    
    // 第 25 个脉冲
    fast_write_sck(1);
    value = value ^ 0x800000; 
    fast_write_sck(0); 
    
    return value;   
}

// 5. 释放资源
void close_hx711() {
    if (fd_sck >= 0) close(fd_sck);
    if (fd_dout >= 0) close(fd_dout);
}
// #include<stdio.h>
// #include<sys/types.h>
// #include<sys/stat.h>
// #include<fcntl.h>
// #include<unistd.h>
// #include<string.h>
// #include "HX711_scalel.h"
// //my通用的linux版本read_gpio和write_gpio函数,通过/sys/class/gpio接口操作GPIO引脚,参数为GPIO编号和要写入的值和读到的值
// int read_gpio(int fd,int gpio_pin){
//     char path[50];
//     snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_pin);
//     fd = open(path, O_WRONLY);
//     if (fd < 0) {
//         perror("open gpio direction");
//         return -1;
//     }
//     if (write(fd, "in", 2) != 2) {
//         perror("write gpio direction");
//         close(fd);
//         return -1;
//     }
//     close(fd);
//     return 1;
// }

// int read_gpio_value(int fd,int gpio_pin,char *value){
//     char path[50];
//     snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_pin);
//     fd = open(path, O_RDONLY);
//     if (fd < 0) {
//         perror("open gpio value");
//         return -1;
//     }
//     if (read(fd, value, 1) != 1) {
//         perror("read gpio value");
//         close(fd);
//         return -1;
//     }
//     printf("value is %c\n", *value);
//     if (*value == '0') {
//         return 0;
//     }
//     if (*value == '1') {
//         return 1;
//     }
//     return -1;
// }//开启value文件读取GPIO值,返回0或1，但没有clear（fd）关闭文件，需要调用者自己关闭文件描述符

// int write_gpio(int fd,int gpio_pin){

//     char path[50];
//     snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_pin);
//     fd = open(path, O_WRONLY);
//     if (fd < 0) {
//         perror("open gpio direction");
//         return -1;
//     }
//     if (write(fd, "out", 3) != 3) {
//         perror("write gpio direction");
//         close(fd);
//         return -1;
//     }
//     close(fd);
//     return 1;
// }
// int write_gpio_value(int fd,int gpio_pin, const char value){
//     char path[50];
//     snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_pin);
//     fd = open(path, O_WRONLY);
//     if (fd < 0) {
//         perror("open gpio value");
//         return -1;
//     }
//     if (write(fd, value, 1) != 1) {
//         perror("write gpio value");
//         close(fd);
//         return -1;
//     }
//     return 0;
// }

// u_int32_t Read_HX711(void)
// {   
//     int fd;
// 	u_int8_t i;
//     char HX711_DATA;
// 	u_int32_t value = 0;
	
// 	/**
// 	数据手册写到，当数据输出管脚 DOUT 为高电平时，表明A/D 转换器还未准备好输出数据，此时串口时
// 	钟输入信号 PD_SCK 应为低电平，所以下面设置引脚状态。
// 	**/
// 	write_gpio(fd,HX711_SCK);
//     write_gpio_value(fd,HX711_SCK, "0"); //初始状态SCK引脚为低电平
// 	read_gpio(fd,HX711_DOUT);
//     read_gpio_value(fd,HX711_DOUT, &HX711_DATA); //初始状态DOUT引脚为高电平，等待变为低电平表示数据准备好
// 	/**
// 	等待DT引脚变为低电平跳出while
// 	**/
// 	while (HX711_DATA == '1') {
// 		read_gpio_value(fd,HX711_DOUT, &HX711_DATA);
// 	}
// 	delay_us(1);
	
// 	/**
// 	当 DOUT 从高电平变低电平后，PD_SCK 应输入 25 至 27 个不等的时钟脉冲
// 	25个时钟脉冲 ---> 通道A 增益128
// 	26个时钟脉冲 ---> 通道B 增益32
// 	27个时钟脉冲 ---> 通道A 增益64
// 	**/
// 	for(i=0; i<24; i++) //24位输出数据从最高位至最低位逐位输出完成
// 	{
//         write_gpio_value(fd,HX711_SCK, "1");
// 		delay_us(1);
//         read_gpio_value(fd,HX711_DOUT, &HX711_DATA);
// 		write_gpio_value(fd,HX711_SCK, "0");
// 		if(HX711_DATA == '0')
// 		{
// 			value = value << 1;
// 			value |= 0x00;
// 		}
// 		if(HX711_DATA == '1')
// 		{
// 			value = value << 1;
// 			value |= 0x01;
// 		}
// 		delay_us(1);
// 	}	
// 	//第 25至 27 个时钟脉冲用来选择下一次 A/D 转换的输入通道和增益
//     write_gpio_value(fd,HX711_SCK, "1");
// 	value = value^0x800000; 
// 	delay_us(1); 
// 	write_gpio_value(fd,HX711_SCK, "0"); 
// 	delay_us(1);  
//     close(fd);
// 	return value; 	
// }
