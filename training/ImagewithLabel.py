import os
import cv2
import numpy as np
from pathlib import Path

# 定义目录路径
IMAGES_DIR = Path('dataset/images/val')
LABELS_DIR = Path('dataset/labels/val')
OUTPUT_DIR = Path('dataset/labeled_images')  # 保存带标签框的图像

# 创建输出目录
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

# 类别ID到名称的映射（需要与生成标签时的映射一致）
# 这里会自动从标签文件和图像文件名重建映射，也可手动指定
category_map = {}

def draw_labels_on_image():
    """读取图像和标签，绘制边界框并保存"""
    # 获取所有图像文件
    image_files = [f for f in IMAGES_DIR.iterdir() if f.suffix.lower() in ['.jpg', '.jpeg', '.png']]
    
    if not image_files:
        print("未找到图像文件！")
        return
    
    # 重建类别映射（从之前生成的标签和图像中）
    # 注意：如果需要精确映射，最好保存之前的category_id_map到文件中
    # 这里简化处理，假设类别名称在标签生成时已正确映射
    
    for img_path in image_files:
        # 对应的标签文件路径
        label_path = LABELS_DIR / f"{img_path.stem}.txt"
        
        if not label_path.exists():
            print(f"警告：未找到 {img_path.name} 对应的标签文件，跳过...")
            continue
        
        # 读取图像
        image = cv2.imread(str(img_path))
        if image is None:
            print(f"无法读取图像 {img_path.name}，跳过...")
            continue
        
        h, w = image.shape[:2]  # 图像高度和宽度（应为640x640）
        
        # 读取标签文件
        with open(label_path, 'r') as f:
            lines = f.readlines()
        
        # 绘制每个标签框
        for line in lines:
            line = line.strip()
            if not line:
                continue
            
            # 解析YOLO格式标签：类别ID 中心点x 中心点y 宽度 高度（均为归一化值）
            parts = line.split()
            if len(parts) != 5:
                print(f"标签格式错误：{line}，跳过...")
                continue
            
            try:
                class_id = int(parts[0])
                cx = float(parts[1])
                cy = float(parts[2])
                bw = float(parts[3])
                bh = float(parts[4])
            except ValueError:
                print(f"标签数值错误：{line}，跳过...")
                continue
            
            # 将归一化坐标转换为像素坐标
            center_x = int(cx * w)
            center_y = int(cy * h)
            box_width = int(bw * w)
            box_height = int(bh * h)
            
            # 计算边界框左上角和右下角坐标
            x1 = int(center_x - box_width / 2)
            y1 = int(center_y - box_height / 2)
            x2 = int(center_x + box_width / 2)
            y2 = int(center_y + box_height / 2)
            
            # 绘制边界框（随机颜色）
            color = (random.randint(0, 255), random.randint(0, 255), random.randint(0, 255))
            cv2.rectangle(image, (x1, y1), (x2, y2), color, 2)
            
            # 标注类别ID（如果有类别名称映射可替换）
            class_name = f"class_{class_id}"
            # 如果有类别名称映射，可替换为实际名称
            # if class_id in category_map:
            #     class_name = category_map[class_id]
            
            # 绘制类别名称
            cv2.putText(
                image, 
                class_name, 
                (x1, y1 - 10), 
                cv2.FONT_HERSHEY_SIMPLEX, 
                0.5, 
                color, 
                2
            )
        
        # 保存带标签框的图像
        output_path = OUTPUT_DIR / img_path.name
        cv2.imwrite(str(output_path), image)
        print(f"已保存带标签框的图像：{output_path}")

if __name__ == "__main__":
    import random  # 用于生成随机颜色
    draw_labels_on_image()
    print(f"\n所有带标签框的图像已保存至：{OUTPUT_DIR}")