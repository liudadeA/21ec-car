import serial
import time
from datetime import datetime
import numpy as np

# 配置串口
SERIAL_PORT = 'COM9'
BAUD_RATE = 921600
TIMEOUT = 30
TOTAL_SAMPLES = 50000
END_MARKER = '!END_OF_DATA!'  # 结束标记

def main():
    try:
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TIMEOUT) as ser:
            ser.reset_input_buffer()
            print(f"连接到 {SERIAL_PORT}，波特率 {BAUD_RATE}")
            
            debug_info = []
            data_start_found = False
            
            # 第一阶段：收集调试信息直到"data:"标记
            start_time = time.time()
            while not data_start_found and time.time() - start_time < TIMEOUT:
                try:
                    line = ser.readline().decode('ascii', errors='ignore').strip()
                    if line:
                        print(f"[DEBUG] {line}")
                        debug_info.append(line)
                        
                        if "data:" in line:
                            print(">> 开始接收文本数据")
                            data_start_found = True
                            break
                except UnicodeDecodeError:
                    continue
            
            if not data_start_found:
                print("未找到数据开始标记")
                return
            
            # 第二阶段：接收文本格式的ADC数据
            samples = []
            start_recv = time.time()
            last_update = time.time()
            end_found = False
            line_count = 0
            
            while len(samples) < TOTAL_SAMPLES and not end_found:
                # 检查超时
                if time.time() - start_recv > TIMEOUT:
                    print(f"\n接收超时: {len(samples)}/{TOTAL_SAMPLES}样本")
                    break
                
                # 读取一行数据
                try:
                    line = ser.readline().decode('ascii', errors='ignore').strip()
                    line_count += 1
                    if not line:
                        continue
                    
                    # 检查结束标记
                    if END_MARKER in line:
                        print("\n>> 收到结束标记")
                        end_found = True
                        break
                    
                    # 尝试解析为ADC值
                    try:
                        adc_value = int(line)
                        samples.append(adc_value)
                    except ValueError:
                        # 如果不是数字，可能是调试信息或无效行
                        print(f"[WARN] 忽略非数据行 ({line_count}): {line}")
                        debug_info.append(f"非数据行: {line}")
                        continue
                    
                except UnicodeDecodeError:
                    print(f"[ERROR] 解码错误 ({line_count})")
                    continue
                
                # 显示进度
                if time.time() - last_update > 0.5:
                    progress = len(samples) / TOTAL_SAMPLES * 100
                    print(f"\r接收进度: {progress:.1f}% ({len(samples)}/{TOTAL_SAMPLES}样本) - 行: {line_count}", end='')
                    last_update = time.time()
            
            # 检查数据完整性
            print(f"\n>> 成功接收 {len(samples)}样本 (总行数: {line_count})")
            
            if len(samples) < TOTAL_SAMPLES:
                print(f"警告: 数据不完整 (预期 {TOTAL_SAMPLES} 样本)")
            
            # 保存数据
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            data_filename = f"adc_data_{timestamp}.csv"
            debug_filename = f"debug_log_{timestamp}.txt"
            
            # 保存调试信息
            with open(debug_filename, 'w') as f:
                f.write("\n".join(debug_info))
            
            if samples:
                # 保存CSV数据（使用正确的AD7606量程）
                print(">> 开始转换和保存加速度数据...")
                with open(data_filename, 'w') as f:
                    # 添加加速度列
                    f.write("sample_index,timestamp_us,adc_value,voltage,acceleration_g\n")
                    
                    for i, adc in enumerate(samples):
                        timestamp_us = i * 100  # 100μs间隔 (10kHz)
                        
                        # AD7606配置为±5V量程
                        # 码值0对应-5V，65535对应+5V
                        voltage = (adc / 65535.0) * 5.0

                        # 根据A29T02-E传感器规格书：
                        # 偏置电压 = 2.5V
                        # 灵敏度 = 100mV/g = 0.1V/g
                        acceleration_g = (voltage - 2.5) / 0.1
                        
                        f.write(f"{i},{timestamp_us},{adc},{voltage:.6f},{acceleration_g:.6f}\n")
                
                print(f">> 采样数据保存至 {data_filename}")
                
                # 基本统计
                voltages = [(adc / 65535.0) * 10.0 - 5.0 for adc in samples]
                accelerations = [(v - 2.5) / 0.1 for v in voltages]
                
                avg_voltage = np.mean(voltages)
                avg_accel = np.mean(accelerations)
                
                print(f"电压统计: 平均={avg_voltage:.3f}V")
                print(f"加速度统计: 平均={avg_accel:.3f}g")
                
                # 检查零点偏移
                zero_g_voltage = 2.5  # 根据传感器规格书
                actual_zero_g = np.median(voltages)  # 使用中值更抗噪
                offset = actual_zero_g - zero_g_voltage
                print(f"零点偏移: {offset:.4f}V (建议校准: {offset/0.1:.2f}g)")
                
                # 检查电压范围
                min_voltage = min(voltages)
                max_voltage = max(voltages)
                print(f"电压范围: {min_voltage:.3f}V - {max_voltage:.3f}V")
                
                # 检查加速度范围
                min_accel = min(accelerations)
                max_accel = max(accelerations)
                print(f"加速度范围: {min_accel:.3f}g - {max_accel:.3f}g")
                
                # 传感器量程检查
                if min_accel < -20 or max_accel > 20:
                    print("警告: 检测到加速度超出传感器量程(±20g)!")
            else:
                print(">> 无有效数据可保存")
            
            print(f">> 调试信息保存至 {debug_filename}")
            
    except serial.SerialException as e:
        print(f"串口错误: {e}")
    except Exception as e:
        print(f"错误: {e}")

if __name__ == "__main__":
    main()