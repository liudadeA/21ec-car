from ultralytics import YOLO

# 路径配置
DATASET_DIR = 'datasets'  # 数据集根目录
MODEL = 'yolov8n.pt'      # 使用nano模型
EPOCHS = 100              # 训练轮数
IMG_SIZE = 640            # 输入图片尺寸

# 训练
if __name__ == '__main__':
    model = YOLO(MODEL)
    model.train(
        data=f'{DATASET_DIR}/data.yaml',  # 数据集配置文件
        epochs=EPOCHS,
        imgsz=IMG_SIZE,
        batch=16,
        workers=4,
        device='cpu',
        project='runs/detect',
        name='digit_yolov8n',
        exist_ok=True
    )
    print('训练完成，最优权重保存在runs/detect/digit_yolov8n/weights/best.pt')
