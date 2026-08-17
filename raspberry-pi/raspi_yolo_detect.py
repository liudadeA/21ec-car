import os
import cv2
import zmq
import base64
import numpy as np
import time
from ultralytics import YOLO
from picamera2 import Picamera2

# 配置ZeroMQ推流
IP = '192.168.100.53'  # 改为你的PC端IP
context = zmq.Context()
footage_socket = context.socket(zmq.PUSH)
footage_socket.connect(f'tcp://{IP}:5500')

# 加载YOLO模型
model = YOLO('best.pt')

# 初始化树莓派摄像头
picam2 = Picamera2()
picam2.configure(picam2.create_video_configuration(main={"size": (640, 480)}))
picam2.start()

frame_count = 0
start_time = time.time()
fps = 0.0

while True:
    frame = picam2.capture_array()
    frame_bgr = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)

    # YOLO推理
    results = model(frame_bgr)
    annotated_frame = results[0].plot()

    # 统计帧率
    frame_count += 1
    elapsed = time.time() - start_time
    if elapsed >= 1.0:
        fps = frame_count / elapsed
        frame_count = 0
        start_time = time.time()
    cv2.putText(annotated_frame, f"FPS: {fps:.2f}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0,255,0), 2)

    # 本地显示
    cv2.imshow('YOLOv8n 实时检测', annotated_frame)

    # 推流到PC（带fps信息）
    _, buffer = cv2.imencode('.jpg', annotated_frame)
    jpg_as_text = base64.b64encode(buffer).decode('utf-8')
    msg = f"{fps}|{jpg_as_text}"
    footage_socket.send_string(msg)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

picam2.close()
cv2.destroyAllWindows()
