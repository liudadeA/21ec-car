#include "stm32f4xx.h"                  
#include "Control.h"
#include <stdbool.h>
#include <stdlib.h>

Motor_t left_motor = {0};
Motor_t right_motor = {0};
uint8_t pid_control_enabled = 0;
DelayControl_t delay_ctrl = {0};
uint32_t system_tick_count = 0;
float interval_in;
float interval_out;

float total_revolutions_right;
float total_revolutions_left;

static ControlMode_t current_control_mode = CONTROL_MODE_SPEED;
static uint8_t motion_complete = 1; // 动作完成标志

/**
  * @brief  初始化电机控制相关引脚和PWM
  * @param  None
  * @retval None
  */
void Motor_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
 
    // 初始化为停止状态
    HAL_GPIO_WritePin(GPIOA, AO1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, AO2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, BO1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, BO2_PIN, GPIO_PIN_RESET);	
	
    // 启动双通道PWM输出（打开TIM2的两个输出通道）
    HAL_TIM_PWM_Start(MOTOR_TIM, PWM_CHANNEL_LEFT);  
    HAL_TIM_PWM_Start(MOTOR_TIM, PWM_CHANNEL_RIGHT);
}

/**
  * @brief  控制电机方向
  * @param  dir: 方向（MOTOR_STOP / MOTOR_FORWARD / MOTOR_BACKWARD）
  * @retval None
  */
void Motor_SetDirection(uint8_t dir)
{
    switch(dir)
    {
        case MOTOR_STOP:
            HAL_GPIO_WritePin(GPIOA, AO1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, AO2_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, BO1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, BO2_PIN, GPIO_PIN_RESET);
            break;
            
        case MOTOR_FORWARD:
            HAL_GPIO_WritePin(GPIOA, AO1_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, AO2_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, BO1_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, BO2_PIN, GPIO_PIN_RESET);
            break;
            
        case MOTOR_BACKWARD:
            HAL_GPIO_WritePin(GPIOA, AO1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, AO2_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, BO1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, BO2_PIN, GPIO_PIN_SET);
            break;
            
        default:
            break;
    }
}

/**
  * @brief  设置右轮PWM占空比
  * @param  duty_percent: 占空比（0-100，对应0%-100%）
  * @retval None
  */
void Motor_Right_SetSpeed(uint8_t duty_percent)
{
    uint16_t arr = __HAL_TIM_GET_AUTORELOAD(MOTOR_TIM);   //获得TIM2的ARR的值
    uint16_t pulse = (duty_percent * (arr + 1)) / 100;
    
    if (pulse > arr) pulse = arr;  

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = pulse;  
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    
    if (HAL_TIM_PWM_ConfigChannel(MOTOR_TIM, &sConfigOC, PWM_CHANNEL_RIGHT) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_TIM_PWM_Start(MOTOR_TIM, PWM_CHANNEL_RIGHT);
}

/**
  * @brief  设置左轮PWM占空比
  * @param  duty_percent: 占空比（0-100，对应0%-100%）
  * @retval None
  */
void Motor_Left_SetSpeed(uint8_t duty_percent)
{
    uint16_t arr = __HAL_TIM_GET_AUTORELOAD(MOTOR_TIM);
    uint16_t pulse = (duty_percent * (arr + 1)) / 100;
    
    if (pulse > arr) pulse = arr;  

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = pulse;  
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    
    if (HAL_TIM_PWM_ConfigChannel(MOTOR_TIM, &sConfigOC, PWM_CHANNEL_LEFT) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_TIM_PWM_Start(MOTOR_TIM, PWM_CHANNEL_LEFT);
}

/**
  * @brief  初始化PID控制器
  */
void PID_Init(PID_Controller *pid, float kp, float ki, float kd,float output_limit,float integral_limit,float dead_zone)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    
    pid->setpoint = 0.0f;
    pid->input = 0.0f;
    pid->output = 0.0f;
    
    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->integral = 0.0f;
    pid->derivative = 0.0f;
    
    pid->output_min = -output_limit;
    pid->output_max = output_limit;
    
    pid->last_time = HAL_GetTick();
    pid->enabled = 1;
    
    pid->integral_limit = integral_limit;  // 积分限制
    pid->dead_zone = dead_zone;        // 死区
}

/**
  * @brief  PID计算函数
  */
float PID_Compute(PID_Controller *pid, float input)
{
    // 计算误差
    pid->input = input;
    pid->error = pid->setpoint - pid->input;
    
    // 死区处理
    if (fabs(pid->error) < pid->dead_zone) {
        pid->error = 0.0f;
    }
    
    // 比例项
    float proportional = pid->Kp * pid->error;
    
    // 积分项
    if (fabs(pid->error) > 0.1f) {
        pid->integral += pid->error ;
        
        // 积分限幅
        if (pid->integral > pid->integral_limit) {
            pid->integral = pid->integral_limit;
        } else if (pid->integral < -pid->integral_limit) {
            pid->integral = -pid->integral_limit;
        }
    }
    
    float integral_term = pid->Ki * pid->integral;
    
    // 微分项
    pid->derivative = (pid->error - pid->last_error) ;
    float derivative_term = pid->Kd * pid->derivative;
    
    // PID输出
    pid->output = proportional + integral_term + derivative_term;
    
    // 输出限幅
    if (pid->output > pid->output_max) {
        pid->output = pid->output_max;
    } else if (pid->output < pid->output_min) {
        pid->output = pid->output_min;
    }
    
    // 更新历史值
    pid->last_error = pid->error;

    
    return pid->output;
}

/**
  * @brief  设置PID目标值
  */
void PID_SetSetpoint(PID_Controller *pid, float setpoint)
{
    pid->setpoint = setpoint;
}

/**
  * @brief  重置PID控制器
  */
void PID_Reset(PID_Controller *pid)
{
    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->integral = 0.0f;
    pid->derivative = 0.0f;
    pid->output = 0.0f;
    pid->last_time = HAL_GetTick();
}

/**
  * @brief  初始化编码器
  */
void Encoder_Init(Encoder_t *encoder, TIM_HandleTypeDef *htim)
{
    encoder->htim = htim;
    encoder->pulse_count = 0;
    encoder->last_pulse_count = 0;
    encoder->speed_rpm = 0.0f;
    encoder->last_time = HAL_GetTick();
    encoder->total_revolutions = 0.0f;
    encoder->speed_filter = 0.0f;
    encoder->last_speed = 0.0f;
}

/**
  * @brief  更新编码器数据
  */
void Encoder_Update(Encoder_t *encoder)
{
    
        // 读取编码器计数值
        encoder->pulse_count = (int32_t)__HAL_TIM_GET_COUNTER(encoder->htim);
        
        // 计算脉冲差值
        int32_t pulse_diff = encoder->pulse_count - encoder->last_pulse_count;
        
        // 处理计数器溢出
        if (pulse_diff > 32767) {
            pulse_diff -= 65536;
        } else if (pulse_diff < -32768) {
            pulse_diff += 65536;
        }
        
        // 计算转速（RPM），单位是转/分钟
        float raw_speed = (float)pulse_diff * 60.0f * 1000.0f / 
                         (20470 * interval_in);
        
        
        // 累计圈数
        encoder->total_revolutions += (float)pulse_diff /20470*100;
        
        // 更新历史值
        encoder->last_pulse_count = encoder->pulse_count;
        
        encoder->speed_rpm = raw_speed;
    }


/**
  * @brief  初始化电机PID控制系统
  */
void Motor_PID_Init(void)
{
    // 初始化编码器（TIM4和TIM8配置为编码器模式）
    Encoder_Init(&left_motor.encoder, &htim4);   // 左轮编码器
    Encoder_Init(&right_motor.encoder, &htim8);  // 右轮编码器
    
    // 初始化PID控制器
    PID_Init(&left_motor.pid, 0.02f, 0.21f, 0.13f,100.0f,150.0f,5.0f);      //内环PID参数设置
    PID_Init(&right_motor.pid, 0.02f, 0.21f, 0.13f,100.0f,150.0f,5.0f);		//kp ki,kd, output_limit, integral_limit, dead_zone
	
	PID_Init(&left_motor.outerpid,10.0f,0.0f,0.0f,100.0f,10.0f,3.0f);		//外环PID参数设置
    PID_Init(&right_motor.outerpid,10.0f,0.0f,0.0f,100.0f,10.0f,3.0f);
    
    // 初始化电机参数
    left_motor.target_speed = 0;
    left_motor.current_speed = 0;
	left_motor.target_quanshu = 0;
	left_motor.current_quanshu = 0;
    left_motor.enabled = 0;
    
    right_motor.target_speed = 0;
    right_motor.current_speed = 0;
	right_motor.target_quanshu = 0;
	right_motor.current_quanshu = 0;
	
    right_motor.enabled = 0;
    
    // 启动编码器定时器
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
    
    pid_control_enabled = 1;
}

/**
  * @brief  电机PID速度控制
  */
void Motor_PID_Speed_Control(Motor_t *motor, float target_speed)
{
    // 更新编码器数据
    Encoder_Update(&motor->encoder);
    
    // 设置目标速度
    motor->target_speed = target_speed;
    motor->current_speed = motor->encoder.speed_rpm;
        
    // 计算PID输出
    PID_SetSetpoint(&motor->pid, target_speed);
    float pid_output = PID_Compute(&motor->pid, motor->current_speed);
    
    motor->pwm_output = (int16_t)pid_output;
}

// 电机位置PID控制(为外环PID控制，目的是为了内环速度pid提供目标速度)
void Motor_PID_Position_Control(Motor_t *motor, float target_quanshu)
{
    // 更新编码器数据
    Encoder_Update(&motor->encoder);
    
    // 设置目标圈数
    motor->target_quanshu = target_quanshu;
    motor->current_quanshu = motor->encoder.total_revolutions;
        
    // 计算PID输出
    PID_SetSetpoint(&motor->outerpid, target_quanshu);
    float pid_output = PID_Compute(&motor->outerpid, motor->current_quanshu);
    
    motor->target_speed = (int16_t)pid_output;
}

/**
  * @brief  统一的电机控制任务 - 根据当前模式选择控制方式
  */
void Motor_Control_Task(void)
{
    static uint32_t last_control_time_in = 0;
    static uint32_t last_control_time_out = 0;
    uint32_t current_time = HAL_GetTick();
    
    if (!pid_control_enabled) return;
    
    if (current_control_mode == CONTROL_MODE_SPEED) {
        // 速度控制模式 - 每30ms执行一次
        interval_in = current_time - last_control_time_in;
        if (interval_in >= 30) {
            Motor_PID_Speed_Control(&left_motor, left_motor.target_speed);
            Motor_PID_Speed_Control(&right_motor, right_motor.target_speed);
            
            TB6612_SetMotor(0, left_motor.pwm_output);
            TB6612_SetMotor(1, right_motor.pwm_output);
            
            last_control_time_in = current_time;
        }
    } else {
        // 位置控制模式 - 串级PID
        interval_in = current_time - last_control_time_in;
        interval_out = current_time - last_control_time_out;
        
        // 内环速度控制 - 每10ms
        if (interval_in >= 10) {
            Motor_PID_Speed_Control(&left_motor, left_motor.target_speed);
            Motor_PID_Speed_Control(&right_motor, right_motor.target_speed);
            TB6612_SetMotor(0, left_motor.pwm_output);
            TB6612_SetMotor(1, right_motor.pwm_output);
            last_control_time_in = current_time;
        }
        
        // 外环位置控制 - 每40ms
        if (interval_out >= 40) {
            Motor_PID_Position_Control(&left_motor, left_motor.target_quanshu);
            Motor_PID_Position_Control(&right_motor, right_motor.target_quanshu);
            last_control_time_out = current_time;
        }
    }
}

/**
  * @brief  设置控制模式
  */
void Set_Control_Mode(ControlMode_t mode)
{
    if (current_control_mode != mode) {
        current_control_mode = mode;
        // 切换模式时重置PID
        PID_Reset(&left_motor.pid);
        PID_Reset(&right_motor.pid);
        if (mode == CONTROL_MODE_POSITION) {
            PID_Reset(&left_motor.outerpid);
            PID_Reset(&right_motor.outerpid);
        }
    }
}

/**
  * @brief  检查位置控制是否完成
  */
uint8_t Is_Position_Control_Complete(void)
{
    if (current_control_mode != CONTROL_MODE_POSITION) {
        return 1; // 速度模式总是返回完成
    }
    printf("\xBB[电机] 左轮圈数: %.2f, 右轮圈数: %.2f\r\n\x55\x44\x33", 
           left_motor.current_quanshu, right_motor.current_quanshu);
          
    left_motor.current_quanshu = left_motor.encoder.total_revolutions;
    right_motor.current_quanshu = right_motor.encoder.total_revolutions;

    printf("\xBB[电机] 左轮圈数: %.2f, 右轮圈数: %.2f\r\n\x55\x44\x33", 
           left_motor.current_quanshu, right_motor.current_quanshu);

    float left_error = fabs(left_motor.target_quanshu - left_motor.current_quanshu);
    float right_error = fabs(right_motor.target_quanshu - right_motor.current_quanshu);
    
    printf("\xBB[电机] 左轮误差: %.2f, 右轮误差: %.2f\r\n\x55\x44\x33", 
           left_error, right_error);

    return (left_error <= 10.0f && right_error <= 10.0f);
}

/**
  * @brief  设置电机目标速度，
  */
void Set_Motor_Target_Speed(float left_rpm, float right_rpm)
{
    left_motor.target_speed = left_rpm;
    right_motor.target_speed = right_rpm;
}

//设置电机目标圈数
void Set_Motor_Target_Position(float left_quanshu, float right_quanshu)
{
    left_motor.target_quanshu = left_quanshu;
    right_motor.target_quanshu = right_quanshu;
}

/**
  * @brief  重置编码器圈数计数
  */
void Reset_Encoder_Revolution_Count(void)
{
    left_motor.encoder.total_revolutions = 0.0f;
    right_motor.encoder.total_revolutions = 0.0f;
}

/**
  * @brief  TB6612电机驱动函数
  */
void TB6612_SetMotor(uint8_t motor_id, int16_t speed)
{
    uint32_t pwm_value;
    
    // 限制速度范围
    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;
    
    // 计算PWM值
    pwm_value = (uint32_t)(abs(speed));
    
    if (motor_id == 0) {  // 左轮
        if (speed > 0) {
            // 正转
            HAL_GPIO_WritePin(GPIOA, AO1_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, AO2_PIN, GPIO_PIN_RESET);
        } else if (speed <= 0) {
            // 反转
            HAL_GPIO_WritePin(GPIOA, AO1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, AO2_PIN, GPIO_PIN_SET);
        } 
        Motor_Left_SetSpeed(pwm_value);
    } else {  // 右轮
        if (speed > 0) {
            // 正转
            HAL_GPIO_WritePin(GPIOA, BO1_PIN, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOA, BO2_PIN, GPIO_PIN_RESET);
        } else if (speed <= 0) {
            // 反转
            HAL_GPIO_WritePin(GPIOA, BO1_PIN, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOA, BO2_PIN, GPIO_PIN_SET);
        } 
        Motor_Right_SetSpeed(pwm_value);
    }
}

//void Car_Straight_Then_Turn_Left_90_And_Stop(void)

//所有需要执行的函数如下：(转向/掉头结束后，应将状态设置为停止状态Car_Stop（），再去识别改变状态机)
//转向用最后面的两个函数，void Car_Straight_Then_Turn_Right_90_And_Stop(void)和void Car_Straight_Then_Turn_Left_90_And_Stop(void)，完成后返回In_Turn = 0;

/**
  * @brief  连续运动 - 速度控制
  */
void Car_Go(float left_rpm, float right_rpm)
{
    Set_Control_Mode(CONTROL_MODE_SPEED);
    Set_Motor_Target_Speed(left_rpm, right_rpm);
    motion_complete = 0; // 开始运动
}

void Car_Go_Position(void)
{
    Set_Control_Mode(CONTROL_MODE_POSITION);
    float current_left = left_motor.encoder.total_revolutions;
    float current_right = right_motor.encoder.total_revolutions;
    Set_Motor_Target_Position(current_left + 150.0f, current_right - 150.0f);
}

/**
  * @brief  立即停止 - 速度控制
  */
void Car_Stop(void)
{
    Set_Control_Mode(CONTROL_MODE_SPEED);
    Set_Motor_Target_Speed(0.0f, 0.0f);
    motion_complete = 1; // 停止完成
}

/**
  * @brief  精确停止 - 位置控制
  */
void Car_Stop_Precise(void)
{
    Set_Control_Mode(CONTROL_MODE_POSITION);
    // 保持当前位置
    Set_Motor_Target_Position(left_motor.encoder.total_revolutions, 
                            right_motor.encoder.total_revolutions);   
  
}

/**
  * @brief  原地左转90度
  */
void Car_Turn_Left_90_And_Stop(void)
{
    static uint8_t turn_initialized = 0;
    
    if (!turn_initialized) {
        Set_Control_Mode(CONTROL_MODE_POSITION);
        total_revolutions_left = left_motor.encoder.total_revolutions;
        total_revolutions_right = right_motor.encoder.total_revolutions;
        
        // 左转：左轮后退，右轮前进
        Set_Motor_Target_Position(total_revolutions_left - 160.0f, 
                                total_revolutions_right + 160.0f);
        turn_initialized = 1;
        motion_complete = 0;
    }
    
    if (Is_Position_Control_Complete()) {
        turn_initialized = 0;
        motion_complete = 1;
    }
}

/**
  * @brief  原地右转90度
  */
void Car_Turn_Right_90_And_Stop(void)
{
    static uint8_t turn_initialized = 0;
    
    if (!turn_initialized) {
        Set_Control_Mode(CONTROL_MODE_POSITION);
        total_revolutions_left = left_motor.encoder.total_revolutions;
        total_revolutions_right = right_motor.encoder.total_revolutions;
        
        // 右转：左轮前进，右轮后退
        Set_Motor_Target_Position(total_revolutions_left + 160.0f, 
                                total_revolutions_right - 160.0f);
        turn_initialized = 1;
        motion_complete = 0;
    }
    
    if (Is_Position_Control_Complete()) {
        turn_initialized = 0;
        motion_complete = 1;
    }
}

/**
  * @brief  原地掉头180度
  */
void Car_Turn_Around_And_Stop(void)
{
    static uint8_t turn_initialized = 0;
    
    if (!turn_initialized) {
        Set_Control_Mode(CONTROL_MODE_POSITION);
        total_revolutions_left = left_motor.encoder.total_revolutions;
        total_revolutions_right = right_motor.encoder.total_revolutions;
        
        // 掉头：两轮反向转动
        Set_Motor_Target_Position(total_revolutions_left + 320.0f, 
                                total_revolutions_right - 320.0f);
        turn_initialized = 1;
        motion_complete = 0;
    }
    
    if (Is_Position_Control_Complete()) {
        turn_initialized = 0;
        motion_complete = 1;
    }
}

/**
  * @brief  先直行再右转90度
  */
 uint8_t turn_step = 0;
uint8_t turn_initialized = 0;
float turn_start_left = 0.0f;
float turn_start_right = 0.0f;
void Car_Straight_Then_Turn_Right_90_And_Stop(void)
{
    if (!turn_initialized) {
        Set_Control_Mode(CONTROL_MODE_POSITION);
        turn_start_left = left_motor.encoder.total_revolutions;
        turn_start_right = right_motor.encoder.total_revolutions;

        printf("\xBB[转向动作] 转之前圈数: 左轮 %.2f, 右轮 %.2f\r\n\x55\x44\x33", 
               turn_start_left, turn_start_right);

        turn_step = 0;
        turn_initialized = 1;
        motion_complete = 0;
        printf("\xBB[转向动作] 开始执行：先直行再右转90度\r\n\x55\x44\x33");
    }
    
    if (turn_step == 0) {
        // 第一步：直行
        printf("\xBB[转向动作] 步骤0：直行\r\n\x55\x44\x33");
        Set_Motor_Target_Position(turn_start_left + 1500.0f, turn_start_right);

        while(!Is_Position_Control_Complete()){
        Motor_Control_Task();
        }

        printf("\xBB[转向动作] 转之后圈数: 左轮 %.2f, 右轮 %.2f\r\n\x55\x44\x33", 
               left_motor.encoder.total_revolutions, right_motor.encoder.total_revolutions);

        if (Is_Position_Control_Complete()) {
            turn_step = 1;
            // 记录转向起点
            turn_start_left = left_motor.encoder.total_revolutions;
            turn_start_right = right_motor.encoder.total_revolutions;
            printf("\xBB[转向动作] 步骤0完成，进入步骤1\r\n\x55\x44\x33");
        }
    } 
    if (turn_step == 1) {
        // 第二步：右转90度
        printf("\xBB[转向动作] 步骤1：右转90度\r\n\x55\x44\x33");
        Set_Motor_Target_Position(turn_start_left + 1600.0f, 
                                turn_start_right - 1600.0f);

        while(!Is_Position_Control_Complete()){
        Motor_Control_Task();
        }

        if (Is_Position_Control_Complete()) {
            printf("\xBB[转向动作] 步骤1完成，动作结束\r\n\x55\x44\x33");
            turn_initialized = 0;
            turn_step = 0;
            motion_complete = 1;
        }
    }
}

/**
  * @brief  先直行再左转90度
  */
void Car_Straight_Then_Turn_Left_90_And_Stop(void)
{
    static uint8_t step = 0;
    static uint8_t initialized = 0;
    
    if (!initialized) {
        Set_Control_Mode(CONTROL_MODE_POSITION);
        total_revolutions_left = left_motor.encoder.total_revolutions;
        total_revolutions_right = right_motor.encoder.total_revolutions;
        step = 0;
        initialized = 1;
        motion_complete = 0;
    }
    
    if (step == 0) {
        // 第一步：直行
        Set_Motor_Target_Position(total_revolutions_left + 150.0f, 
                                total_revolutions_right - 150.0f);
        
        if (Is_Position_Control_Complete()) {
            step = 1;
            // 记录转向起点
            total_revolutions_left = left_motor.encoder.total_revolutions;
            total_revolutions_right = right_motor.encoder.total_revolutions;
        }
    } else if (step == 1) {
        // 第二步：左转90度
        Set_Motor_Target_Position(total_revolutions_left - 160.0f, 
                                total_revolutions_right + 160.0f);
        
        if (Is_Position_Control_Complete()) {
            initialized = 0;
            step = 0;
            motion_complete = 1;
        }
    }
}

/**
  * @brief  检查运动是否完成
  */
uint8_t Is_Motion_Complete(void)
{
    return motion_complete;
}
