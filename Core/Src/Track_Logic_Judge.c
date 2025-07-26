#include "Track_Logic_Judge.h"
#include <stdio.h>
#include <string.h>

/*可以不写，已经在gpio.c的配置中了*/
//模块初始化
//void TrackModule_Init(void) {
//    // 初始化GPIO时钟
//  GPIO_InitTypeDef GPIO_InitStruct = {0};

//  /* GPIO Ports Clock Enable */
//  __HAL_RCC_GPIOE_CLK_ENABLE();

//  /*Configure GPIO pins : PE8 PE10 PE12 PE15 */
//  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_10|GPIO_PIN_12|GPIO_PIN_15;
//  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
//  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
//}

/**********************************************************
@attention:传感器数据采集，main函数中不使用
!!!在Track_Logic_Judge.h修改采样间隔 ms
***********************************************************/
SensorState Track_ReadSensors(void) {
    SensorState state;
    
    // 读取传感器状态（带软件滤波）
    for(int i = 0; i < 3; i++) {
        state.DH1 = HAL_GPIO_ReadPin(DH_GPIO, DH1_Pin);
        state.DH2 = HAL_GPIO_ReadPin(DH_GPIO, DH2_Pin);
        state.DH3 = HAL_GPIO_ReadPin(DH_GPIO, DH3_Pin);
        state.DH4 = HAL_GPIO_ReadPin(DH_GPIO, DH4_Pin);
        HAL_Delay(Sensors_Delay);
    }
    
    return state;
}

// 巡线控制逻辑
ControlOutput Track_Process(const SensorState* state, TrackParams* params) {
    static int last_state = 0;
    static int ten_time = 0;
    ControlOutput output = {0, 0};
    
    // 组合传感器状态（DH1为最高位）
    int DH_state = (state->DH1 << 3) | (state->DH2 << 2) | 
                  (state->DH3 << 1) | state->DH4;

    switch (DH_state) {
        case 0: // 0000 十字路口
            ten_time++;
            if (ten_time < 1000) { // 停止2秒
                output.Move_X = 0;
                output.Move_Z = 0;
            } else if (ten_time >= 1000) { // 直行通过
                output.Move_X = params->baseSpeed;
                output.Move_Z = 0;
            }
            last_state = 1;
            break;

        case 1: // 0001 左直角弯
            ten_time = 0;
            output.Move_X = params->baseSpeed * 0.3f;
            output.Move_Z = params->Turn90Angle;
            last_state = 2;
            break;

        case 8: // 1000 右直角弯
            ten_time = 0;
            output.Move_X = params->baseSpeed * 0.3f;
            output.Move_Z = -params->Turn90Angle;
            last_state = 3;
            break;

        case 7: // 0111 左大弯
            ten_time = 0;
            output.Move_X = params->baseSpeed * 0.7f;
            output.Move_Z = params->maxTurnAngle;
            last_state = 4;
            break;

        case 14: // 1110 右大弯
            ten_time = 0;
            output.Move_X = params->baseSpeed * 0.7f;
            output.Move_Z = -params->maxTurnAngle;
            last_state = 5;
            break;

        case 11: // 1011 左微调
            ten_time = 0;
            output.Move_X = params->baseSpeed * 0.8f;
            output.Move_Z = params->minTurnAngle;
            last_state = 6;
            break;

        case 13: // 1101 右微调
            ten_time = 0;
            output.Move_X = params->baseSpeed * 0.8f;
            output.Move_Z = -params->minTurnAngle;
            last_state = 7;
            break;

        case 9: // 1001 直行
            ten_time = 0;
            output.Move_X = params->baseSpeed;
            output.Move_Z = 0;
            last_state = 8;
            break;

        case 15: // 1111 丢线情况
            ten_time = 0;
            if(last_state == 4 || last_state == 6) {
                output.Move_X = params->baseSpeed * 0.8f;
                output.Move_Z = params->midTurnAngle;
            }
            if(last_state == 5 || last_state == 7) {
                output.Move_X = params->baseSpeed * 0.8f;
                output.Move_Z = -params->midTurnAngle;
            }
            break;
            
        default:
            // 未知状态保持上次输出
            break;
    }
    
    return output;
}


/***************************************************************
@brief:巡线测试:红外传感器输出+状态，main中直接调用,使用GPIO读取数据
@example: 0000 十字路口
@attention: 无输出，也无引用，要使用数据要添加buffer
***************************************************************/ 
void Track_DebugMode(UART_HandleTypeDef* huart) {
    char buffer[80];
    
    // 1. 读取传感器数据
    SensorState sensorState = Track_ReadSensors();
    
    // 2. 串口输出状态
    snprintf(buffer, sizeof(buffer), 
            "Sensors: %d%d%d%d\r\n",
            sensorState.DH1, sensorState.DH2, 
            sensorState.DH3, sensorState.DH4);
    
    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
    
    // 4. 添加详细传感器状态解释（文档3.1节相关）
    const char* stateDescription = "";
    int DH_state = (sensorState.DH1 << 3) | (sensorState.DH2 << 2) | 
                  (sensorState.DH3 << 1) | sensorState.DH4;
    
    switch (DH_state) {
        case 0: stateDescription = "十字路口"; break;
        case 1: stateDescription = "左直角弯"; break;
        case 8: stateDescription = "右直角弯"; break;
        case 7: stateDescription = "左大弯"; break;
        case 14: stateDescription = "右大弯"; break;
        case 11: stateDescription = "左微调"; break;
        case 13: stateDescription = "右微调"; break;
        case 9: stateDescription = "直行"; break;
        case 15: stateDescription = "丢线"; break;
        default: stateDescription = "未知状态";
    }
    
    snprintf(buffer, sizeof(buffer), "当前状态: %s (二进制: %d%d%d%d)\r\n",
            stateDescription, sensorState.DH1, sensorState.DH2, 
            sensorState.DH3, sensorState.DH4);
    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
    
//    // 5. 添加电位器调节建议（文档2.2节相关）
//    if (sensorState.DH1 == 1 && sensorState.DH2 == 1 && 
//        sensorState.DH3 == 1 && sensorState.DH4 == 1) {
//        HAL_UART_Transmit(huart, (uint8_t*)"[建议] 所有传感器未检测到线路，请检查电位器调节或模块高度\r\n", 65, 100);
//    }
}
