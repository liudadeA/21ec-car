/* USER CODE BEGIN PTD */
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

// 卡尔曼滤波器结构体
typedef struct {
    float q; // 过程噪声协方差
    float r; // 测量噪声协方差
    float x; // 估计值
    float p; // 估计误差协方差
    float k; // 卡尔曼增益
} KalmanFilter;
/* USER CODE END PTD */

/* USER CODE BEGIN PD */
#define MAX_FRAME_LEN 32  // 最大帧长度
#define FRAME_TIMEOUT_MS 100 // 帧超时时间
/* USER CODE END PD */

/* USER CODE BEGIN PV */
// 初始化卡尔曼滤波器
void KalmanInit(KalmanFilter* kf, float q, float r, float initial_value) {
    kf->q = q;
    kf->r = r;
    kf->x = initial_value;
    kf->p = 1.0f; // 初始估计误差协方差
}

// 卡尔曼滤波器更新
float KalmanUpdate(KalmanFilter* kf, float measurement) {
    // 预测
    kf->p = kf->p + kf->q;
    
    // 更新
    kf->k = kf->p / (kf->p + kf->r);
    kf->x = kf->x + kf->k * (measurement - kf->x);
    kf->p = (1 - kf->k) * kf->p;
    
    return kf->x;
}

// 为每个轴创建卡尔曼滤波器
KalmanFilter kalman_roll, kalman_pitch, kalman_yaw;
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
uint8_t frame_counter = 0;  // 帧计数器
/* USER CODE END PV */

/* USER CODE BEGIN PFP */

void ProcessFrame(uint8_t* data, uint8_t len);
void ProcessAttitudeFrame(uint8_t* data);
void ProcessQuaternionFrame(uint8_t* data);
void ProcessGyroAccelFrame(uint8_t* data);
void ProcessPortStatusFrame(uint8_t* data);
/* USER CODE END PFP */

/* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart4, &uart4_rx_byte, 1);  // 启动中断接收
  printf("\r\n\r\nSystem Start\r\n");
KalmanInit(&kalman_roll, 0.01, 0.1, 0.0);
KalmanInit(&kalman_pitch, 0.01, 0.1, 0.0);
KalmanInit(&kalman_yaw, 0.01, 0.5, 0.0); // Yaw噪声更大
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
            } else {
                state = 0;
                indexx = 0;
            }
            break;
        case 2:
            // 只接受姿态角帧ID 0x01
            if (byte == 0x01) {
                rx_buffer[2] = byte;
                indexx = 3;
                state = 3;
            } else {
                // 忽略其他帧ID
                state = 0;
                indexx = 0;
            }
            break;
        case 3:
            data_len = byte;
            rx_buffer[3] = byte;
            // 姿态角帧长度应为6字节
            if (data_len != 6) {
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

            // 完整帧接收完成: 帧头(2) + ID(1) + 长度(1) + 数据(6) + 校验和(1) = 11字节
            if (indexx >= 11) {
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
    // 计算校验和 (除最后1字节外的所有字节之和)
    uint8_t checksum = 0;
    for (int i = 0; i < len - 1; i++) {
        checksum += data[i];
    }

    // 验证校验和
    if (checksum != data[len - 1]) {
        return; // 校验失败，丢弃帧
    }

    // 只处理姿态角帧 (帧ID 0x01)
    if (data[2] == 0x01) {
        ProcessAttitudeFrame(data);
    }
}

void ProcessAttitudeFrame(uint8_t* data)
{
        // 每25帧打印一次
    frame_counter++;
    if (frame_counter >= 25) {
        frame_counter = 0;  // 重置计数器
        // 提取原始数据 (小端模式)
    int16_t raw_roll  = (int16_t)((data[5] << 8) | data[4]);  // Roll
    int16_t raw_pitch = (int16_t)((data[7] << 8) | data[6]);  // Pitch
    int16_t raw_yaw   = (int16_t)((data[9] << 8) | data[8]);  // Yaw

    // 转换为实际角度值 (单位：度)
    float roll  = (float)raw_roll  / 32768.0f * 180.0f;
    float pitch = (float)raw_pitch / 32768.0f * 180.0f;
    float yaw   = (float)raw_yaw   / 32768.0f * 180.0f;

	float filtered_roll = KalmanUpdate(&kalman_roll, roll);
    float filtered_pitch = KalmanUpdate(&kalman_pitch, pitch);
    float filtered_yaw = KalmanUpdate(&kalman_yaw, yaw);
		
        // 打印姿态角数据
        printf("Roll: %7.2f°, Pitch: %7.2f°, Yaw: %7.2f°\r\n", roll, pitch, yaw);
		printf("Filtered_Roll: %7.2f°, Filtered_Pitch: %7.2f°, Filtered_Yaw: %7.2f°\r\n",
		filtered_roll, filtered_pitch, filtered_yaw);
    }
}
/* USER CODE END 4 */
