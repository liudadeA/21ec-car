import os
import shutil
import re
from tqdm import tqdm

def is_valid_yolo_line(line, img_width=640, img_height=640):
    """检查单行标注是否符合YOLO格式要求"""
    parts = line.strip().split()
    if len(parts) < 5:  # 至少需要类别ID和4个坐标值
        return False
    
    # 检查类别ID是否为整数
    try:
        class_id = int(parts[0])
        if class_id < 0:
            return False
    except ValueError:
        return False
    
    # 检查坐标是否为浮点数且在有效范围内
    try:
        x_center, y_center, width, height = map(float, parts[1:5])
        if not (0.0 <= x_center <= 1.0 and 0.0 <= y_center <= 1.0 and 
                0.0 <= width <= 1.0 and 0.0 <= height <= 1.0):
            return False
    except ValueError:
        return False
    
    return True

def validate_yolo_labels(label_path, img_width=640, img_height=640):
    """验证整个标签文件是否符合YOLO格式"""
    if not os.path.exists(label_path):
        return False
    
    with open(label_path, 'r') as f:
        lines = f.readlines()
        if not lines:  # 空文件视为无效
            return False
        
        for line in lines:
            if not is_valid_yolo_line(line, img_width, img_height):
                return False
    
    return True

def find_image_path(label_file, image_dir):
    """查找与标签文件对应的图像文件路径"""
    base_name = os.path.splitext(os.path.basename(label_file))[0]
    
    # 尝试多种常见图像扩展名
    for ext in ['.jpg', '.jpeg', '.png', '.bmp', '.webp']:
        image_path = os.path.join(image_dir, base_name + ext)
        if os.path.exists(image_path):
            return image_path
    
    return None

def clean_invalid_yolo_files(image_dir, label_dir, dry_run=True):
    """清理不符合YOLO格式的图像和标签文件
    
    Args:
        image_dir: 图像文件目录
        label_dir: 标签文件目录
        dry_run: 是否只显示不删除，默认为True
    """
    if not os.path.exists(image_dir):
        print(f"错误: 图像目录不存在: {image_dir}")
        return
    
    if not os.path.exists(label_dir):
        print(f"错误: 标签目录不存在: {label_dir}")
        return
    
    # 获取所有标签文件
    label_files = []
    for root, _, files in os.walk(label_dir):
        for file in files:
            if file.endswith('.txt'):
                label_files.append(os.path.join(root, file))
    
    print(f"找到 {len(label_files)} 个标签文件，开始检查...")
    
    invalid_count = 0
    deleted_count = 0
    
    for label_file in tqdm(label_files, desc="检查标签文件"):
        # 验证标签文件格式
        if not validate_yolo_labels(label_file):
            invalid_count += 1
            
            # 查找对应的图像文件
            image_file = find_image_path(label_file, image_dir)
            
            # 打印信息
            print(f"\n发现无效标签: {label_file}")
            if image_file:
                print(f"  对应的图像: {image_file}")
            else:
                print("  未找到对应的图像文件")
            
            # 执行删除操作
            if not dry_run:
                # 删除标签文件
                try:
                    os.remove(label_file)
                    print(f"  已删除标签文件")
                    deleted_count += 1
                except Exception as e:
                    print(f"  删除标签文件失败: {e}")
                
                # 删除对应的图像文件
                if image_file:
                    try:
                        os.remove(image_file)
                        print(f"  已删除图像文件")
                        deleted_count += 1
                    except Exception as e:
                        print(f"  删除图像文件失败: {e}")
    
    print(f"\n检查完成:")
    print(f"  发现 {invalid_count} 个无效标签文件")
    if not dry_run:
        print(f"  已删除 {deleted_count} 个文件")
    else:
        print(f"  干运行模式，未删除任何文件。如需执行删除，请将dry_run设置为False")

if __name__ == "__main__":
    # 获取用户输入的路径
    image_dir = input("请输入图像文件夹路径: ").strip()
    label_dir = input("请输入标签文件夹路径: ").strip()
    
    # 先进行干运行，查看有哪些文件会被删除
    clean_invalid_yolo_files(image_dir, label_dir, dry_run=False)
    
    # 如果确认要删除，将dry_run改为False再次运行
    # clean_invalid_yolo_files(image_dir, label_dir, dry_run=False)