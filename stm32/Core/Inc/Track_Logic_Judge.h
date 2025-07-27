#ifndef __TRACK_MODULE_H
#define __TRACK_MODULE_H

#include "stm32f4xx_hal.h"
#include <string.h>

// 传感器引脚定义（根据实际接线修改）
#define DH1_Pin GPIO_PIN_8
#define DH2_Pin GPIO_PIN_10
#define DH3_Pin GPIO_PIN_12
#define DH4_Pin GPIO_PIN_15
#define DH_GPIO GPIOE
#define Sensors_Delay 10

// 巡线参数结构体（可通过串口动态调整）
typedef struct {
    float Turn90Angle;   // 直角弯转向角度
    float maxTurnAngle;  // 大弯转向角度
    float midTurnAngle;  // 丢线转向角度
    float minTurnAngle;  // 微调转向角度
    float baseSpeed;     // 基础速度
} TrackParams;

// 传感器状态结构体
typedef struct {
    uint8_t DH1;
    uint8_t DH2;
    uint8_t DH3;
    uint8_t DH4;
} SensorState;

// 控制输出结构体
typedef struct {
    float Move_X;  // 前进速度
    float Move_Z;  // 转向角速度
} ControlOutput;

// 模块初始化函数
//void TrackModule_Init(void);

// 传感器数据采集
SensorState Track_ReadSensors(void);

// 巡线控制逻辑
ControlOutput Track_Process(const SensorState* state, TrackParams* params);

// 红外采集调试
// 新增调试函数实现
void Track_DebugMode(UART_HandleTypeDef* huart);
#endif
