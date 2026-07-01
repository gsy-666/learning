/**
 * @file DM6220.c
 * @brief DM6220伺服电机驱动实现文件
 * @details 实现DM6220电机的CAN通信控制，包括位置模式、速度模式、MIT模式等
 */

#include "DM6220.h"

/**
 * @brief DM6220电机全局结构体
 * @details 存储电机的CAN帧信息和状态数据
 */
DM_Motor_Info_Typedef DM_6220_Motor = { //6220电机结构体
 
	 .CANFrameInfo = {
		.CAN_ID = 0x01,        // 电机CAN ID (1-32)
	  .Master_ID = 0x00,     // 主控CAN ID
	 },
 
};

// ==================== 电机参数限制 ====================
#define P_MAX 12.5f          // 最大位置限制 (圈)
#define V_MAX 45.f          // 最大速度限制 (rad/s)
#define T_MAX 10.f          // 最大扭矩限制 (N·m)

// 全局电机控制结构体
DM_Motor_Control_Typedef DM_Motor_Control;

/**
 * @brief 将整型数据转换为浮点数
 * @param X_int 输入的整型值
 * @param X_min 最小值范围
 * @param X_max 最大值范围
 * @param Bits 数据位宽
 * @return 转换后的浮点数
 * @note 用于将CAN通信中的定点数转换为实际物理量
 */
static float uint_to_float(int X_int, float X_min, float X_max, int Bits){
    float span = X_max - X_min;
    float offset = X_min;
    return ((float)X_int)*span/((float)((1<<Bits)-1)) + offset;
}

/**
 * @brief 将浮点数转换为整型数据
 * @param X_float 输入的浮点值
 * @param X_min 最小值范围
 * @param X_max 最大值范围
 * @param bits 数据位宽
 * @return 转换后的整型值
 * @note 用于将实际物理量转换为CAN通信中的定点数
 */
static int float_to_uint(float X_float, float X_min, float X_max, int bits){
    float span = X_max - X_min;
    float offset = X_min;
    return (int) ((X_float-offset)*((float)((1<<bits)-1))/span);
}

/**
 * @brief 发送电机控制命令
 * @param TxFrame CAN发送帧结构体
 * @param TxStdId CAN标准ID
 * @param CMD 命令类型 (使能/禁用/保存零位)
 * @details 支持的命令:
 *          - Motor_Enable: 使能电机
 *          - Motor_Disable: 禁用电机
 *          - Motor_Save_Zero_Position: 保存当前位置为零位
 */
void DM_Motor_Command(FDCAN_TxFrame_TypeDef *TxFrame,uint16_t TxStdId,uint8_t CMD){
	// 添加调试信息
	// HAL_UART_Transmit(&huart1, (uint8_t*)"#DM_CMD Start$", 14, 100);

	 TxFrame->Header.Identifier = TxStdId;
  	
	 TxFrame->Data[0] = 0xFF;
   TxFrame->Data[1] = 0xFF;
 	 TxFrame->Data[2] = 0xFF;
 	 TxFrame->Data[3] = 0xFF;
 	 TxFrame->Data[4] = 0xFF;
 	 TxFrame->Data[5] = 0xFF;
 	 TxFrame->Data[6] = 0xFF;
	
	 switch(CMD){
		 
		  case Motor_Enable :        // 使能电机命令 (0xFC)
	        TxFrame->Data[7] = 0xFC; 
	    break;
      
			case Motor_Disable :       // 禁用电机命令 (0xFD)
	        TxFrame->Data[7] = 0xFD; 
      break;
      
			case Motor_Save_Zero_Position :  // 保存零位命令 (0xFE)
	        TxFrame->Data[7] = 0xFE; 
			break;
			
			default:
	    break;   
	}
	
	// 添加发送状态检查
	// HAL_UART_Transmit(&huart1, (uint8_t*)"#Before Send$", 14, 100);
	HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(TxFrame->hcan,&TxFrame->Header,TxFrame->Data);
	// HAL_UART_Transmit(&huart1, (uint8_t*)"#After Send$", 13, 100);
	
	if(status != HAL_OK) {
		// 发送失败
		// HAL_UART_Transmit(&huart1, (uint8_t*)"#CAN Send Failed$", 17, 100);
	} else {
		// 发送成功
		// HAL_UART_Transmit(&huart1, (uint8_t*)"#CAN Send OK$", 14, 100);
	}

}

/**
 * @brief 发送电机CAN控制帧
 * @param TxFrame CAN发送帧结构体
 * @param DM_Motor 电机信息结构体
 * @param Mode 控制模式
 * @param Postion 目标位置 (圈)
 * @param Velocity 目标速度 (rad/s)
 * @param KP 位置环P增益
 * @param KD 位置环D增益
 * @param Torque 目标扭矩 (N·m)
 * @details 支持三种控制模式:
 *          - MIT_Mode: 力矩模式，同时控制位置、速度、扭矩
 *          - Position_Velocity_Mode: 位置速度模式
 *          - Velocity_Mode: 纯速度模式
 */
void DM_Motor_CAN_TxMessage(FDCAN_TxFrame_TypeDef *TxFrame,DM_Motor_Info_Typedef *DM_Motor,uint8_t Mode,float Postion, float Velocity, float KP, float KD, float Torque){

	 if(Mode > Velocity_Mode) Mode = MIT_Mode;	// 默认使用MIT模式
		 
	 // ==================== MIT模式 (力矩控制模式) ====================
	 if(Mode == MIT_Mode) {
		
			 uint16_t Postion_Tmp,Velocity_Tmp,Torque_Tmp,KP_Tmp,KD_Tmp;
			 
			 // 将浮点数转换为定点数 (CAN协议规定的格式)
			 Postion_Tmp  =  float_to_uint(Postion,-P_MAX,P_MAX,16) ;    // 位置: 16位, 范围±12.5圈
			 Velocity_Tmp =  float_to_uint(Velocity,-V_MAX,V_MAX,12);    // 速度: 12位, 范围±45 rad/s
			 Torque_Tmp = float_to_uint(Torque,-T_MAX,T_MAX,12);        // 扭矩: 12位, 范围±10 N·m
			 KP_Tmp = float_to_uint(KP,0,500,12);                       // KP: 12位, 范围0-500
			 KD_Tmp = float_to_uint(KD,0,5,12);                         // KD: 12位, 范围0-5

			 TxFrame->Header.Identifier = DM_Motor->CANFrameInfo.CAN_ID;  // 使用电机ID
			 
			 // 打包8字节数据 (符合DM6220 MIT协议)
			 TxFrame->Data[0] = (uint8_t)(Postion_Tmp>>8);               // 位置高8位
			 TxFrame->Data[1] = (uint8_t)(Postion_Tmp);                 // 位置低8位
			 TxFrame->Data[2] = (uint8_t)(Velocity_Tmp>>4);             // 速度高8位 (低4位保留)
			 TxFrame->Data[3] = (uint8_t)((Velocity_Tmp&0x0F)<<4) | (uint8_t)(KP_Tmp>>8); // 速度低4位 + KP高4位
			 TxFrame->Data[4] = (uint8_t)(KP_Tmp);                       // KP低8位
			 TxFrame->Data[5] = (uint8_t)(KD_Tmp>>4);                   // KD高8位 (低4位保留)
			 TxFrame->Data[6] = (uint8_t)((KD_Tmp&0x0F)<<4) | (uint8_t)(Torque_Tmp>>8); // KD低4位 + 扭矩高4位
			 TxFrame->Data[7] = (uint8_t)(Torque_Tmp);                   // 扭矩低8位
			
			 HAL_FDCAN_AddMessageToTxFifoQ(TxFrame->hcan,&TxFrame->Header,TxFrame->Data);
	 
	 // ==================== 位置速度模式 ====================
	 }else if(Mode == Position_Velocity_Mode){
	 
		   KP = 0; KD = 0; Torque = 0;  // 此模式下忽略这些参数
		 
       uint8_t *Postion_Tmp,*Velocity_Tmp;
		   
		   // 直接将float的4字节复制到CAN数据
		   Postion_Tmp = (uint8_t *)&Postion; 
		   Velocity_Tmp = (uint8_t *)&Velocity; 
		 
	     TxFrame->Header.Identifier = DM_Motor->CANFrameInfo.CAN_ID + 0x100;  // ID偏移0x100表示位置速度模式
			 
			 TxFrame->Data[0] = *(Postion_Tmp);
			 TxFrame->Data[1] = *(Postion_Tmp + 1);
			 TxFrame->Data[2] = *(Postion_Tmp + 2);
			 TxFrame->Data[3] = *(Postion_Tmp + 3);
			 TxFrame->Data[4] = *(Velocity_Tmp);
			 TxFrame->Data[5] = *(Velocity_Tmp + 1);
			 TxFrame->Data[6] = *(Velocity_Tmp + 2);
			 TxFrame->Data[7] = *(Velocity_Tmp + 3);
			
			 HAL_FDCAN_AddMessageToTxFifoQ(TxFrame->hcan,&TxFrame->Header,TxFrame->Data);
	 
	 // ==================== 纯速度模式 ====================
	 }else if(Mode == Velocity_Mode){
	 
	     Postion = 0;KP = 0; KD = 0; Torque = 0;  // 此模式下忽略其他参数
		 
       uint8_t *Velocity_Tmp;
		   
		   Velocity_Tmp = (uint8_t *)&Velocity; 
		 
	     TxFrame->Header.Identifier = DM_Motor->CANFrameInfo.CAN_ID + 0x200;  // ID偏移0x200表示速度模式
			 
			 TxFrame->Data[0] = *(Velocity_Tmp);
			 TxFrame->Data[1] = *(Velocity_Tmp + 1);
			 TxFrame->Data[2] = *(Velocity_Tmp + 2);
			 TxFrame->Data[3] = *(Velocity_Tmp + 3);
			 TxFrame->Data[4] = 0;
			 TxFrame->Data[5] = 0;
			 TxFrame->Data[6] = 0;
			 TxFrame->Data[7] = 0;
			
			 HAL_FDCAN_AddMessageToTxFifoQ(TxFrame->hcan,&TxFrame->Header,TxFrame->Data);
	 
	 }
	 
	 
}

/**
 * @brief 解析电机返回的CAN数据
 * @param Data 接收的CAN数据 (8字节)
 * @param DM_Motor 电机信息结构体 (用于存储解析结果)
 * @details 解析电机返回的8字节数据，提取:
 *          - State: 电机状态 (故障/正常等)
 *          - Position: 当前位置 (圈)
 *          - Velocity: 当前速度 (rad/s)
 *          - Torque: 当前扭矩 (N·m)
 *          - Temperature_MOS: MOS管温度 (℃)
 *          - Temperature_Rotor: 转子温度 (℃)
 */
void DM_Motor_Info_Update(uint8_t *Data,DM_Motor_Info_Typedef *DM_Motor)
{
		
	  DM_Motor->Data.State = Data[0]>>4;  // 高4位为状态位
		// 解析位置 (16位, 范围±12.5圈)
		DM_Motor->Data.P_int = ((uint16_t)(Data[1]) <<8) | ((uint16_t)(Data[2]));
		// 解析速度 (12位, 范围±45 rad/s)
		DM_Motor->Data.V_int = ((uint16_t)(Data[3]) <<4) | ((uint16_t)(Data[4])>>4);
		// 解析扭矩 (12位, 范围±10 N·m)
		DM_Motor->Data.T_int = ((uint16_t)(Data[4]&0xF) <<8) | ((uint16_t)(Data[5]));
		
		// 转换为实际物理量
		DM_Motor->Data.Torque=  uint_to_float(DM_Motor->Data.T_int,-T_MAX,T_MAX,12);
		DM_Motor->Data.Position=uint_to_float(DM_Motor->Data.P_int,-P_MAX,P_MAX,16);
    DM_Motor->Data.Velocity=uint_to_float(DM_Motor->Data.V_int,-V_MAX,V_MAX,12);
    // 温度直接为数值, 无需转换
    DM_Motor->Data.Temperature_MOS   = (float)(Data[6]);    // MOS管温度
		DM_Motor->Data.Temperature_Rotor = (float)(Data[7]);   // 转子温度


}
	 





