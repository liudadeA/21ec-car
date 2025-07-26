/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @文件           : main.c
  * @�??�?          : 智能送药小车主程�?
  ******************************************************************************
  * @注意
  *
  * Copyright (c) 2025 STMicroelectronics.
  * 保留�?有权�?
  *
  * 本软件的许可条款可在该软件组件根目录下的LICENSE文件中找到�??
  * 如果本软件未附带LICENSE文件，则按AS-IS方式提供�?
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include "ring_buffer.h"
#include "usart_handler.h"
#include "Control.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// 系统状�?�枚�?
typedef enum {
    SYS_INIT = 0,                  // 系统初始�?
    SYS_WAITING_MEDICINE,          // 等待装药
    SYS_LINE_TRACKING,             // 循线行驶
    SYS_CROSS_PROCESSING,          // 路口处理
    SYS_ARRIVED,                   // 到达目的�?
    SYS_MISSION_COMPLETE,          // 任务完成
    SYS_ERROR                      // 错误状�??
} SystemState_t;

// 电机动作枚举
typedef enum {
    MOTOR_ACT_STOP,                // 停止
    MOTOR_ACT_FORWARD,             // 前进
    MOTOR_TURN_LEFT,               // 原地左转
    MOTOR_TURN_RIGHT,              // 原地右转
    MOTOR_TURN_AROUND,             // 掉头(180�?)
    MOTOR_ADJUST_LEFT,             // 轻微左调
    MOTOR_ADJUST_RIGHT,            // 轻微右调
    MOTOR_CURVE_LEFT,              // 大曲线左�?
    MOTOR_CURVE_RIGHT              // 大曲线右�?
} MotorAction_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_CROSS_RECORDS     20    // �?大路口记录数
#define TURN_DELAY_MS         2000  // 90度原地转向的延迟时间(毫秒)
#define AROUND_DELAY_MS       4000  // 180度掉头的延迟时间(毫秒)
#define SPEED_FORWARD         150.0f // 前进速度
#define SPEED_ADJUST_FACTOR   1.2f  // 轻微调整的�?�度系数
#define SPEED_CURVE_FACTOR    0.3f  // 大曲线转向�?�度系数(例如：内侧车30%速度)
#define PI_UART               &huart2 // 与树莓派通信的UART
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// UART环形缓冲�?
RingBuffer usart1_rx_buffer = {0};
RingBuffer usart2_rx_buffer = {0};
RingBuffer usart3_rx_buffer = {0};

// 系统状�??
volatile SystemState_t system_state = SYS_INIT;
volatile uint8_t medicine_taken = 0;          // 药品已放置标�?
volatile uint8_t sensor_state_8 = 0;          // 8位传感器状�??
volatile uint8_t In_Turn = 0;                 // 正在转向标志

// 返程路径记录
uint8_t cross_go[MAX_CROSS_RECORDS];          // 去程路口记录
uint8_t cross_cnt_go = 0;                     // 去程路口计数
uint8_t GO_or_BACK = 0;                       // 0-去程, 1-返程

// 通信缓冲�?
uint16_t track_pkt_len = 0;                   // 新传感器数据标志
extern uint8_t frame_buffer[];                // usart_handler.c中定�?

bool action_initialized = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void SystemClock_Config(void);
void System_Init(void);
void Process_Communication(void);
void Line_Tracking_Task(void);
void Execute_Line_Tracking(uint8_t sensor_state_8);
void Handle_Cross_Road(void);
void  Motor_Action(MotorAction_t action);
void Handle_Arrival(void);
void Execute_Cross_Action(uint8_t action);
void Controlled_Delay(uint32_t delay_ms);
void Send_Status_Message(const char* message);
void System_Status_Monitor(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM8_Init();
  /* USER CODE BEGIN 2 */
System_Init();
Pi_Start();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
printf("\xBB智能送药小车启动...\r\n\x55\x44\x33");
    
    // 1. 等待药品装载
system_state = SYS_WAITING_MEDICINE;
printf("\xBB等待药品装载...\r\n\x55\x44\x33");
    while(medicine_taken == 0) {
        // 该标志可通过按键中断或命令设�?
        HAL_Delay(10);
        }   
medicine_taken = 0;

printf("\xBB开始巡线\r\n\x55\x44\x33");

system_state = SYS_LINE_TRACKING;
    while (1)
    {
    /* USER CODE END WHILE */
  //#ifdef DEBUG_MODE
    //System_Status_Monitor();
    //#endif
    /* USER CODE BEGIN 3 */
        
    // 1. 统一调用电机控制任务

    Motor_Control_Task(); // 保持电机控制
    // 2. 处理传入的传感器数据和其他通信数据
    Process_Communication();
    
    // 3. 主状态机
    switch(system_state) {
        case SYS_LINE_TRACKING:
            Line_Tracking_Task();
            break;
            
        case SYS_CROSS_PROCESSING:
            Handle_Cross_Road();
            break;
            
        case SYS_ARRIVED:
            Handle_Arrival();
            break;
            
        case SYS_MISSION_COMPLETE:
            // 任务完成状态，保持停止
            printf("\xBB任务完成，等待新命令...\r\n\x55\x44\x33");
            Car_Stop_Precise();
            break;

        case SYS_ERROR:
            printf("\xBB系统错误! 停止运行。\r\n\x55\x44\x33");
            Car_Stop_Precise();
            while(1); // 停机
            break;
            
        default:
            break;
    }

        HAL_Delay(10); // 主循环延时
    }
  
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
  * @brief  系统初始�?
  */
void System_Init(void)
{
    RingBuffer_Init(&usart1_rx_buffer);
    RingBuffer_Init(&usart2_rx_buffer);
    RingBuffer_Init(&usart3_rx_buffer);
    
    HAL_UART_Receive_IT(&huart1, &usart1_rx_buffer.buffer[usart1_rx_buffer.head], 1);
    HAL_UART_Receive_IT(&huart2, &usart2_rx_buffer.buffer[usart2_rx_buffer.head], 1);
    HAL_UART_Receive_IT(&huart3, &usart3_rx_buffer.buffer[usart3_rx_buffer.head], 1);
    
    Motor_Init();
    Motor_PID_Init();
    printf("\xBB[电机] 初始化完成\r\n\x55\x44\x33");
    Car_Stop();
    
    system_state = SYS_INIT;
    GO_or_BACK = 0;
    cross_cnt_go = 0;
    In_Turn = 0;
}

/**
  * @brief  处理来自传感器和树莓派的通信数据
  */
void Process_Communication(void)
{
    // �?查UART2的新数据(传感器数据或树莓派命�?)
    static char rx_track_buf[MAX_FRAME_SIZE];
    uint16_t len = GetPacket(PI_UART, &usart2_rx_buffer, rx_track_buf);
    
    if(len > 0) {
        // �?查是否是特殊命令
        char command = rx_track_buf[1];
        
        // �?查目的地到达信号
        if (command == 'f' || command == 'e') {
            printf("\xBB接收到目的地信号: '%c'\r\n\x55\x44\x33", command);
            system_state = SYS_ARRIVED;
            return;
        }
        
        // �?查路口信�?
        if (command == 'c') { // 假设'c'表示�?测到路口
            printf("\xBB接收到路口信号\r\n\x55\x44\x33");
            system_state = SYS_CROSS_PROCESSING;
            return;
        }
        
        // 如果处于循线模式，这是传感器数据
        if (system_state == SYS_LINE_TRACKING) {
            sensor_state_8 = rx_track_buf[1]; // 假设数据在第二个字节
            track_pkt_len = len; // 为Line_Tracking_Task设置标志
        }
    }

    // 其他通信处理可以放在这里(例如：蓝牙调试UART)
    USART1_ProcessReceivedData();
}

/**
  * @brief 主循线任务，从主循环调用
  */
void Line_Tracking_Task(void)
{
    // 仅当有新数据且不在转向时处理新传感器数据
    if(track_pkt_len && !In_Turn) {
        track_pkt_len = 0; // 消费数据�?
        Execute_Line_Tracking(sensor_state_8);
    }
}

/**
  * @brief 基于8传感器状态执行循线�?�辑
  * 这是运动的核心决策函�?
  * @param sensor_state_8: 来自传感器阵列的8位状�?
  */
void Execute_Line_Tracking(uint8_t sensor_state_8)
{
    // 标准循线逻辑
    switch(sensor_state_8) {
        case STATE_STRAIGHT:
             Motor_Action(MOTOR_ACT_FORWARD);
            break;
            
        case STATE_ADJUST_LEFT_1:
        case STATE_ADJUST_LEFT_2:
             Motor_Action(MOTOR_ADJUST_LEFT);
            break;
            
        case STATE_ADJUST_RIGHT_1:
        case STATE_ADJUST_RIGHT_2:
             Motor_Action(MOTOR_ADJUST_RIGHT);
            break;
            
        case STATE_CURVE_LEFT:
             Motor_Action(MOTOR_CURVE_LEFT);
            break;
            
        case STATE_CURVE_RIGHT:
             Motor_Action(MOTOR_CURVE_RIGHT);
            break;
            
        default:
            // 如果处于未知状�?�，作为 fallback 继续直行
             Motor_Action(MOTOR_ACT_FORWARD);
            break;
    }
}

/**
  * @brief 处理路口逻辑
  */
void Handle_Cross_Road(void)
{
    Motor_Action(MOTOR_ACT_STOP);
    printf("\xBB检测到路口。\r\n\x55\x44\x33");
    printf("\xBB等待来自树莓派的转向命令(w/a/d)...\r\n\x55\x44\x33");
    
    char action = 0;
    static char rx_buf[5] = {0};
    uint16_t len = 0;

    // 等待来自树莓派的命令
    while(len == 0) {
        len = GetPacket(PI_UART, &usart2_rx_buffer, rx_buf);
        HAL_Delay(10);
    }
    action = rx_buf[1]; // 命令在第二个字节
    
    printf("\xBB收到命令: '%c'\r\n\x55\x44\x33", action);
    
    // 如果是去程，记录转向以便返程
    if(GO_or_BACK == 0 && cross_cnt_go < MAX_CROSS_RECORDS) {
        cross_go[cross_cnt_go] = action;
        cross_cnt_go++;
    }
    
    In_Turn = 1; // 设置转向标志
    
    // 重置动作初始化标志
    action_initialized = 0;
    
    // 执行转向动作，直到完成
    while (In_Turn) {
        // 如果动作尚未初始化，进行初始化
        if (!action_initialized) {
            Execute_Cross_Action(action);
            action_initialized = 1;
        }
        
        // 持续执行控制任务
        Motor_Control_Task();
        HAL_Delay(10);
    }
    // 恢复循线
        system_state = SYS_LINE_TRACKING;
}

/**
  * @brief 处理到达病房或药房的逻辑
  */
void Handle_Arrival(void)
{
    Motor_Action(MOTOR_ACT_STOP);
    printf("\xBB到达目的地，等待药品操作...\r\n\x55\x44\x33");

    char command = 0;
    static char rx_buf[5] = {0};
    uint16_t len = 0;

    // 等待来自树莓派的命令('f'�?'e')
    while(command != 'f' && command != 'e') {
        len = GetPacket(PI_UART, &usart2_rx_buffer, rx_buf);
        if (len > 0) {
            command = rx_buf[1];
        }
        HAL_Delay(10);
    }

    printf("\xBB收到命令: '%c'\r\n\x55\x44\x33", command);

    if (command == 'f' && GO_or_BACK == 0) { // 到达病房
        printf("\xBB到达病房，等待取药\r\n\x55\x44\x33");
        HAL_GPIO_WritePin(RED_LED_PIN_GPIO_Port, RED_LED_PIN_Pin, GPIO_PIN_SET);

        Motor_Action(MOTOR_ACT_STOP);

        while(medicine_taken == 0) {
            HAL_Delay(1000); // 等待按键按下
        }
        medicine_taken = 0;
        HAL_GPIO_WritePin(RED_LED_PIN_GPIO_Port, RED_LED_PIN_Pin, GPIO_PIN_RESET);
        
        printf("\xBBstart go back\r\n\x55\x44\x33");
        
        // 使用修复后的掉头函数
        In_Turn = 1;
        if(In_Turn) {
            Motor_Action(MOTOR_TURN_AROUND);
        }

        printf("\xBBgo back\r\n\x55\x44\x33");
        Send_Packet(PI_UART, "b", 1);

        GO_or_BACK = 1; // 设置为返程模式
        system_state = SYS_LINE_TRACKING; // 恢复循线
        
    } 
    else if (command == 'e' && GO_or_BACK == 1) { // 返回药房
        printf("\xBB返回药房。任务完成�?�\r\n\x55\x44\x33");
        HAL_GPIO_WritePin(GREEN_LED_PIN_GPIO_Port, GREEN_LED_PIN_Pin, GPIO_PIN_SET);
        system_state = SYS_MISSION_COMPLETE;
    } 
    else {
        printf("\xBB错误命令或状态不匹配\r\n\x55\x44\x33");
        system_state = SYS_ERROR;
    }
}

/**
  * @brief  执行特定的电机动�?
  * @param action: 要执行的电机动作
  */
void Motor_Action(MotorAction_t action)
{ 
    switch(action) {
        case MOTOR_ACT_STOP:
            printf("\xBB[电机] 停止\r\n\x55\x44\x33");
            Car_Stop_Precise();
            In_Turn = 0; // 停止时清除转向标志
            break;
            
        case MOTOR_ACT_FORWARD:
        printf("\xBB[电机] 前进\r\n\x55\x44\x33");
            Car_Go(SPEED_FORWARD, -SPEED_FORWARD);
            In_Turn = 0;
            break;
            
        case MOTOR_ADJUST_LEFT:
        printf("\xBB[电机] 轻微左调\r\n\x55\x44\x33");
            Car_Go(SPEED_FORWARD - 5, -(SPEED_FORWARD - 5));
            In_Turn = 0;
            break;
            
        case MOTOR_ADJUST_RIGHT:
        printf("\xBB[电机] 轻微右调\r\n\x55\x44\x33");
            Car_Go(SPEED_FORWARD + 5, -(SPEED_FORWARD + 5));
            In_Turn = 0;
            break;
            
        case MOTOR_CURVE_LEFT:
        printf("\xBB[电机] 大曲线左转\r\n\x55\x44\x33");
            Car_Go(SPEED_FORWARD - 10, -(SPEED_FORWARD - 10));
            In_Turn = 0;
            break;
            
        case MOTOR_CURVE_RIGHT:
        printf("\xBB[电机] 大曲线右转\r\n\x55\x44\x33");
            Car_Go(SPEED_FORWARD + 10, -(SPEED_FORWARD + 10));
            In_Turn = 0;
            break;
            
        case MOTOR_TURN_LEFT:
        printf("\xBB[电机] 原地左转\r\n\x55\x44\x33");
            // 直接调用转向函数
            Car_Straight_Then_Turn_Left_90_And_Stop();
            // 检查转向是否完成
            if (Is_Motion_Complete()) {
                In_Turn = 0; // 清除转向标志
            }
            break;
            
        // case MOTOR_TURN_RIGHT:
        //     // 直接调用转向函数
        //     printf("\xBB[电机] 原地右转\r\n\x55\x44\x33");
        //     Car_Straight_Then_Turn_Right_90_And_Stop();
        //     // 检查转向是否完成
        //     if (Is_Motion_Complete()) {
        //         In_Turn = 0; // 清除转向标志
        //     }
        //     break;
            
        case MOTOR_TURN_RIGHT:
            printf("\xBB[电机] 原地右转\r\n\x55\x44\x33");
            Car_Straight_Then_Turn_Right_90_And_Stop();
            if (Is_Motion_Complete()) {
                In_Turn = 0; // 清除转向标志
                Car_Stop_Precise(); // 确保停止
                printf("\xBB[电机] 右转完成\r\n\x55\x44\x33");
            }
            break;
        
        // case MOTOR_TURN_RIGHT:
        // printf("\xBB[电机] 原地右转\r\n\x55\x44\x33");
        // while (!Is_Motion_Complete()) {
        //     Car_Straight_Then_Turn_Right_90_And_Stop();
        //     Motor_Control_Task(); // 保持电机控制
        //     HAL_Delay(10);
        // }
            In_Turn = 0;
            break;    
        case MOTOR_TURN_AROUND:
            // 直接调用转向函数
            printf("\xBB[电机] 掉头\r\n\x55\x44\x33");
            Car_Turn_Around_And_Stop();
            // 检查转向是否完成
            if (Is_Motion_Complete()) {
                In_Turn = 0; // 清除转向标志
            }
            break;
            
        default:
            printf("\xBB[电机] 未知动作，停止\r\n\x55\x44\x33");
            Car_Stop();
            In_Turn = 0;
            break;
    }
}

/**
  * @brief  在路口执行定时原地转�?
  * @param action: 转向方向字符('w', 'a', 'd')
  */
void Execute_Cross_Action(uint8_t action)
{
     switch(action) {            
        case 'a': // 左转
            printf("\xBB[电机] 左转90度\r\n\x55\x44\x33");
            Motor_Action(MOTOR_TURN_LEFT);
            break;
            
        case 'd': // 右转
            printf("\xBB[电机] 右转90度\r\n\x55\x44\x33");
            Motor_Action(MOTOR_TURN_RIGHT);
            break;
            
        default:
        case 'w': // 直行
            printf("\xBB[电机] 直行通过路口\r\n\x55\x44\x33");
            Motor_Action(MOTOR_ACT_FORWARD); // 直行通过
            HAL_Delay(500); // 直行一小段时间
            In_Turn = 0; // 结束转向
            break;
    }
}

/**
  * @brief  通过调试UART发送状态消息
  * @param message: 要发送的字符串消息
  */
void Send_Status_Message(const char* message)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), 100);
}

/**
  * @brief  系统状�?�监控和诊断
  */
void System_Status_Monitor(void)
{
    static uint32_t last_status_time = 0;
    uint32_t current_time = HAL_GetTick();
    
    // �?5秒输出一次状态信�?
    if (current_time - last_status_time > 5000) {
        printf("\xBB===================\r\n\x55\x44\x33");
        printf("\xBB System_state %d\r\n\x55\x44\x33", system_state);
        printf("\xBB Sensor_state %02X\r\n\x55\x44\x33", sensor_state_8);
        printf("\xBB In_Turn: %d\r\n\x55\x44\x33", In_Turn);
        printf("\xBB Medicine_taken: %d\r\n\x55\x44\x33", medicine_taken);
        printf("\xBB Cross_Num: %d\r\n\x55\x44\x33", cross_cnt_go);
        printf("\xBB Left_motor.current_speed: %.2f RPM\r\n\x55\x44\x33", left_motor.current_speed);
        printf("\xBB Right_motor.current_speed: %.2f RPM\r\n\x55\x44\x33", right_motor.current_speed);
        printf("\xBB===================\r\n\x55\x44\x33");
        
        last_status_time = current_time;
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    __disable_irq();
    while (1) {
    }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* 用户可以添加自己的文件和行号处理逻辑 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
