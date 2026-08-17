import cv2
import zmq
import base64
import numpy as np
import time
context = zmq.Context()
footage_socket = context.socket(zmq.PULL)
footage_socket.bind('tcp://*:5500')
last_print = time.time()
poller = zmq.Poller()
poller.register(footage_socket, zmq.POLLIN)
fps = 0.0
while True:
    now = time.time()
    if now - last_print >= 1:
        last_print = now
    socks = dict(poller.poll(100))  # 100ms 超时
    if footage_socket in socks and socks[footage_socket] == zmq.POLLIN:
        frame = footage_socket.recv_string()
        try:
            fps_str, img_str = frame.split('|', 1)
            fps = float(fps_str)
        except Exception as e:
            print("数据格式错误", e)
            continue
        img = base64.b64decode(img_str)
        npimg = np.frombuffer(img, dtype=np.uint8)
        source = cv2.imdecode(npimg, 1)
        if source is None:
            print("解码失败")
            continue
        # 在画面左上角叠加显示帧率
        cv2.putText(source, f"FPS: {fps:.2f}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0,255,0), 2)
        cv2.imshow("Stream", source)
        cv2.waitKey(1)