import serial
import serial.tools.list_ports
import threading
import time
from datetime import datetime

# 配置参数
HEADER = b'\xaa'        # 帧头
FOOTER = b'\x55\r\n'    # 帧尾
BAUDRATE = 115200       # 波特率
TIMEOUT = 1             # 超时时间(秒)
RX_BUFFER_SIZE = 1024   # 接收缓冲区大小

class SerialDebugger:
    def __init__(self):
        self.ser = None
        self.rx_buffer = bytearray()
        self.is_running = False
        self.receive_thread = None
        self.frame_count = 0
        
    def list_serial_ports(self):
        """列出所有可用串口"""
        ports = serial.tools.list_ports.comports()
        print("可用串口:")
        for i, port in enumerate(ports):
            print(f"  {i+1}. {port.device} - {port.description}")
        return ports
    
    def open_serial(self, port_name, baudrate=BAUDRATE, timeout=TIMEOUT):
        """打开串口连接"""
        try:
            self.ser = serial.Serial(port_name, baudrate, timeout=timeout)
            print(f"已连接到串口: {port_name} ({baudrate}bps)")
            self.is_running = True
            self.receive_thread = threading.Thread(target=self._receive_data)
            self.receive_thread.daemon = True
            self.receive_thread.start()
            return True
        except Exception as e:
            print(f"打开串口失败: {str(e)}")
            return False
    
    def close_serial(self):
        """关闭串口连接"""
        if self.is_running:
            self.is_running = False
            if self.receive_thread:
                self.receive_thread.join(timeout=1.0)
            if self.ser and self.ser.is_open:
                self.ser.close()
                print("串口已关闭")
    
    def _receive_data(self):
        """接收数据线程"""
        while self.is_running and self.ser and self.ser.is_open:
            try:
                # 读取数据
                data = self.ser.read(1)
                if data:
                    self.rx_buffer.extend(data)
                    self._check_for_frame()
            except Exception as e:
                print(f"接收数据异常: {str(e)}")
                break
    
    def _check_for_frame(self):
        """检查缓冲区中是否有完整帧"""
        # 查找帧头
        header_pos = self.rx_buffer.find(HEADER)
        
        # 如果没有找到帧头，清空缓冲区
        if header_pos == -1:
            self.rx_buffer.clear()
            return
        
        # 如果帧头不在起始位置，丢弃之前的数据
        if header_pos > 0:
            self.rx_buffer = self.rx_buffer[header_pos:]
        
        # 查找帧尾
        footer_pos = self.rx_buffer.find(FOOTER)
        
        # 如果找到帧尾，处理完整帧
        if footer_pos != -1:
            frame_end = footer_pos + len(FOOTER)
            frame_data = self.rx_buffer[:frame_end]
            self._process_frame(frame_data)
            
            # 从缓冲区移除已处理的帧
            self.rx_buffer = self.rx_buffer[frame_end:]
    
    def _process_frame(self, frame):
        """处理接收到的完整帧"""
        self.frame_count += 1
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        
        # 提取数据部分(去掉帧头和帧尾)
        data_part = frame[1:-len(FOOTER)]
        
        print(f"\n[{timestamp}] 收到帧 #{self.frame_count} ({len(frame)}字节):")
        print(f"  原始数据: {frame.hex(' ')}")
        
        try:
            # 尝试将数据部分解码为字符串
            data_str = data_part.decode('utf-8')
            print(f"  解码结果: {data_str}")
        except UnicodeDecodeError:
            print(f"  数据部分: {data_part.hex(' ')}")
            print("  无法解码为UTF-8字符串")
    
    def send_data(self, data):
        """发送数据"""
        if self.ser and self.ser.is_open:
            try:
                if isinstance(data, str):
                    data = data.encode('utf-8')
                self.ser.write(data)
                return len(data)
            except Exception as e:
                print(f"发送数据失败: {str(e)}")
                return 0
        else:
            print("串口未打开")
            return 0

def main():
    debugger = SerialDebugger()
    
    # 列出可用串口
    ports = debugger.list_serial_ports()
    if not ports:
        print("未发现可用串口，请检查连接")
        return
    
    # 选择串口
    try:
        port_choice = int(input("请选择要使用的串口编号 (1-%d): " % len(ports))) - 1
        if 0 <= port_choice < len(ports):
            port_name = ports[port_choice].device
        else:
            print("无效的选择")
            return
    except (ValueError, IndexError):
        print("无效的选择")
        return
    
    # 打开串口
    if not debugger.open_serial(port_name):
        return
    
    try:
        print("\n=== 串口调试工具 ===")
        print("使用说明:")
        print("  - 输入数据并按回车发送")
        print("  - 输入 'q' 或 'quit' 退出")
        print("  - 输入 'h' 显示帮助信息")
        
        while True:
            user_input = input("\n> ").strip()
            
            if user_input.lower() in ['q', 'quit']:
                break
            elif user_input.lower() == 'h':
                print("\n帮助信息:")
                print("  - 输入普通文本并回车: 发送文本数据")
                print("  - 输入 'q' 或 'quit': 退出程序")
                print("  - 输入 'h': 显示此帮助信息")
                print("  - 输入 'f': 发送测试帧")
                print("  - 输入 'x XX XX ...': 发送十六进制数据(例如: x aa 55 0d 0a)")
            elif user_input.lower() == 'f':
                # 发送测试帧
                test_frame = HEADER + b"TEST_FRAME" + FOOTER
                debugger.send_data(test_frame)
                print("已发送测试帧")
            elif user_input.lower().startswith('x '):
                # 发送十六进制数据
                try:
                    hex_str = user_input[2:].replace(' ', '')
                    hex_data = bytes.fromhex(hex_str)
                    debugger.send_data(hex_data)
                    print(f"已发送十六进制数据: {hex_data.hex(' ')}")
                except ValueError:
                    print("无效的十六进制格式")
            else:
                # 发送普通文本
                debugger.send_data(user_input)
                print(f"已发送: {user_input}")
    
    except KeyboardInterrupt:
        print("\n程序被用户中断")
    finally:
        # 关闭串口
        debugger.close_serial()

if __name__ == "__main__":
    main()