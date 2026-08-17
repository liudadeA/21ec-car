import cv2
import numpy as np
import os
import time
import json
from picamera2 import Picamera2

# 禁用PyKMS预览
os.environ["PYKMS_NO_PREVIEW"] = "1"

# 初始化摄像头
picam2 = Picamera2()
# 设置分辨率为640x480
picam2.configure(picam2.create_video_configuration(main={"size": (640, 480)}))
picam2.start()

# 分辨率参数
MAX_WIDTH = 640
MAX_HEIGHT = 480

# 创建主窗口
cv2.namedWindow("Red Object Detection", cv2.WINDOW_NORMAL)
cv2.resizeWindow("Red Object Detection", MAX_WIDTH, MAX_HEIGHT + 200)  # 增加高度以容纳滑块

# 默认参数
DEFAULT_PARAMS = {
    'h_low1': 0,          # 红色范围1 - 最小H值
    'h_high1': 10,        # 红色范围1 - 最大H值
    'h_low2': 170,        # 红色范围2 - 最小H值
    'h_high2': 180,       # 红色范围2 - 最大H值
    's_low': 100,         # 最小饱和度
    's_high': 255,        # 最大饱和度
    'v_low': 100,         # 最小亮度
    'v_high': 255,        # 最大亮度
    'min_width': 20,      # 最小宽度
    'max_width': MAX_WIDTH,  # 最大宽度
    'min_height': 20,     # 最小高度
    'max_height': MAX_HEIGHT # 最大高度
}

# 尝试从配置文件加载参数
CONFIG_FILE = "red_detection_config.json"
params = DEFAULT_PARAMS.copy()

try:
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, 'r') as f:
            loaded_params = json.load(f)
            # 只加载有效的参数
            for key in DEFAULT_PARAMS.keys():
                if key in loaded_params:
                    params[key] = loaded_params[key]
except Exception as e:
    print(f"Config load failed: {e}")

# 创建滑块回调函数（空函数）
def nothing(x):
    pass

# 在主窗口创建滑块
cv2.createTrackbar('H1 Low', 'Red Object Detection', params['h_low1'], 180, nothing)
cv2.createTrackbar('H1 High', 'Red Object Detection', params['h_high1'], 180, nothing)
cv2.createTrackbar('H2 Low', 'Red Object Detection', params['h_low2'], 180, nothing)
cv2.createTrackbar('H2 High', 'Red Object Detection', params['h_high2'], 180, nothing)
cv2.createTrackbar('S Low', 'Red Object Detection', params['s_low'], 255, nothing)
cv2.createTrackbar('S High', 'Red Object Detection', params['s_high'], 255, nothing)
cv2.createTrackbar('V Low', 'Red Object Detection', params['v_low'], 255, nothing)
cv2.createTrackbar('V High', 'Red Object Detection', params['v_high'], 255, nothing)
cv2.createTrackbar('Min Width', 'Red Object Detection', params['min_width'], MAX_WIDTH, nothing)
cv2.createTrackbar('Max Width', 'Red Object Detection', params['max_width'], MAX_WIDTH, nothing)
cv2.createTrackbar('Min Height', 'Red Object Detection', params['min_height'], MAX_HEIGHT, nothing)
cv2.createTrackbar('Max Height', 'Red Object Detection', params['max_height'], MAX_HEIGHT, nothing)

# 帧率计算
frame_count = 0
start_time = time.time()
fps = 0.0

# 保存参数到文件
def save_params():
    try:
        with open(CONFIG_FILE, 'w') as f:
            json.dump(params, f, indent=4)
        print("Parameters saved")
    except Exception as e:
        print(f"Save failed: {e}")

# 创建显示面板
display_panel = np.zeros((200, MAX_WIDTH, 3), dtype=np.uint8)

try:
    while True:
        # 从摄像头捕获帧
        frame = picam2.capture_array()
        frame_bgr = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
        
        # 计算帧率
        frame_count += 1
        elapsed = time.time() - start_time
        if elapsed >= 1.0:
            fps = frame_count / elapsed
            frame_count = 0
            start_time = time.time()
        
        # 从滑块获取当前参数值
        params['h_low1'] = cv2.getTrackbarPos('H1 Low', 'Red Object Detection')
        params['h_high1'] = cv2.getTrackbarPos('H1 High', 'Red Object Detection')
        params['h_low2'] = cv2.getTrackbarPos('H2 Low', 'Red Object Detection')
        params['h_high2'] = cv2.getTrackbarPos('H2 High', 'Red Object Detection')
        params['s_low'] = cv2.getTrackbarPos('S Low', 'Red Object Detection')
        params['s_high'] = cv2.getTrackbarPos('S High', 'Red Object Detection')
        params['v_low'] = cv2.getTrackbarPos('V Low', 'Red Object Detection')
        params['v_high'] = cv2.getTrackbarPos('V High', 'Red Object Detection')
        params['min_width'] = cv2.getTrackbarPos('Min Width', 'Red Object Detection')
        params['max_width'] = cv2.getTrackbarPos('Max Width', 'Red Object Detection')
        params['min_height'] = cv2.getTrackbarPos('Min Height', 'Red Object Detection')
        params['max_height'] = cv2.getTrackbarPos('Max Height', 'Red Object Detection')
        
        # 确保最小值小于最大值
        if params['min_width'] > params['max_width']:
            params['min_width'], params['max_width'] = params['max_width'], params['min_width']
            cv2.setTrackbarPos('Min Width', 'Red Object Detection', params['min_width'])
            cv2.setTrackbarPos('Max Width', 'Red Object Detection', params['max_width'])
            
        if params['min_height'] > params['max_height']:
            params['min_height'], params['max_height'] = params['max_height'], params['min_height']
            cv2.setTrackbarPos('Min Height', 'Red Object Detection', params['min_height'])
            cv2.setTrackbarPos('Max Height', 'Red Object Detection', params['max_height'])
        
        # 红色物体识别
        hsv = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2HSV)
        
        # 定义红色范围
        lower_red1 = np.array([params['h_low1'], params['s_low'], params['v_low']])
        upper_red1 = np.array([params['h_high1'], params['s_high'], params['v_high']])
        lower_red2 = np.array([params['h_low2'], params['s_low'], params['v_low']])
        upper_red2 = np.array([params['h_high2'], params['s_high'], params['v_high']])
        
        # 创建红色掩模
        mask1 = cv2.inRange(hsv, lower_red1, upper_red1)
        mask2 = cv2.inRange(hsv, lower_red2, upper_red2)
        red_mask = cv2.bitwise_or(mask1, mask2)
        
        # 形态学操作
        kernel = np.ones((5, 5), np.uint8)
        red_mask = cv2.morphologyEx(red_mask, cv2.MORPH_OPEN, kernel)
        red_mask = cv2.morphologyEx(red_mask, cv2.MORPH_CLOSE, kernel)
        
        # 查找轮廓
        contours, _ = cv2.findContours(red_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        # 标记红色物体
        red_objects = []
        for contour in contours:
            area = cv2.contourArea(contour)
            x, y, w, h = cv2.boundingRect(contour)
            
            # 检查尺寸是否符合要求
            if (params['min_width'] <= w <= params['max_width'] and 
                params['min_height'] <= h <= params['max_height']):
                red_objects.append((x, y, w, h, area))
                
                # 在图像上绘制矩形
                cv2.rectangle(frame_bgr, (x, y), (x + w, y + h), (0, 255, 0), 2)
                cv2.putText(frame_bgr, f"{w}x{h}", (x, y - 10), 
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        
        
        # 显示组合图像
        cv2.imshow("Red Object Detection", frame_bgr)
        
        # 处理键盘输入
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            # 退出前保存参数
            save_params()
            break
        elif key == ord('s'):
            # 保存参数
            save_params()
        elif key == ord('r'):
            # 重置参数
            for key, value in DEFAULT_PARAMS.items():
                params[key] = value
                cv2.setTrackbarPos(key.replace('_', ' ').title(), 'Red Object Detection', value)
            print("Parameters reset")

finally:
    # 退出前保存参数
    save_params()
    picam2.stop()
    cv2.destroyAllWindows()
