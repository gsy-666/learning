/**
 * @file DM6220.h
 * @brief DM6220伺服电机驱动头文件
 * @details 定义电机控制相关的枚举、结构体和函数声明
 */

#ifndef DM6220_H
#define DM6220_H

#include "bsp_can.h"
#include "stdbool.h"
#include <stdint.h>

// ==================== 电机控制命令枚举 ====================
/**
 * @brief 电机控制命令类型
 * @details 用于DM_Motor_Command()函数的命令参数
 */
typedef enum{

  Motor_Enable,                /**< @brief 使能电机 (0xFC) */
  Motor_Disable,               /**< @brief 禁用电机 (0xFD) */
  Motor_Save_Zero_Position,   /**< @brief 保存当前位置为零位 (0xFE) */
  DM_Motor_CMD_Type_Num,      /**< @brief 命令类型总数 */

}DM_Motor_CMD_e;

// ==================== 电机控制模式枚举 ====================
/**
 * @brief 电机控制模式类型
 * @details 用于DM_Motor_CAN_TxMessage()函数的模式参数
 */
typedef enum{

  MIT_Mode,                    /**< @brief MIT模式: 力矩控制模式，同时控制位置/速度/扭矩/PD参数 */
  Position_Velocity_Mode,      /**< @brief 位置速度模式: 分别控制位置和速度 */
  Velocity_Mode,               /**< @brief 纯速度模式: 仅控制速度 */
  DM_Motor_Mode_Type_Num,     /**< @brief 模式类型总数 */

}DM_Motor_Mode_e;


// ==================== 电机实时数据结构体 ====================
/**
 * @brief 电机实时数据 (从电机反馈中解析)
 * @details 存储电机当前的状态和物理量
 */
typedef struct 
{
  int16_t  State;               /**< @brief 电机状态 (高4位) */
  uint16_t  P_int;              /**< @brief 位置原始值 (16位定点数) */
  uint16_t  V_int;              /**< @brief 速度原始值 (12位定点数) */
  uint16_t  T_int;              /**< @brief 扭矩原始值 (12位定点数) */
  float  Position;              /**< @brief 实际位置 (圈) */
  float  Velocity;              /**< @brief 实际速度 (rad/s) */
  float  Torque;                /**< @brief 实际扭矩 (N·m) */
  float  Temperature_MOS;       /**< @brief MOS管温度 (℃) */
  float  Temperature_Rotor;     /**< @brief 转子温度 (℃) */
  
}DM_Motor_Data_Typedef;


// ==================== CAN帧信息结构体 ====================
/**
 * @brief CAN通信帧信息
 * @details 存储CAN ID配置
 */
typedef struct
{
  uint32_t Master_ID;           /**< @brief 主控CAN ID (发送方) */
  uint32_t CAN_ID;              /**< @brief 电机CAN ID (接收方, 1-32) */
  
}Motor_CANFrameInfo_typedef;


// ==================== 电机完整信息结构体 ====================
/**
 * @brief 电机完整信息结构体
 * @details 包含CAN通信信息和实时数据
 */
typedef struct
{
	uint16_t ID;                 /**< @brief 电机编号 */
  Motor_CANFrameInfo_typedef CANFrameInfo;  /**< @brief CAN帧信息 */
	DM_Motor_Data_Typedef Data;  /**< @brief 电机实时数据 */
  
}DM_Motor_Info_Typedef;


// ==================== 电机控制参数结构体 ====================
/**
 * @brief 电机控制参数 (用于PID控制)
 * @details 存储位置环和速度环的控制参数
 */
typedef struct
{
	float  KP;                   /**< @brief 位置环P增益 */
	float  KD;                   /**< @brief 位置环D增益 */
	float  Position;             /**< @brief 目标位置 (圈) */
  float  Velocity;             /**< @brief 目标速度 (rad/s) */
  float  Torque;               /**< @brief 目标扭矩 (N·m) */
	
}DM_Motor_Control_Typedef;


// ==================== 外部变量声明 ====================
extern DM_Motor_Info_Typedef DM_6220_Motor;        /**< @brief 电机全局实例 */
extern DM_Motor_Control_Typedef DM_Motor_Control;  /**< @brief 电机控制参数全局实例 */


// ==================== 函数声明 ====================

/**
 * @brief 解析电机返回的CAN数据 (标准8字节格式)
 * @param rxBuf 接收的CAN数据缓冲区 (8字节)
 * @param DM_Motor 电机信息结构体 (用于存储解析结果)
 */
extern void DM_Motor_Info_Update(uint8_t *rxBuf,DM_Motor_Info_Typedef *DM_Motor);

/**
 * @brief 解析电机返回的CAN数据 (扩展格式, 多电机)
 * @param Data 接收的CAN数据缓冲区
 * @param DM_Motor 电机信息结构体
 * @note 用于多电机级联时的数据解析
 */
extern void DM_Motor_Multi_Info_Update(uint8_t *Data,DM_Motor_Info_Typedef *DM_Motor);

/**
 * @brief 发送电机控制命令
 * @param TxFrame CAN发送帧结构体
 * @param TxStdId CAN标准ID
 * @param CMD 命令类型 (Motor_Enable/Motor_Disable/Motor_Save_Zero_Position)
 */
extern void DM_Motor_Command(FDCAN_TxFrame_TypeDef *TxFrame,uint16_t TxStdId,uint8_t CMD);

/**
 * @brief 发送电机CAN控制帧
 * @param TxFrame CAN发送帧结构体
 * @param DM_Motor 电机信息结构体
 * @param Mode 控制模式 (MIT_Mode/Position_Velocity_Mode/Velocity_Mode)
 * @param Postion 目标位置 (圈)
 * @param Velocity 目标速度 (rad/s)
 * @param KP 位置环P增益
 * @param KD 位置环D增益
 * @param Torque 目标扭矩 (N·m)
 */
extern void DM_Motor_CAN_TxMessage(FDCAN_TxFrame_TypeDef *TxFrame,DM_Motor_Info_Typedef *DM_Motor,uint8_t Mode,
	                                             float Postion, float Velocity, float KP, float KD, float Torque);

#endif