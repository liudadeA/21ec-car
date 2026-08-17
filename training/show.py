import zmq
import cv2
import base64
import numpy as np

# 配置ZeroMQ接收
context = zmq.Context()
footage_socket = context.socket(zmq.PULL)
footage_socket.bind('tcp://*:5500')  # 监听本机所有网卡的5500端口

while True:
    try:
        msg = footage_socket.recv_string()
        # 拆分fps和图片内容
        fps_str, jpg_as_text = msg.split('|', 1)
        # 解码图片
        jpg_original = base64.b64decode(jpg_as_text)
        np_img = np.frombuffer(jpg_original, dtype=np.uint8)
        frame = cv2.imdecode(np_img, 1)
        # 显示帧率
        cv2.imshow("YOLO Stream", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    except Exception as e:
        print(f"接收或解码出错: {e}")
        continue

cv2.destroyAllWindows()