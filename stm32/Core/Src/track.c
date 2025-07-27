#include "track.h"
#include "gpio.h"
#include "usart.h"
#include "usart_handler.h"
#include "Control.h"

//#include "motor_control.h"  // 电机驱动头文件

// ================= 全局变量定义 =================
//float baseSpeed = 100.0f;     // 默认基准速度
//float maxTurnAngle = 30.0f;   // 默认最大转向角
//float midTurnAngle = 15.0f;   // 默认中等转向角
//float minTurnAngle = 5.0f;    // 默认微调转向角

// ================= 八路循迹主函数 =================
/********************************************
@attention: 
1. 没加电机转动时间                              已解决
2. 到十字路口后，下一次消息action如何接收？？？   已解决
3. 停止状态怎么写 ？？？                         不延时，直接停止
4. 到达病房或者药房的状态判断可能有误    
5. 在电机的定时器加入In_turn   									已完成
@debug :
1. 十字路口正常 直行：'w' ,左转：'a',右转:'d'
2. 十字路口pi传回多次0xFF后发送指令，stm32 do while判断一直到接收指令 -- >> 执行指令
*********************************************/
// 转向中断，1. 设标志位 In_Turn
//           2. 清空RingBuffer: head = tail 直到定时结束
uint8_t In_Turn = 0;
void EightSensor_LineTracking(uint8_t sensor_state) {
		printf("进入状态机\r\n");
    static uint8_t last_state = 0;     // 上一次的状态编码
    static uint32_t cross_timer = 0;   // 十字路口计时器

    // 十字路口检测 所有传感器都触发 或者 左边 全触发 或右边全触发
    if (sensor_state == 0x03 ||sensor_state == 0x07   //0111 OR 0011
			||sensor_state == 0x0E ||sensor_state == 0x0C		//1110 OR 1100
			||sensor_state == 0x0F  //1111
			) { 
				// #1. 等待转向指令
				printf("十字路口\r\n");
				Set_Motor_State(0);
				char rx_track_buf[5] = {0};
				uint8_t len = 0;
				char action;
//				/*测试用，使用USART1直接给单片机发送左右直角弯指令*/
//				len = GetPacket(&huart1, &usart1_rx_buffer, rx_track_buf);
				do{
					len = GetPacket(&huart2, &usart2_rx_buffer, rx_track_buf);
				}
				while(len == 0 || (uint8_t)rx_track_buf[1] == sensor_state);
				/*************************************************/
				printf("接受到 %d 字节信息",len);
				action = rx_track_buf[1];
				/*不严谨的做法，没有判断包头包尾*/
				In_Turn = 1;
        Crs_Road(action);         // 调用自定义十字路口处理
				//In_Turn = 0;放Crs_Road的延迟函数中
        cross_timer = 0;
        last_state = 0;     // 重置历史状态   
        return;             // 结束当前处理周期
    }
    
    // 状态机处理
    switch (sensor_state) {
        // --- 特殊路径处理 ---
        case 0x08: // 左大弯 1000
            Set_Motor_State(7);
            last_state = LEFT_BIG_CURVE;
            break;
            
        case 0x01: // 右大弯 0001
            Set_Motor_State(8);
            last_state = RIGHT_BIG_CURVE;
            break;
            
        // --- 直行与微调 ---
        case 0x06:      // 四路直线行驶 0110
            Set_Motor_State(1);
            last_state = STRAIGHT;
            break;
            
        case 0x04: // 向左微调  0100
            Set_Motor_State(5); 
            last_state = sensor_state;
            break;
            
        case 0x02: // 向右微调 0010
            Set_Motor_State(6);
            last_state = sensor_state;
            break;
            
        // --- 异常处理 ---
        case 0x00:         // 全白(丢线)
            if (last_state == 0x08) {
                Set_Motor_State(7);  // 左转搜索
            } else if (last_state == 0x01){
                Set_Motor_State(8);  // 右转搜索
            } else {
							 //使用红外传感器看黑色 or 不使用丢线 or 使用最后一种情况
							In_Turn = 1;
							arrive();
            }
            break;
        // --- 默认处理 ---
        default: 
					 printf("没有相同状态\r\n");
            Set_Motor_State(1);  // 保守直行
            break;
    }
}

/************************************************
@brief: 八路传感器转4路  假设 8 7 6 5 4 3 2 1 （右）
												转为 8     5  4    1   一个uint8_t值，2是低位										
************************************************/
uint8_t Sensor8_to_Sensor4(uint8_t sensor_state) {
    return ((sensor_state >> 7) & 0x01) << 3 | // 第8路 → bit3
           ((sensor_state >> 4) & 0x01) << 2 | // 第5路 → bit2
           ((sensor_state >> 3) & 0x01) << 1 | // 第4路 → bit1
           ((sensor_state >> 0) & 0x01);       // 第1路 → bit0
}

// =================4路循迹主函数 =================
/*****************************************************
@brief: 4路循迹，减少未知状态
@attention: 在电机的定时器加入In_turn   已完成
******************************************************/
extern uint8_t sensor_state_8;
void FourSensor_LineTracking(uint8_t sensor_state) {
		printf("进入状态机\r\n");
    static uint8_t last_state = 0;     // 上一次的状态编码
    static uint32_t cross_timer = 0;   // 十字路口计时器

    // 十字路口 or T形
    if (sensor_state == 0x07 || sensor_state == 0x03   // 0111 or 0011
			|| sensor_state == 0x0E || sensor_state == 0x0C  // 1110 or 1100
			||sensor_state == 0x0F                           // 1111
			) {  // 0x3C = 00111100 (S2-S5)
				// #1. 等待转向指令
				printf("十字路口\r\n");
				Set_Motor_State(0);
				char rx_track_buf[5] = {0};
				uint8_t len = 0;
				char action;
//				/*测试用，使用USART1直接给单片机发送左右直角弯指令*/
//				len = GetPacket(&huart1, &usart1_rx_buffer, rx_track_buf);
				do{
					len = GetPacket(&huart2, &usart2_rx_buffer, rx_track_buf);
				}
				while(len == 0 || (uint8_t)rx_track_buf[1] == 0xFF);
				/*************************************************/
				printf("接受到 %d 字节信息",len);
				action = rx_track_buf[1];
				/*不严谨的做法，没有判断包头包尾*/
				//In_Turn = 1;
        Crs_Road(action);         // 调用自定义十字路口处理
				//In_Turn = 0;放Crs_Road的延迟函数中
        cross_timer = 0;
        last_state = 0;     // 重置历史状态   
        return;             // 结束当前处理周期
    }
    
    // 状态机处理
    switch (sensor_state) {
        // --- 特殊路径处理 ---
        case 0x08: // 左大弯 1000
            Set_Motor_State(7);
            last_state = sensor_state;
            break;
            
        case 0x01: // 右大弯 0001
            Set_Motor_State(8);
            last_state = sensor_state;
            break;
            
        // --- 直行与微调 ---
        case 0x06:      // 直线 0110
            Set_Motor_State(1);
            last_state = sensor_state;
            break;
            
        case 0x04: // 轻微左调 0100
            Set_Motor_State(5); 
            last_state = sensor_state;
            break;
            
        case 0x02: // 轻微右调 0010
            Set_Motor_State(6);
            last_state = sensor_state;
            break;
            
        // --- 异常处理 ---
        case 0x00:         // 全白(丢线)
						// #1. 先停止电机在判断
						Set_Motor_State(0);
						
						// #2. 判断下一步行动 只有传感器8路都是0才是 到达
						if(sensor_state_8 == 0x00){
							// #2.1 丢线
							if (last_state == 0x08) {	//1000
									Set_Motor_State(7);  // 左转搜索
							} else if (last_state == 0x01) {
									Set_Motor_State(8);  // 0001
							} else {
								
								// #2.2 到达
								 //使用红外传感器看黑色 or 不使用丢线 or 使用最后一种情况
								printf("达到病房（药房）\r\n");
								//Set_Motor_State(0);
//								char rx_track_buf[5] = {0};
//								uint8_t len = 0;
//								char action;
//				//				/*测试用，使用USART1直接给单片机发送左右直角弯指令*/
//				//				len = GetPacket(&huart1, &usart1_rx_buffer, rx_track_buf);
//								do{
//									len = GetPacket(&huart2, &usart2_rx_buffer, rx_track_buf);
//								}
//								while(len == 0 || (uint8_t)rx_track_buf[1] != 0x00);
//								/*************************************************/
//								printf("接受到 %d 字节信息",len);
//								action = rx_track_buf[1];
								//In_Turn = 1;
								arrive();
							}
							
						}
						break;
        // --- 默认处理 ---
        default: 
					 printf("没有相同状态\r\n");
            Set_Motor_State(1);  // 保守直行
            break;
    }
}

// ================= 十字路口全局变量 =================
uint8_t cross_go[20];    // 存储最多20个路口的转向动作
uint8_t cross_cnt_go = 0;    // 已记录的路口数量
uint8_t cross_index = 0;     // 当前处理的路口索引
uint8_t GO_or_BACK = 0;      // 默认前进模式

// ================= 十字路口动作执行器 =================
	/*修改：一直到状态监测为直行，保证转过90度*/
void Execute_Cross_Action(uint8_t action) {
    // 1. 停止电机（200ms）
	Set_Motor_State(0);
    
    // 2. 执行转向动作 ****优先级 > 接受循迹数据*** 
	/*修改：一直到状态监测为直行，保证转过90度*/
    switch(action) {
			case 'w':  //直行
            Set_Motor_State(1);  // 直行通过
            break;
            
        case 'a':  //左转
            Set_Motor_State(2);  // 左轮后退，右轮前进
            break;
            
        case 'd':  //右转
            Set_Motor_State(3);  // 左轮前进，右轮后退
            break;
				default:
						In_Turn = 0;
						printf("当前为：%d ,没有配置的指令\r\n",action);
    }
    
    // 3. 恢复直行
//    Set_Motor_Speed(80, 80);
}

// ================= 十字路口处理函数 =================
void Crs_Road(uint8_t action) {
    //前进模式：记录并执行动作
    if(GO_or_BACK == 0) {
//			// #1. 思路一：单片机记录路口转向
//        cross_go[cross_cnt_go] = action;
//        // 执行当前动作
//        Execute_Cross_Action(cross_go[cross_cnt_go]);
//        
//        // 更新索引
//        cross_cnt_go++;
//        cross_index = cross_cnt_go;  // 更新当前索引
			
			//# 2. PI记录，单片机只执行转向
			Execute_Cross_Action(action);
    }
    // 返回模式：反向执行动作
    else {
			//返回寻路指令由Pi发出
			// #1. 思路一：使用单片机返回寻路
			/************************************
        if(cross_index > 0) {
            cross_index--;  // 回溯到上一个路口
            
            // 动作反向转换
            uint8_t reverse_action;
            switch(cross_go[cross_index]) {
                case 1: 
                    reverse_action = 2;  // 原左转变为右转
                    break;
                case 2: 
                    reverse_action = 1;   // 原右转变为左转
                    break;
                default: 
                    reverse_action = 0;   // 直行保持不变
            }
            
            Execute_Cross_Action(reverse_action);
        }
			*****************************************/
			
			// #2. pi发送指令
			Execute_Cross_Action(action);  
    }
}

/*到达病房和返回药房使用相同的判断状态 ARRIVE*/
extern uint8_t medicine_taken;  // 取药完成标志（中断设置）
// 到达/返回处理函数
/*****************************************************************
@brief: 到达状态下的响应函数，分为到达病房和到达药房  测试
*****************************************************************/
void arrive(void)
{
    if (GO_or_BACK == 0)  // GO 前进模式 (使用数字0而不是GO)
    {
				printf("到达病房\r\n");
        // 1.停止电机
        //Set_Motor_State(0);
        // 2.等待卸药，点亮红灯 -->> 取药完成，熄灭红灯
        HAL_GPIO_WritePin(GPIOE, RED_LED_PIN_Pin, GPIO_PIN_SET);
				printf("等待取药\r\n");
        while (medicine_taken == 0) {
					//for(int i = 0;i<1000;i++);
					HAL_Delay(0);
        }
        medicine_taken = 0;
        HAL_GPIO_WritePin(GPIOE, RED_LED_PIN_Pin, GPIO_PIN_RESET);
				/*********************************/
//        Send_Packet(&huart1,"b",1);  //测试用suart1，实际用usart2
				
				/*********************************/
        // 3.调头 + 继续循迹        
        Set_Motor_State(4);
				
        Send_Packet(&huart2,"b",1);
        // 4. 设置为返回状态
        GO_or_BACK = 1;  // 使用数字1表示返回
				
    }
    else if (GO_or_BACK == 1)  // BACK 返回模式 (使用数字1而不是BACK)
    {
				printf("到达药房\r\n");
        // 1. 停止电机
        Set_Motor_State(0);
        
        // 2. 点亮绿灯
        HAL_GPIO_WritePin(GPIOE, GREEN_LED_PIN_Pin, GPIO_PIN_SET);
        
        // 3. 停止循迹（进入永久停止状态）
        while (1) 
        {
            // 在此处添加任何需要的低功耗或安全代码
            HAL_Delay(100);  // 维持系统运行
        }
    }
}

// ================= 电机控制接口 =================
//void Set_Motor_Speed(float left_speed, float right_speed) {
//    // 调用电机驱动函数 (需用户实现)
//    Motor_SetLeftSpeed(left_speed);
//    Motor_SetRightSpeed(right_speed);
//}

// demo_state动作定义
// 0: 停止
// 1: 前进
// 2: 左转
// 3: 右转
// 4: 掉头
// 5: 左微调
// 6: 右微调
// 7: 左大弯
// 8: 右大弯
/***************************************************
@brief: 测试函数，到电机状态后直接打印状态
void Car_Stop(void);
bool Car_Turn_Left_90_And_Stop2s(void);
bool Car_Turn_Right_90_And_Stop2s(void);
bool Car_Turn_Arounde_And_Stop2s(void);
bool Car_G0_Straight_And_Stop2s(void);
uint8_t Car_Active_Brake(float deceleration);
Set_Motor_Target_Speed(10.0*speed_L,-10.0*speed_R);
****************************************************/
extern float speed_L;
extern float speed_R;
void Set_Motor_State(uint8_t demo_state) {
    switch(demo_state) {
        case 0:
            printf("[MOTOR] 停止\n");
						Car_Stop();
						Motor_Control_Task();
            // 停止电机逻辑
            break;
            
        case 1:
            printf("[MOTOR] 前进\n");
						Set_Motor_Target_Speed(10.0,-10.0);
						Motor_Control_Task();
						//NonBlocking_Delay(1000);
            // 前进逻辑
            break;
            
        case 2:
            printf("[MOTOR] 左转\n");
						Car_Turn_Left_90_And_Stop2s();
						Motor_Control_Task();
						//NonBlocking_Delay(1000);
						Set_Motor_Target_Speed(0.0f, -8.0f);
						Motor_Control_Task();
						NonBlocking_Delay(5000);  //实际调试的是  300
						Car_Stop();
						printf("转向停止/r/n");
						Motor_Control_Task();		
            // 左转逻辑
            break;
            
        case 3:
            printf("[MOTOR] 右转\n");
						Car_Turn_Right_90_And_Stop2s();
						Motor_Control_Task();
						//NonBlocking_Delay(1000);
						Set_Motor_Target_Speed(8.0f, 0.0f);
						Motor_Control_Task();
						NonBlocking_Delay(5000);  //实际调试的是  300
						Car_Stop();
						printf("转向停止/r/n");
						Motor_Control_Task();		
            // 右转逻辑
            break;
            
        case 4:
            printf("[MOTOR] 掉头\n");
						//Car_Turn_Arounde_And_Stop2s();
						Set_Motor_Target_Speed(8.0f, 9.0f);
						Motor_Control_Task();
						NonBlocking_Delay(5000);  //实际调试的是  350
						Car_Stop();
						printf("转向停止/r/n");
						Motor_Control_Task();				
            // 掉头逻辑
            break;
            
        case 5:
            printf("[MOTOR] 左微调\n");
						Set_Motor_Target_Speed(10.0,-10.0*1.1);
						Motor_Control_Task();
            // 左轮微调加速逻辑
            break;
            
        case 6:
						Set_Motor_Target_Speed(10.0*1.1,-10.0);
						Motor_Control_Task();
            printf("[MOTOR] 右微调\n");
            // 右轮微调加速逻辑
            break;
            
        case 7:
						Set_Motor_Target_Speed(10.0,-10.0*100);
						Motor_Control_Task();
            printf("[MOTOR] 左大弯\n");
            // 左轮微调减速逻辑
            break;
            
        case 8:
						Set_Motor_Target_Speed(10.0*100,-10.0);
						Motor_Control_Task();
            printf("[MOTOR] 右大弯\n");
            // 右轮微调减速逻辑
            break;
            
        default:
            printf("[ERROR] 未知电机状态: %d\n", demo_state);
            break;
    }
}

/**********************************************
@brief: 测试函数，应该写进电机控制函数中的定时器
***********************************************/
void NonBlocking_Delay(uint32_t delay_ms) {
    uint32_t start_tick = HAL_GetTick();
    while (HAL_GetTick() - start_tick < delay_ms) {
       In_Turn = 1;
    }
		printf("转向完成/r/n");
		In_Turn = 0;
		usart2_rx_buffer.tail = usart2_rx_buffer.head;  // 不读转向时的传感器数据
}
