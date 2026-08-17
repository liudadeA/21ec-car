// 姿态角数据结构
typedef struct {
    float roll;
    float pitch;
    float yaw;
} AttitudeData;

// 陀螺仪和加速度数据结构
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} GyroAccelData;

#define MAX_FRAME_LEN 32  // 最大帧长度
#define FRAME_TIMEOUT_MS 100 // 帧超时时间

void UART4_RxHandler(uint8_t byte);
uint8_t uart4_rx_byte;                 // 单字节接收变量
#define MAX_LEN 50
uint8_t rx_buffer[MAX_LEN];      // 数据帧缓冲
uint8_t indexx = 0;
uint8_t data_len = 0;
uint8_t state = 0;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART4) {
        UART4_RxHandler(uart4_rx_byte);
        HAL_UART_Receive_IT(&huart4, &uart4_rx_byte, 1);
    }
}
uint8_t rx_byte;
uint32_t last_rx_time = 0; // 最后接收时间

void SystemClock_Config(void);

void ProcessFrame(uint8_t* data, uint8_t len);
void ProcessAttitudeFrame(uint8_t* data);
void ProcessQuaternionFrame(uint8_t* data);
void ProcessGyroAccelFrame(uint8_t* data);
void ProcessPortStatusFrame(uint8_t* data);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart4, &uart4_rx_byte, 1);  // 启动中断接收
  printf("System Start\r\n");
  /* USER CODE END 2 */

/* USER CODE BEGIN 4 */
void UART4_RxHandler(uint8_t byte)
{
    last_rx_time = HAL_GetTick();

    switch (state) {
        case 0:
            if (byte == 0x55) {
                rx_buffer[0] = byte;
                indexx = 1;
                state = 1;
            }
            break;
        case 1:
            if (byte == 0x55) {
                rx_buffer[1] = byte;
                indexx = 2;
                state = 2;
            } else if (byte == 0x55) {
                rx_buffer[0] = 0x55;
                indexx = 1;
                state = 1;
            } else {
                state = 0;
                indexx = 0;
            }
            break;
        case 2:
            if (byte == 0x01 || byte == 0x02 || byte == 0x03 || byte == 0x06 || byte == 0x0D) {
                rx_buffer[2] = byte;
                indexx = 3;
                state = 3;
            } else {
                printf("[Error] Invalid ID: 0x%02X\r\n", byte);
                state = 0;
                indexx = 0;
            }
            break;
        case 3:
            data_len = byte;
            rx_buffer[3] = byte;
            if (data_len < 1 || data_len > (MAX_FRAME_LEN - 5)) {
                printf("[Error] Invalid Len: %d\r\n", data_len);
                state = 0;
                indexx = 0;
            } else {
                indexx = 4;
                state = 4;
            }
            break;
        case 4:
            if (indexx < MAX_FRAME_LEN) {
                rx_buffer[indexx++] = byte;
            }

            if (indexx >= 4 + data_len + 1) {
                ProcessFrame(rx_buffer, indexx);
                state = 0;
                indexx = 0;
            }
            break;
        default:
            state = 0;
            indexx = 0;
            break;
    }
}
void ProcessFrame(uint8_t* data, uint8_t len)
{
    uint8_t checksum = 0;
    for (int i = 0; i < len - 1; i++) {
        checksum += data[i];
    }

    uint8_t recv_checksum = data[len - 1];
    uint8_t frame_id = data[2];
    uint8_t frame_len = data[3];

    if (checksum != recv_checksum) {
        printf("[Checksum Error] ID=0x%02X, Calc=0x%02X, Recv=0x%02X\r\n",
               frame_id, checksum, recv_checksum);
        return;
    }

    printf("[Frame] ID=0x%02X, Len=%d\r\n", frame_id, frame_len);

    switch (frame_id) {
        case 0x01:
            ProcessAttitudeFrame(data);
            break;
        case 0x02:
            ProcessQuaternionFrame(data);
            break;
        case 0x03:
            ProcessGyroAccelFrame(data);
            break;
        case 0x06:
            ProcessPortStatusFrame(data);
            break;
        case 0x0D:
            printf("[Info] Module Info Frame Received\r\n");
            break;
        default:
            printf("[Error] Unknown Frame ID: 0x%02X\r\n", frame_id);
            break;
    }
}

/* ========================== 各类帧数据处理函数 ========================== */

// 姿态角帧
void ProcessAttitudeFrame(uint8_t* data)
{
        // 验证数据长度（必须为6字节）
    if (data[3] != 6) {
        printf("[Error] Attitude frame length error: %d (expected 6)\r\n", data[3]);
        return;
    }

    // 提取原始数据（小端模式）
    int16_t raw_roll  = (int16_t)((data[5] << 8) | data[4]);  // RollH + RollL
    int16_t raw_pitch = (int16_t)((data[7] << 8) | data[6]); // PitchH + PitchL
    int16_t raw_yaw   = (int16_t)((data[9] << 8) | data[8]); // YawH + YawL

    // 转换为实际角度值（单位：度）
    float roll  = (float)raw_roll  / 32768.0f * 180.0f;
    float pitch = (float)raw_pitch / 32768.0f * 180.0f;
    float yaw   = (float)raw_yaw   / 32768.0f * 180.0f;

    // 打印结果
    printf("[Attitude] Roll=%.2f°, Pitch=%.2f°, Yaw=%.2f°\r\n", roll, pitch, yaw);
}

// 四元数帧
void ProcessQuaternionFrame(uint8_t* data)
{
    if (data[3] != 8) return;

    int16_t q0 = (int16_t)((data[5] << 8) | data[4]);
    int16_t q1 = (int16_t)((data[7] << 8) | data[6]);
    int16_t q2 = (int16_t)((data[9] << 8) | data[8]);
    int16_t q3 = (int16_t)((data[11] << 8) | data[10]);

    printf("[Quaternion] q0=%.4f, q1=%.4f, q2=%.4f, q3=%.4f\r\n",
           q0 / 32768.0f, q1 / 32768.0f, q2 / 32768.0f, q3 / 32768.0f);
}

// 加速度/陀螺仪帧
void ProcessGyroAccelFrame(uint8_t* data)
{
    if (data[3] != 12) return;

    int16_t ax = (int16_t)((data[5] << 8) | data[4]);
    int16_t ay = (int16_t)((data[7] << 8) | data[6]);
    int16_t az = (int16_t)((data[9] << 8) | data[8]);
    int16_t gx = (int16_t)((data[11] << 8) | data[10]);
    int16_t gy = (int16_t)((data[13] << 8) | data[12]);
    int16_t gz = (int16_t)((data[15] << 8) | data[14]);

    printf("[Accel] X=%d, Y=%d, Z=%d | [Gyro] X=%d, Y=%d, Z=%d\r\n",
           ax, ay, az, gx, gy, gz);
}

// 端口状态帧
void ProcessPortStatusFrame(uint8_t* data)
{
    if (data[3] != 8) return;

    uint16_t d0 = (data[5] << 8) | data[4];
    uint16_t d1 = (data[7] << 8) | data[6];
    uint16_t d2 = (data[9] << 8) | data[8];
    uint16_t d3 = (data[11] << 8) | data[10];

    printf("[PortStatus] D0=%d, D1=%d, D2=%d, D3=%d\r\n", d0, d1, d2, d3);
}
/* USER CODE END 4 */
