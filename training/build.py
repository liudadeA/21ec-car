import os
import random
import cv2
import numpy as np
from PIL import Image
from pathlib import Path

# 固定类别ID映射
CATEGORY_ID_MAP = {
    '7': 0,
    '4': 1,
    '3': 2,
    '6': 3,
    '8': 4,
    '1': 5,
    '2': 6,
    '5': 7
}

def set_random_seed():
    """设置随机种子（从30000开始，避免与之前重复）"""
    seed = random.randint(30000, 100000)
    random.seed(seed)
    np.random.seed(seed)
    return seed

def perspective_transform(image):
    """实现从下向上60°视角透视效果"""
    # 基础角度60°，增加±5°随机变化
    angle = 70 + random.uniform(-10, 5)
    width, height = image.size
    img_cv = cv2.cvtColor(np.array(image), cv2.COLOR_RGB2BGR)
    
    angle_rad = np.radians(angle)
    
    # 计算透视变换后的顶部宽度
    top_width = int(width * np.cos(angle_rad))
    top_offset = (width - top_width) // 2 + random.randint(-8, 8)
    
    # 源点和目标点
    src_points = np.float32([[0, 0], [width, 0], [0, height], [width, height]])
    dst_points = np.float32([
        [top_offset, 0],
        [top_offset + top_width, 0],
        [0 + random.randint(-5, 5), height],
        [width + random.randint(-5, 5), height]
    ])
    
    # 获取透视变换矩阵
    matrix = cv2.getPerspectiveTransform(src_points, dst_points)
    
    # 计算变换后的边界
    corners = np.float32([[0, 0], [width, 0], [width, height], [0, height]]).reshape(-1, 1, 2)
    transformed_corners = cv2.perspectiveTransform(corners, matrix)
    
    x_min, x_max = min(transformed_corners[:, 0, 0]), max(transformed_corners[:, 0, 0])
    y_min, y_max = min(transformed_corners[:, 0, 1]), max(transformed_corners[:, 0, 1])
    
    new_width, new_height = int(x_max - x_min), int(y_max - y_min)
    
    # 平移矩阵，确保图像在画布内
    translation_matrix = np.array([[1, 0, -x_min], [0, 1, -y_min], [0, 0, 1]], dtype=np.float32)
    final_matrix = translation_matrix @ matrix
    
    # 执行透视变换
    transformed = cv2.warpPerspective(
        img_cv, 
        final_matrix, 
        (new_width, new_height),
        flags=cv2.INTER_LINEAR,
        borderMode=cv2.BORDER_CONSTANT,
        borderValue=(255, 255, 255)
    )
    
    return Image.fromarray(cv2.cvtColor(transformed, cv2.COLOR_BGR2RGB))

def create_background():
    """创建纯白背景"""
    return Image.new('RGB', (640, 640), (255, 255, 255))

def get_category_id(filename):
    """获取固定类别ID"""
    category_name = filename.stem
    if category_name not in CATEGORY_ID_MAP:
        raise ValueError(f"未定义的类别: {category_name}")
    return CATEGORY_ID_MAP[category_name]

def binarize_image(image):
    """二值化图像"""
    return image.convert('L').point(lambda x: 255 if x > 127 else 0, mode='1').convert('RGB')

def create_composite_image(build_dir, image_dir, label_dir, img_id, digit_name=None):
    """创建单张合成图像（一张背景一个数字）"""
    seed = set_random_seed()
    print(f"生成图像 {img_id:04d}，使用种子: {seed}")
    
    image_files = list(build_dir.glob('*.[jJpP][pPnN][gG]'))
    if not image_files:
        print("未找到图片文件!")
        return False
    
    valid_files = [f for f in image_files if f.stem in CATEGORY_ID_MAP]
    if not valid_files:
        print("没有符合要求的图片文件!")
        return False
    
    # 如果指定了数字名称，使用该数字；否则随机选择
    if digit_name:
        selected_files = [f for f in valid_files if f.stem == digit_name]
        if not selected_files:
            print(f"未找到数字 {digit_name} 的图片文件!")
            return False
        selected_image = selected_files[0]
        print(f"  使用指定数字: {digit_name}")
    else:
        selected_image = random.choice(valid_files)
    
    background = create_background()
    
    try:
        category_id = get_category_id(selected_image)
        image = Image.open(selected_image).convert('RGB')
        
        # 图像处理：60°视角透视变换
        image = perspective_transform(image)
        image = binarize_image(image)
        
        img_width, img_height = image.size
        
        # 【核心修改：放大数字】将缩放比例从 20%-80% 调整为 60%-90%（背景尺寸的更大比例）
        # 数值越大，数字越大（建议范围：0.6-0.9，确保数字不会超出背景）
        scale_factor = random.uniform(2, 2.1)  # 这里是放大数字的关键参数
        new_size = (int(img_width * scale_factor), int(img_height * scale_factor))
        image = image.resize(new_size, Image.LANCZOS)
        
        # 随机位置放置数字（确保数字完全在背景内）
        max_x = 640 - new_size[0]
        max_y = 640 - new_size[1]
        
        if max_x > 0 and max_y > 0:
            x = random.randint(0, max_x)
            y = random.randint(0, max_y)
            
            # 计算数字中心位置
            center_x = x + new_size[0] // 2
            center_y = y + new_size[1] // 2
            
            # YOLO格式标签（归一化坐标）
            yolo_x = center_x / 640
            yolo_y = center_y / 640
            yolo_w = new_size[0] / 640
            yolo_h = new_size[1] / 640
            
            # 粘贴数字到背景
            background.paste(image, (x, y))
            
            # 生成标签
            label = f"{category_id} {yolo_x:.6f} {yolo_y:.6f} {yolo_w:.6f} {yolo_h:.6f}"
            
        else:
            print(f"数字尺寸过大，无法放置，跳过ID: {img_id}")
            return False
            
    except Exception as e:
        print(f"处理图片出错: {selected_image}，错误: {e}")
        return False
    
    # 最终二值化
    background = binarize_image(background)
    
    # 保存结果
    image_filename = f"{img_id:04d}.jpg"
    image_path = image_dir / image_filename
    background.save(image_path)
    
    label_filename = f"{img_id:04d}.txt"
    label_path = label_dir / label_filename
    with open(label_path, 'w') as f:
        f.write(label)
    
    return True

def generate_balanced_sequence(digit_names, total_images):
    """生成平衡的数字序列，确保每个数字出现次数相等"""
    num_digits = len(digit_names)
    images_per_digit = total_images // num_digits
    remaining_images = total_images % num_digits
    
    sequence = []
    
    # 为每个数字分配基础数量
    for digit in digit_names:
        sequence.extend([digit] * images_per_digit)
    
    # 随机分配剩余的图像
    if remaining_images > 0:
        extra_digits = random.sample(digit_names, remaining_images)
        sequence.extend(extra_digits)
    
    # 打乱序列
    random.shuffle(sequence)
    
    return sequence  # 返回生成的序列

def generate_custom_sequence(digit_counts):
    """根据指定类别数量生成序列"""
    sequence = []
    for digit, count in digit_counts.items():
        sequence.extend([digit] * count)
    random.shuffle(sequence)
    return sequence

def get_next_image_id(image_dir):
    """获取下一个可用的图像ID（延续已有编号）"""
    existing_ids = []
    for file in image_dir.glob('*.jpg'):
        try:
            img_id = int(file.stem)
            existing_ids.append(img_id)
        except ValueError:
            continue
    
    if not existing_ids:
        return 1  # 没有现有文件，从1开始
    
    return max(existing_ids) + 1  # 从最大ID+1开始


if __name__ == "__main__":
    BUILD_DIR = Path('build')
    OUTPUT_IMAGE_DIR = Path('datasets/images/val')
    OUTPUT_LABEL_DIR = Path('datasets/labels/val')
    
    OUTPUT_IMAGE_DIR.mkdir(parents=True, exist_ok=True)
    OUTPUT_LABEL_DIR.mkdir(parents=True, exist_ok=True)
    
    # 获取下一个可用ID（延续已有编号）
    start_id = get_next_image_id(OUTPUT_IMAGE_DIR)
    
    # 指定每个类别生成的数量
    # 格式: {'类别名称': 数量}，可根据需要修改
    digit_counts = {
        '7': 50,
        '4': 119,
        '3': 153,
        '6': 80,
        '8': 99,
        '1': 72,
        '2': 149,
        '5': 144
    }
    
    # 计算总生成数量
    num_images = sum(digit_counts.values())
    
    # 生成自定义序列
    digit_sequence = generate_custom_sequence(digit_counts)
    
    print(f"将从ID {start_id:04d} 开始生成 {num_images} 张图像（数字已放大）")
    print("数字分布预览:")
    for digit, count in digit_counts.items():
        print(f"  数字 {digit}: {count} 张")
    
    success_count = 0
    digit_stats = {digit: 0 for digit in digit_counts.keys()}
    
    for i in range(num_images):
        img_id = start_id + i
        digit_name = digit_sequence[i]
        
        if create_composite_image(BUILD_DIR, OUTPUT_IMAGE_DIR, OUTPUT_LABEL_DIR, img_id, digit_name):
            success_count += 1
            digit_stats[digit_name] += 1
    
    print(f"\n成功生成 {success_count}/{num_images} 张图像（ID范围: {start_id:04d}~{start_id+num_images-1:04d}）")
    print("\n实际生成的数字分布:")
    for digit in sorted(digit_stats.keys()):
        print(f"  数字 {digit}: {digit_stats[digit]} 张")
    
    print("\n类别ID映射:")
    for name, id in sorted(CATEGORY_ID_MAP.items(), key=lambda x: x[1]):
        print(f"  ID {id}: 数字 {name}")