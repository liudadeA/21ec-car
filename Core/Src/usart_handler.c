#include "usart_handler.h"
#include <string.h>
#include <stdio.h>
#include "ring_buffer.h"
#include "main.h"

// UART1帧解析状态变??
static FrameState frame1_state = FRAME_WAIT_HEADER;
static uint8_t frame1_buffer[MAX_FRAME_SIZE] = {0};
static uint16_t frame1_index = 0;

// UART2帧解析状态变??
static FrameState frame2_state;
static uint8_t frame2_buffer[MAX_FRAME_SIZE];
static uint16_t frame2_index;


// USART3蓝牙真解析状态
static FrameState bt_frame_state = FRAME_WAIT_HEADER;
static uint16_t bt_frame_index = 0;
static uint8_t bt_frame_buffer[MAX_BT_FRAME_SIZE];

// 定义包头包尾
const uint8_t g_header[] = {0xAA};
const uint8_t g_footer[] = {0x55, 0x44,0x33}; // \x55\r\n


extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

/*****************************************************************************
@brief: 函数测试USART1作为Debug串口，两个功能：(没有中断，都是接收完数据后调用)
#1. 测试：电脑发送： usart1 单片机回复： usart1ok\r\n --->>>> 收发正常
#2. static frame1_buffer 提取RingBuffer的第一个完整帧
@attention: 发送的数据包只能用一个长度，每次发送一个帧
******************************************************************************/
void USART1_ProcessReceivedData(void) {
    while (usart1_rx_buffer.count > 0) {
        uint8_t ch = RingBuffer_Read(&usart1_rx_buffer);
        
        switch (frame1_state) {
            case FRAME_WAIT_HEADER:
                if (ch == g_header[0]) {
                    frame1_state = FRAME_IN_DATA;
                    frame1_index = 0;  // 重置帧缓冲区索引
                }
                break;
                
            case FRAME_IN_DATA:
                // 检查是否开始包尾
                if (ch == g_footer[0]) {
                    frame1_state = FRAME_WAIT_FOOTER1;
                } 
                // 检查缓冲区是否溢出
                else if (frame1_index < MAX_FRAME_SIZE - 1) {
                    frame1_buffer[frame1_index++] = ch;
                } else {
                    // 帧过长，重置状态机
                    frame1_state = FRAME_WAIT_HEADER;
                }
                break;
                
            // -------------------- 修正核心区域 --------------------
            case FRAME_WAIT_FOOTER1:
                if (ch == g_footer[1]) {
                    // 修正：使用 USART1 专属状态变量 frame1_state
                    frame1_state = FRAME_WAIT_FOOTER2;  
                } else {
                    // 包尾不匹配，丢弃当前帧并重置状态机
                    frame1_state = FRAME_WAIT_HEADER;    // 重置状态
                    frame1_index = 0;                    // 清空缓冲区索引
                }
                break;
                
            case FRAME_WAIT_FOOTER2:
                if (ch == g_footer[2]) {
                    // 修正：使用 USART1 专属状态变量 frame1_state
                    frame1_state = FRAME_WAIT_FOOTER3;  
                } else {
                    // 包尾不匹配，丢弃当前帧并重置状态机
                    frame1_state = FRAME_WAIT_HEADER;    // 重置状态
                    frame1_index = 0;                    // 清空缓冲区索引
                }
                break;
            // -------------------- 修正核心区域 --------------------
                
            case FRAME_WAIT_FOOTER3:
                // 完整帧接收完成
                frame1_buffer[frame1_index] = '\0';  // 添加字符串结束符
                
                // 比较接收到的数据 0为相等
                if (strcmp((char*)frame1_buffer, TEST_STRING) == 0) {
                    // 发送响应
                    uint8_t response[] = RESPONSE_STRING;
                    HAL_UART_Transmit(&huart1, response, sizeof(response) - 1, 50);
                }
                HAL_UART_Transmit(&huart1, frame1_buffer, sizeof(frame1_buffer) - 1, 50);
                // 重置状态机
                frame1_state = FRAME_WAIT_HEADER;
                frame1_index = 0;
                break;
        }
    }
}


/*****************************************************
@brief: 函数USART2，接受第一个完整的数据帧：
#1. 存储到static frame2_buffer中
#2. 正确的字符及其长度发送到usart1电脑串口中
******************************************************/
void USART2_ProcessReceivedData() {
    while (usart2_rx_buffer.count > 0) {
        uint8_t ch = RingBuffer_Read(&usart2_rx_buffer);

        switch (frame2_state) {
            case FRAME_WAIT_HEADER:
                if (ch == g_header[0]) {
                    frame2_state = FRAME_IN_DATA;
                    frame2_index = 0;
                    memset(frame2_buffer, 0, MAX_FRAME_SIZE);
                }
                break;

            case FRAME_IN_DATA:
                if (ch == g_footer[0]) {
                    frame2_state = FRAME_WAIT_FOOTER1;
                } else if (frame2_index < MAX_FRAME_SIZE - 1) {
                    frame2_buffer[frame2_index++] = ch;
                } else {
                    // 溢出，回到等待包头
                    frame2_state = FRAME_WAIT_HEADER;
                    frame2_index = 0;
                }
                break;

            case FRAME_WAIT_FOOTER1:
                if (ch == g_footer[1]) {
                    frame2_state = FRAME_WAIT_FOOTER2;
                } else {
                    // 包尾不匹配，回到数据区，且把前面包尾和当前字节都当作数据
                    if (frame2_index < MAX_FRAME_SIZE - 2) {
                        frame2_buffer[frame2_index++] = g_footer[0];
                        frame2_buffer[frame2_index++] = ch;
                        frame2_state = FRAME_IN_DATA;
                    } else {
                        frame2_state = FRAME_WAIT_HEADER;
                        frame2_index = 0;
                    }
                }
                break;

            case FRAME_WAIT_FOOTER2:
                if (ch == g_footer[2]) {
                    // 完整帧
                    frame2_buffer[frame2_index] = '\0';
									//返回完整帧的字符串和长度
                    HAL_UART_Transmit(&huart1, frame2_buffer, frame2_index, 50);
                    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 50);
                    frame2_state = FRAME_WAIT_HEADER;
                    frame2_index = 0;
                } else {
                    // 包尾不匹配，回到数据区
                    if (frame2_index < MAX_FRAME_SIZE - 3) {
                        frame2_buffer[frame2_index++] = g_footer[0];
                        frame2_buffer[frame2_index++] = g_footer[1];
                        frame2_buffer[frame2_index++] = ch;
                        frame2_state = FRAME_IN_DATA;
                    } else {
                        frame2_state = FRAME_WAIT_HEADER;
                        frame2_index = 0;
                    }
                }
                break;

            default:
                frame2_state = FRAME_WAIT_HEADER;
                frame2_index = 0;
                break;
        }
    }
}



/**********************************************************************
@brief: USART3作为蓝牙接收串口，此函数提取蓝牙接收到RingBuffer中的数据
#1. 每次只提取第一个完整的帧，通过多次调用可以多次提取，tail 读指针可以自动更新
#2. 每次提取到完整帧时会发送到电脑串口中
@attention:
#1. !!发送时一次也只能发送一帧；；；；多帧当发送速度过快时接收错误。
#2. 每次发送的长度要一致，不然那输出时会对之前的帧覆盖不完全:
aa 48 45 4c 4ac 55 0d 0a --->
aa 48 45 55 0d 0a
@parameters:
#1. rutern packet_len：帧长度
						0 ：没有完整帧或帧过长不提取
#2. out_buf: 提取帧的储存数组（输出缓冲区）
@example:发送aa 48 45 4c 4c 4f 55 0d 0a 循环发送
out_buf:aa 48 45 4c 4c 4f 55 0d 0a
***********************************************************************
*/
uint16_t BT_GetPacket(char* out_buf) 
{
    uint16_t packet_len = 0;
    
    while (usart3_rx_buffer.count > 0) {
        uint8_t ch = RingBuffer_Read(&usart3_rx_buffer);
        
        switch (bt_frame_state) {
            case FRAME_WAIT_HEADER:
                if (ch == *g_header) {
                    bt_frame_state = FRAME_IN_DATA;
                    bt_frame_index = 0;
                    bt_frame_buffer[bt_frame_index++] = ch;  // 存储帧头
                }
                break;
                
            case FRAME_IN_DATA:
                // 先检查缓冲区溢出
                if (bt_frame_index >= MAX_BT_FRAME_SIZE - 1) {
                    bt_frame_state = FRAME_WAIT_HEADER;  // 重置状态机
                    bt_frame_index = 0;
                    break;
                }
                
                bt_frame_buffer[bt_frame_index++] = ch;
                
                // 检查是否开始包尾
                if (ch == g_footer[0]) {
                    bt_frame_state = FRAME_WAIT_FOOTER1;
                }
                break;
                
            case FRAME_WAIT_FOOTER1:
                // 检查缓冲区溢出
                if (bt_frame_index >= MAX_BT_FRAME_SIZE - 1) {
                    bt_frame_state = FRAME_WAIT_HEADER;
                    bt_frame_index = 0;
                    break;
                }
                
                bt_frame_buffer[bt_frame_index++] = ch;
                
                if (ch == g_footer[1]) {
                    bt_frame_state = FRAME_WAIT_FOOTER2;
                } else {
                    bt_frame_state = FRAME_IN_DATA;  // 非完整帧尾
                }
                break;
                
            case FRAME_WAIT_FOOTER2:
                // 检查缓冲区溢出
                if (bt_frame_index >= MAX_BT_FRAME_SIZE - 1) {
                    bt_frame_state = FRAME_WAIT_HEADER;
                    bt_frame_index = 0;
                    break;
                }
                
                bt_frame_buffer[bt_frame_index++] = ch;
                
                if (ch == g_footer[2]) {
                    // 完整帧接收完成
                    bt_frame_state = FRAME_WAIT_FOOTER3;
                    
                    // 复制完整帧到输出缓冲区
                    memcpy(out_buf, bt_frame_buffer, bt_frame_index);
                    packet_len = bt_frame_index;
									
									//返回完整帧的字符串和长度
									  bt_frame_buffer[bt_frame_index] = '\0';
                    HAL_UART_Transmit(&huart1, bt_frame_buffer, bt_frame_index, 50);
                    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 50);
                    
									// 重置状态机
                    bt_frame_state = FRAME_WAIT_HEADER;
                    bt_frame_index = 0;
                    
                    return packet_len;  // 返回包长度
                } else {
                    bt_frame_state = FRAME_IN_DATA;  // 非完整帧尾
                }
                break;
                
            case FRAME_WAIT_FOOTER3:
                // 正常情况下不应进入此状态
                bt_frame_state = FRAME_WAIT_HEADER;
                bt_frame_index = 0;
                break;
        }
    }
    
    return 0;  // 无完整数据包
}




FrameState frame_state = FRAME_WAIT_HEADER;
uint16_t frame_index = 0;
uint8_t frame_buffer[MAX_FRAME_SIZE] = {0};

/***************************************************************************
@brief:接受模版：统一使用的获取数据包到输出缓冲区out_buf
@attention: 提前定义静态变量，不然响应速度太慢
***************************************************************************/
uint16_t GetPacket(UART_HandleTypeDef *huart, RingBuffer *usartx_rx_buffer, char* out_buf) 
{
    uint16_t packet_len = 0;
    
    while (usartx_rx_buffer->count > 0) {
        uint8_t ch = RingBuffer_Read(usartx_rx_buffer);
        
        switch (frame_state) {
            case FRAME_WAIT_HEADER:
                if (ch == *g_header) {
                    frame_state = FRAME_IN_DATA;
                    frame_index = 0;
                    frame_buffer[frame_index++] = ch;  // 存储帧头
                }
                break;
                
            case FRAME_IN_DATA:
                // 检查缓冲区溢出
                if (frame_index >= MAX_BT_FRAME_SIZE - 1) {
                    frame_state = FRAME_WAIT_HEADER;
                    frame_index = 0;
                    break;
                }
                
                frame_buffer[frame_index++] = ch;
                
                // 检查是否开始包尾
                if (ch == g_footer[0]) {
                    frame_state = FRAME_WAIT_FOOTER1;
                }
                break;
                
            case FRAME_WAIT_FOOTER1:
                if (frame_index >= MAX_BT_FRAME_SIZE - 1) {
                    frame_state = FRAME_WAIT_HEADER;
                    frame_index = 0;
                    break;
                }
                
                frame_buffer[frame_index++] = ch;
                
                if (ch == g_footer[1]) {
                    frame_state = FRAME_WAIT_FOOTER2;
                } else {
                    frame_state = FRAME_IN_DATA;
                }
                break;
                
            case FRAME_WAIT_FOOTER2:
                if (frame_index >= MAX_BT_FRAME_SIZE - 1) {
                    frame_state = FRAME_WAIT_HEADER;
                    frame_index = 0;
                    break;
                }
                
                frame_buffer[frame_index++] = ch;
                
                if (ch == g_footer[2]) {
                    frame_state = FRAME_WAIT_HEADER;  // 直接重置状态机
                    
                    // 复制完整帧到输出缓冲区
                    memcpy(out_buf, frame_buffer, frame_index);
                    packet_len = frame_index;
                    
                    // 添加字符串终止符并打印到USART1
                    out_buf[frame_index] = '\0';
                    HAL_UART_Transmit(&huart1, frame_buffer, frame_index, 50);
                    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 50);
                    
                    // 重置索引并返回包长度
                    frame_index = 0;
                    return packet_len;
                } else {
                    frame_state = FRAME_IN_DATA;
                }
                break;
                
            case FRAME_WAIT_FOOTER3:
                // 该状态已被简化处理
                frame_state = FRAME_WAIT_HEADER;
                frame_index = 0;
                break;
        }
    }
    
    return 0;  // 无完整数据包
}

/*********************************************************
@brief: 串口通用发送数据包
**********************************************************/
void Send_Packet(UART_HandleTypeDef *huart,const char* data,uint16_t len)
{
	    // 发送帧头
    HAL_UART_Transmit(huart, (uint8_t*)g_header, 1, HAL_MAX_DELAY);
    
    // 发送数据
    HAL_UART_Transmit(huart, (uint8_t*)data, len, HAL_MAX_DELAY);
    
    // 发送帧尾
    HAL_UART_Transmit(huart, (uint8_t*)g_footer, 3, HAL_MAX_DELAY);
}
// 蓝牙发送函数（复用通用发送）
void BT_SendPacket(const char* data, uint16_t len) {
    Send_Packet(&huart3, data, len);
}

// 树莓派启动指令发送（复用通用发送）
void Pi_Start() {
    Send_Packet(&huart2, "r", 1); // 发送"r"指令
}
