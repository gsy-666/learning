#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include "HX711_scale.h" // 确保名字和你的头文件一致

extern void close_hx711(void);

// --- PWM 相关配置 ---
#define PWM_CHIP_PATH      "/sys/class/pwm/pwmchip3"
#define PWM_CHANNEL        0
#define PWM_PERIOD_NS      20000000
#define PWM_DUTY_NORMAL    500000
#define PWM_DUTY_ACTIVE    1000000
#define PWM_TOLERANCE_G    2

static uint32_t target_weight = 0;
static int pwm_current_duty = -1;

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

static int write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        perror(path);
        return -1;
    }

    if (fputs(text, fp) == EOF) {
        perror(path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int write_uint(const char *path, unsigned int value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", value);
    return write_text(path, buf);
}

static int pwm_init(void)
{
    char path[128];

    if (access("/sys/class/pwm/pwmchip3/pwm0", F_OK) != 0) {
        snprintf(path, sizeof(path), "%s/export", PWM_CHIP_PATH);
        if (write_uint(path, PWM_CHANNEL) < 0) {
            return -1;
        }
        usleep(100000);
    }

    snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, PWM_CHANNEL);
    if (write_uint(path, 0) < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/period", PWM_CHIP_PATH, PWM_CHANNEL);
    if (write_uint(path, PWM_PERIOD_NS) < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/polarity", PWM_CHIP_PATH, PWM_CHANNEL);
    if (write_text(path, "normal") < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/duty_cycle", PWM_CHIP_PATH, PWM_CHANNEL);
    if (write_uint(path, PWM_DUTY_NORMAL) < 0) {
        return -1;
    }

    if (write_uint("/sys/class/pwm/pwmchip3/pwm0/enable", 1) < 0) {
        return -1;
    }

    pwm_current_duty = PWM_DUTY_NORMAL;
    return 0;
}

static int pwm_set_duty(unsigned int duty_ns)
{
    char path[128];

    if (pwm_current_duty == (int)duty_ns) {
        return 0;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, PWM_CHANNEL);
    if (write_uint(path, 0) < 0) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s/pwm%d/duty_cycle", PWM_CHIP_PATH, PWM_CHANNEL);
    if (write_uint(path, duty_ns) < 0) {
        return -1;
    }

    if (write_uint("/sys/class/pwm/pwmchip3/pwm0/enable", 1) < 0) {
        return -1;
    }

    pwm_current_duty = (int)duty_ns;
    return 0;
}

static void pwm_cleanup(void)
{
    char path[128];

    snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, PWM_CHANNEL);
    write_uint(path, 0);

    if (access("/sys/class/pwm/pwmchip3/pwm0", F_OK) == 0) {
        snprintf(path, sizeof(path), "%s/unexport", PWM_CHIP_PATH);
        write_uint(path, PWM_CHANNEL);
    }

    pwm_current_duty = -1;
}

// 捕获 Ctrl+C 信号，确保优雅退出
void handle_sigint(int sig) {
    (void)sig;
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
    atexit(pwm_cleanup);

    if (pwm_init() < 0) {
        printf("PWM 初始化失败！请确认 pwmchip3/pwm0 已经可用。\n");
        return -1;
    }

    // 2. 开机预热与去皮
    printf("传感器预热中...\n");
    Read_HX711(); usleep(100000);
    Read_HX711(); usleep(100000); // 丢弃前几组不稳定数据
    
    printf("请清空秤台，正在获取皮重...\n");
    Get_Tare();
    printf("去皮完成！当前皮重基数: %u\n", pi_weight);
    printf("请输入目标重量(g): ");
    fflush(stdout);
    if (scanf("%u", &target_weight) != 1) {
        printf("输入失败！\n");
        return -1;
    }

    if (pwm_set_duty(PWM_DUTY_ACTIVE) < 0) {
        printf("PWM 占空比设置失败！\n");
        return -1;
    }

    printf("目标重量: %u g, PWM 已切到 %u ns\n", target_weight, PWM_DUTY_ACTIVE);
    printf("====================================\n\n");

    // 3. 循环测重：启动一次，达到目标后关闭一次
    medleng = 0;
    int done = 0;
    while(1)
    {
        if (done) {
            usleep(20000);
            continue;
        }

        Get_Weight(); // 读取并算出当前这一刻的 weight

        // --- 针对算出的重量再进行一次中值滤波 ---
        if(medleng == 0)
        {
            buffer[0] = weight;
            medleng = 1;
        }
        else
        {
            for(int j = 0; j < medleng; j++)
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
            uint32_t final_weight = buffer[MEDIAN];
            medleng = 0;

            // 达到目标后，只执行一次关闭
            if (final_weight + PWM_TOLERANCE_G >= target_weight) {
                if (pwm_set_duty(PWM_DUTY_NORMAL) < 0) {
                    printf("PWM 占空比设置失败！\n");
                    return -1;
                }
                printf("\n达到目标重量(%u g >= %u g)，PWM 已切到 %u ns，流程结束。\n", final_weight, target_weight, PWM_DUTY_NORMAL);
                done = 1;
            } else {
                // 还没到目标，保持开启（不反复切换）
                pwm_set_duty(PWM_DUTY_ACTIVE);
                printf("\r实测重量: %4u g  目标: %4u g  PWM: %u   ", final_weight, target_weight, PWM_DUTY_ACTIVE);
                fflush(stdout);
            }
        }

        usleep(20000);
    }
    
    return 0;
}