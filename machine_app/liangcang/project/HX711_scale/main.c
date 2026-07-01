#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include "HX711_scale.h" // 确保名字和你的头文件一致

extern void close_hx711(void);

// --- 中值滤波相关变量 ---
#define MEDIAN_LEN  5       // 中值滤波的滤波长度,一般取奇数
#define MEDIAN      2       // 中值在滤波数组中的位置 (5个数据，中间那个索引是 2)
uint32_t buffer[MEDIAN_LEN]; // 中值滤波的数据缓存
int medleng = 0;            // 一组中值滤波数据中,进入滤波缓存的数据个数
uint32_t xd;                // 数据对比大小中间变量

// --- 重量计算相关变量 ---
uint32_t weight = 0;        // 最终算出的重量值 (克)
uint32_t pi_weight = 0;     // 皮重
uint32_t hx711_xishu = 24578; // 修正系数

// 捕获 Ctrl+C 信号，确保优雅退出
void handle_sigint(int sig) {
    printf("\n\n程序退出，已释放 GPIO 资源。\n");
    exit(0); 
}

// 获取皮重 (带中值滤波)
void Get_Tare(void)
{
    uint32_t hx711_dat;
    int i, j; // 修复 Bug：增加内层循环变量 j
    
    medleng = 0;
    for(i = 0; i < MEDIAN_LEN; i++)
    {
        hx711_dat = Read_HX711();    // 采集数据
        
        if(medleng == 0)             // 缓存的第1个元素,直接放入
        { 
            buffer[0] = hx711_dat; 
            medleng = 1; 
        }
        else                         // 插入排序算法
        {  
            for(j = 0; j < medleng; j++) // 注意：这里必须用 j，不能再用 i
            {
                if(buffer[j] > hx711_dat) 
                { 
                    xd = hx711_dat; 
                    hx711_dat = buffer[j]; 
                    buffer[j] = xd;
                }
            }
            buffer[medleng] = hx711_dat; 
            medleng++;
        }        
        
        // 延时 100ms (匹配 HX711 默认 10Hz 输出速率)
        usleep(100000); 
    }
    
    // 循环结束后，取中值作为最终基础数据
    hx711_dat = buffer[MEDIAN];    
    pi_weight = (uint32_t)(hx711_dat * 0.01);
}

// 获取被测物体重量
void Get_Weight(void)
{
    uint32_t hx711_data;
    uint32_t get, aa;  
    
    hx711_data = Read_HX711();          // 采集数据
    get = (uint32_t)(hx711_data * 0.01); // 数据缩小 100 倍
    
    if(get > pi_weight)
    {
        // 优化：直接使用本次采集的 get，去掉冗余的第二次 Read_HX711()
        aa = get - pi_weight;           // 减去皮重
        
        // 保留你原来的校准公式
        weight = (uint32_t)((float)aa * 0.00001 * hx711_xishu); 
    }
    else
    {
        weight = 0;
    }
}

int main(void)
{       
    // 1. 注册信号和初始化硬件
    signal(SIGINT, handle_sigint);

    printf("--- HX711 高精度电子秤测试 ---\n");
    if (init_hx711_gpio() < 0) {
        printf("初始化 GPIO 失败！\n");
        return -1;
    }
    atexit(close_hx711); // 注册程序退出时的关门函数

    // 2. 开机预热与去皮
    printf("传感器预热中...\n");
    Read_HX711(); usleep(100000);
    Read_HX711(); usleep(100000); // 丢弃前几组不稳定数据
    
    printf("请清空秤台，正在获取皮重...\n");
    Get_Tare();
    printf("去皮完成！当前皮重基数: %u\n", pi_weight);
    printf("====================================\n\n");

    // 3. 循环测重并使用主循环的滤波
    medleng = 0;
    while(1)
    {       
        Get_Weight(); // 读取并算出当前这一刻的 weight
        
        // --- 针对算出的重量再进行一次中值滤波 ---
        if(medleng == 0) 
        { 
            buffer[0] = weight; 
            medleng = 1; 
        }
        else 
        {  
            for(int j = 0; j < medleng; j++) // 同样修复了这里的 i/j 问题
            {
                if(buffer[j] > weight) 
                { 
                    xd = weight; 
                    weight = buffer[j]; 
                    buffer[j] = xd;
                }
            }
            buffer[medleng] = weight; 
            medleng++;
        }       
        
        if(medleng >= MEDIAN_LEN) 
        {
            // 拿到过滤后的最终稳定重量
            uint32_t final_weight = buffer[MEDIAN];    
            medleng = 0; 
            
            // 终端动态刷新显示效果 (\r 让光标回到行首覆盖打印)
            printf("\r实测重量: %4u g  ", final_weight);
            fflush(stdout); // 强制立刻输出到终端
        }
        
        usleep(20000); // 主循环微微延时防 CPU 跑满，凑够 5 次正好更新一次屏幕
    }
    
    return 0;
}