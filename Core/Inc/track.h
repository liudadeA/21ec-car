#ifndef __LINE_TRACKING_H__
#define __LINE_TRACKING_H__

#include "stm32f4xx.h"

// ================= 八路传感器状态宏定义 =================
#define SENSOR_0      (1 << 0)  // 最左侧传感器
#define SENSOR_1      (1 << 1)
#define SENSOR_2      (1 << 2)
#define SENSOR_3      (1 << 3)  // 中心左侧
#define SENSOR_4      (1 << 4)  // 中心右侧
#define SENSOR_5      (1 << 5)
#define SENSOR_6      (1 << 6)
#define SENSOR_7      (1 << 7)  // 最右侧传感器

// ================= 八路关键状态模式宏 =================
#define CROSS_ROAD      0xFF        // 十字路口                     - - - - | - - - -
#define LEFT_FULL_TRIGGER   (SENSOR_0 | SENSOR_1 | SENSOR_2 | SENSOR_3)      // T字路口
#define RIGHT_FULL_TRIGGER  (SENSOR_4 | SENSOR_5 | SENSOR_6 | SENSOR_7)      // T字路口
#define LEFT_LITTLE (SENSOR_2 | SENSOR_3)
#define RIGHT_LITTLE (SENSOR_4 | SENSOR_5)
#define LEFT_BIG_CURVE  (SENSOR_0 | SENSOR_1) // 左大弯
#define RIGHT_BIG_CURVE (SENSOR_5 | SENSOR_6) // 右大弯
//#define STRAIGHT        (SENSOR_3 | SENSOR_4)      // 八路直线行驶
#define STRAIGHT        (SENSOR_3 | SENSOR_4 | SENSOR_5 | SENSOR_2)      // 四路直线行驶
#define ARRIVE //到达药房或者病房：但是！！！可能和丢线冲突

//// ================= 四路关键状态模式宏 =================
//#define CROSS_ROAD	1        // 十字路口 1111
//#define LEFT_FULL_TRIGGER	0        // T字路口 0111 or 0011
//#define RIGHT_FULL_TRIGGER	        // T字路口  1110 or 1100
//#define LEFT_LITTLE	4               //向左微调  0100
//#define RIGHT_LITTLE	 	2						 //向右微调 0010
//#define LEFT_BIG_CURVE	8 // 左大弯 1000
//#define RIGHT_BIG_CURVE  1 // 右大弯 0001
//#define STRAIGHT	6          // 四路直线行驶 0110
//#define ARRIVE 0//到达药房或者病房：但是！！！可能和丢线冲突

extern uint8_t In_Turn;

// ================= 函数声明 =================
void LineTracking_Init(void);  // 循迹模块初始化
void EightSensor_LineTracking(uint8_t sensor_state);  // 8路循迹函数
void FourSensor_LineTracking(uint8_t sensor_state);   // 4路循迹
void Set_Motor_Speed(float left_speed, float right_speed);  // 电机控制接口
void Set_Motor_State(uint8_t demo_state);
void arrive(void); //到达病房或药房
// 十字路口处理函数 (需用户实现)
void Crs_Road(uint8_t action);

//八路传感 转 4路
uint8_t Sensor8_to_Sensor4(uint8_t sensor_state);

//测试用
void NonBlocking_Delay(uint32_t delay_ms);
#endif // __LINE_TRACKING_H__
