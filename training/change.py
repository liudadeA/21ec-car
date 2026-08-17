import scipy.io as sio
import os
import cv2
import numpy as np
from tqdm import tqdm
from sklearn.model_selection import train_test_split

def convert_svhn_to_yolo(train_mat_path, test_mat_path, output_dir, target_size=640, train_ratio=0.8, only_1to8=True):
    """
    将SVHN数据集转换为YOLOv5格式（修正通道和维度问题）
    
    参数:
    - only_1to8: 是否只保留1-8的数字（过滤0和9）
    """
    # 创建输出目录
    os.makedirs(os.path.join(output_dir, 'images', 'train'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'images', 'val'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'images', 'test'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'labels', 'train'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'labels', 'val'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'labels', 'test'), exist_ok=True)
    
    # 处理训练集
    print("处理训练集...")
    train_data = sio.loadmat(train_mat_path)
    # SVHN的X维度是 (width, height, channels, num_samples) → (32, 32, 3, N)
    num_samples = train_data['X'].shape[3]
    train_indices = np.arange(num_samples)
    train_indices, val_indices = train_test_split(train_indices, train_size=train_ratio, random_state=42)
    
    print("转换训练集样本...")
    for i in tqdm(train_indices):
        process_sample(
            data=train_data, 
            index=i, 
            output_dir=output_dir, 
            split='train', 
            only_1to8=only_1to8
        )
    
    print("转换验证集样本...")
    for i in tqdm(val_indices):
        process_sample(
            data=train_data, 
            index=i, 
            output_dir=output_dir, 
            split='val', 
            only_1to8=only_1to8
        )
    
    # 处理测试集
    print("处理测试集...")
    test_data = sio.loadmat(test_mat_path)
    num_test = test_data['X'].shape[3]
    for i in tqdm(range(num_test)):
        process_sample(
            data=test_data, 
            index=i, 
            output_dir=output_dir, 
            split='test', 
            only_1to8=only_1to8
        )
    
    # 创建data.yaml（根据是否只保留1-8调整类别）
    if only_1to8:
        class_names = ['1', '2', '3', '4', '5', '6', '7', '8']
    else:
        class_names = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9']
    create_data_yaml(output_dir, class_names)
    print(f"数据转换完成！输出目录: {output_dir}")

def process_sample(data, index, output_dir, split, only_1to8):
    """处理单个样本（修正通道和维度问题）"""
    # ----------------------
    # 1. 处理图像（核心修正部分）
    # ----------------------
    # SVHN原始图像维度：(width=32, height=32, channels=3, index)
    img = data['X'][:, :, :, index]  # 取出第i个图像 → shape=(32, 32, 3)
    
    # 修正维度顺序：(width, height, channels) → (height, width, channels)
    img = np.transpose(img, (1, 0, 2))  # 关键修正：交换前两个维度（宽→高）
    
    # 确保数据类型为uint8（OpenCV要求）
    img = img.astype(np.uint8)
    
    # 转换通道顺序：RGB → BGR（OpenCV默认BGR格式，可选但更稳定）
    img = cv2.cvtColor(img, cv2.COLOR_RGB2BGR)
    
    # 保存图像
    img_path = os.path.join(output_dir, 'images', split, f'{index}.jpg')
    cv2.imwrite(img_path, img)  # 现在可正常写入
    
    # ----------------------
    # 2. 处理标签（边界框和类别）
    # ----------------------
    # 获取图像尺寸
    height, width = img.shape[:2]  # (32, 32)
    
    # SVHN的标签格式：data['y']是(N,1)数组，每个样本可能包含多个数字
    # 注意：SVHN的原始标注是"整个图像的类别"，而非每个数字的边界框（Format 1版本）
    # 这里采用"单个数字占满图像"的标注方式（适合单数字图像）
    labels = data['y'][index]
    if isinstance(labels, np.ndarray):
        labels = [labels.item()]  # 转为列表
    
    boxes = []
    for cls in labels:
        # 转换类别（SVHN中0用10表示）
        cls = int(cls)
        if cls == 10:
            cls = 0
        
        # 过滤0和9（如果需要）
        if only_1to8 and cls not in [1,2,3,4,5,6,7,8]:
            continue
        
        # 单个数字占满整个图像（无边界框时的简化标注）
        x_center = 0.5  # 中心x（归一化）
        y_center = 0.5  # 中心y（归一化）
        box_width = 1.0  # 宽度（占满图像）
        box_height = 1.0  # 高度（占满图像）
        
        # 调整1-8的类别索引（0-7对应1-8）
        if only_1to8:
            cls = cls - 1  # 1→0, 2→1, ..., 8→7
        
        boxes.append((cls, x_center, y_center, box_width, box_height))
    
    # 写入YOLO格式标签文件
    label_path = os.path.join(output_dir, 'labels', split, f'{index}.txt')
    with open(label_path, 'w') as f:
        for box in boxes:
            f.write(f"{box[0]} {box[1]:.6f} {box[2]:.6f} {box[3]:.6f} {box[4]:.6f}\n")

def create_data_yaml(output_dir, class_names):
    """创建数据集配置文件"""
    yaml_path = os.path.join(output_dir, 'data.yaml')
    with open(yaml_path, 'w') as f:
        f.write(f"train: {os.path.abspath(os.path.join(output_dir, 'images', 'train'))}\n")
        f.write(f"val: {os.path.abspath(os.path.join(output_dir, 'images', 'val'))}\n")
        f.write(f"test: {os.path.abspath(os.path.join(output_dir, 'images', 'test'))}\n")
        f.write(f"nc: {len(class_names)}\n")
        f.write(f"names: {class_names}\n")

if __name__ == "__main__":
    # 替换为你的SVHN文件路径
    train_mat_path = 'datasets/train_32x32.mat'  # 你的训练集路径
    test_mat_path = 'datasets/test_32x32.mat'    # 你的测试集路径
    output_dir = 'datasets/svhn_yolo'            # 输出目录
    
    convert_svhn_to_yolo(
        train_mat_path=train_mat_path,
        test_mat_path=test_mat_path,
        output_dir=output_dir,
        only_1to8=True  # 只保留1-8的数字
    )