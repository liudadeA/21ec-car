#ifndef __CONTROL_H
#define __CONTROL_H
#include "Control.h"
#include "tim.h"
#include "gpio.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>



// PID控制器结构体
typedef struct {
    float Kp;              // 比例系数
    float Ki;              // 积分系数
    float Kd;              // 微分系数
    
    float setpoint;        // 目标值(RPM)
    float input;           // 当前输入值（反馈RPM）
    float output;          // PID输出值
    
    float error;           // 当前误差
    float last_error;      // 上次误差
    float integral;        // 积分累积
    float derivative;      // 微分值
    
    float output_min;      // 输出最小值
    float output_max;      // 输出最大值
    
    uint32_t last_time;    // 上次计算时间
    uint8_t enabled;       // 是否启用
    
    // 抗积分饱和
    float integral_limit;  // 积分限制值
    float dead_zone;       // 死区大小
} PID_Controller;

// 控制模式枚举
typedef enum {
    CONTROL_MODE_SPEED = 0,
    CONTROL_MODE_POSITION
} ControlMode_t;

// 编码器结构体
typedef struct {
    TIM_HandleTypeDef *htim;    // 定时器句柄
    int32_t pulse_count;        // 脉冲计数
    int32_t last_pulse_count;   // 上次脉冲计数
    float speed_rpm;            // 转速（RPM）
    uint32_t last_time;         // 上次计算时间
    float total_revolutions;    // 总圈数
    
    // 速度滤波
    float speed_filter;         // 速度滤波值
    float last_speed;           // 上次速度
} Encoder_t;

// 电机控制结构体
typedef struct {
    Encoder_t encoder;          // 编码器
    PID_Controller pid;         // PID内环控制器
	PID_Controller outerpid;
    float target_speed;         // 目标速度（RPM）
    float current_speed;        // 当前速度（RPM）
	float target_quanshu;		//目标圈数
	float current_quanshu;		//当前圈数
    int16_t pwm_output;         // PWM输出值
    uint8_t enabled;            // 是否启用PID控制
} Motor_t;




// TB6612控制宏定义
#define MOTOR_TIM &htim2  // 使用TIM2生成PWM
#define PWM_CHANNEL_LEFT TIM_CHANNEL_1  // PA0对应TIM2_CH1 - 左轮
#define PWM_CHANNEL_RIGHT TIM_CHANNEL_2 // PA1对应TIM2_CH2 - 右轮

// 方向控制引脚
#define AO1_PIN GPIO_PIN_4  // 左轮方向1
#define AO2_PIN GPIO_PIN_6  // 左轮方向2
#define BO1_PIN GPIO_PIN_3  // 右轮方向1
#define BO2_PIN GPIO_PIN_5  // 右轮方向2

// 电机方向定义
#define MOTOR_STOP 0
#define MOTOR_FORWARD 1
#define MOTOR_BACKWARD 2

// 编码器相关定义
#define ENCODER_RESOLUTION 40000   // 编码器分辨率（脉冲/转）
#define GEAR_RATIO 20.0f         // 齿轮比
#define SPEED_CALC_PERIOD 50    // 速度计算周期（ms）

// 定时器相关定义
#define DELAY_TIMER &htim3  // 使用TIM3作为延时定时器
#define DELAY_TIMER_CHANNEL TIM_CHANNEL_1

// 延时状态
typedef enum {
    DELAY_IDLE,      // 空闲状态
    DELAY_RUNNING,   // 延时运行中
    DELAY_COMPLETE   // 延时完成
} DelayState_t;

// 延时控制结构体
typedef struct {
    uint32_t target_time;    // 目标时间
    DelayState_t state;      // 当前状态
} DelayControl_t;


// 延时控制变量
extern DelayControl_t delay_ctrl;   // 如果主程序要用，也要extern
extern uint32_t system_tick_count;

// 电机控制全局变量
extern Motor_t left_motor;   // 只声明，不定义
extern Motor_t right_motor;
extern uint8_t pid_control_enabled; // 只声明，不定义

extern float interval_in;   //隔interval长时间（ms）执行一次PID计算和编码器测数；
extern float interval_out;

extern float total_revolutions_right; 
extern float total_revolutions_left; 

extern volatile uint8_t In_Turn;

void Motor_Init(void);
void Motor_SetDirection(uint8_t dir);
void Motor_Right_SetSpeed(uint8_t duty_percent);
void Motor_Left_SetSpeed(uint8_t duty_percent);
void Timer_Delay_Start(uint32_t ms);
uint8_t Timer_Delay_IsComplete(void);


// PID控制函数
void PID_Init(PID_Controller *pid, float kp, float ki, float kd,float output_limit,float integral_limit,float dead_zone);
float PID_Compute(PID_Controller *pid, float input);
void PID_SetSetpoint(PID_Controller *pid, float setpoint);
void PID_Reset(PID_Controller *pid);

// 编码器函数
void Encoder_Init(Encoder_t *encoder, TIM_HandleTypeDef *htim);
void Encoder_Update(Encoder_t *encoder);

// 电机控制函数
void Motor_PID_Init(void);
void Motor_PID_Speed_Control(Motor_t *motor, float target_speed);
void Motor_PID_Position_Control(Motor_t *motor, float target_quanshu);
void Set_Control_Mode(uint8_t mode);
uint8_t Is_Position_Control_Complete(void);
void Motor_Control_Task(void);   
void Set_Motor_Target_Speed(float left_rpm, float right_rpm);
void Set_Motor_Target_Position(float left_quanshu, float right_quanshu);
void TB6612_SetMotor(uint8_t motor_id, int16_t speed);
void Reset_Encoder_Revolution_Count(void);

//电机转向
void Car_Go(float left_rpm, float right_rpm);
void Car_Go_Position(void);
void Car_Stop(void);
void Car_Turn_Left_90_And_Stop(void);
void Car_Turn_Right_90_And_Stop(void);
void Car_Turn_Around_And_Stop(void);
void Car_Go_Straight_And_Stop(void);
void Car_Straight_Then_Turn_Right_90_And_Stop(void);
void Car_Straight_Then_Turn_Left_90_And_Stop(void);
void Car_Stop_Precise(void);
uint8_t Is_Motion_Complete(void);

#endif
