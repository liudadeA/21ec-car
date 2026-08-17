import numpy as np
import os
import glob
import time
import json
import re
import signal
import sys
from PIL import Image, ImageFont, ImageDraw, ImageFilter, ImageEnhance
from keras.models import Model, load_model
from keras.layers import Input, Conv2D, BatchNormalization, Activation, MaxPooling2D, GlobalAveragePooling2D, Dense, Dropout
from keras.optimizers import Adam
from keras.utils import to_categorical
import tensorflow as tf
from sklearn.metrics import confusion_matrix  # 需要添加此导入
from keras.layers import Reshape, Multiply  # 需要添加此导入

# ==================== 全局变量用于中断处理 ====================
current_model = None
current_epoch = 0
interrupt_save_path = ""

# ==================== 最佳模型管理函数 ====================
def find_best_model(model_dir):
    """在模型目录中查找最佳模型（基于训练准确率）"""
    # 最佳模型固定路径
    best_model_path = os.path.join(model_dir, "best_model.keras")
    metrics_path = os.path.join(model_dir, "best_model_metrics.json")
    
    # 如果模型文件不存在，返回空
    if not os.path.exists(best_model_path) or not os.path.exists(metrics_path):
        return None, None, None, None
    
    try:
        # 从指标文件加载最佳模型信息
        with open(metrics_path, 'r') as f:
            metrics = json.load(f)
        best_acc = metrics['train_accuracy']
        best_loss = metrics['train_loss']
        best_epoch = metrics['epoch']
        return best_model_path, best_acc, best_loss, best_epoch
    except (json.JSONDecodeError, KeyError, FileNotFoundError):
        return None, None, None, None

# ==================== 模型保存函数 ====================
def save_best_model(model, model_dir, epoch, train_acc, train_loss):
    """保存最佳模型（基于训练准确率）并记录指标"""
    # 创建模型目录
    os.makedirs(model_dir, exist_ok=True)
    
    # 固定最佳模型路径
    best_model_path = os.path.join(model_dir, "best_model.keras")
    
    # 保存模型
    model.save(best_model_path)
    
    # 保存指标信息
    metrics = {
        "epoch": epoch,
        "train_accuracy": train_acc,
        "train_loss": train_loss,
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
    }
    metrics_path = os.path.join(model_dir, "best_model_metrics.json")
    with open(metrics_path, 'w') as f:
        json.dump(metrics, f, indent=2)
    
    print(f"最佳模型已保存: {best_model_path}")
    print(f"   轮次: {epoch}, 训练准确率: {train_acc:.4f}, 训练损失: {train_loss:.4f}")
    
    return best_model_path

# ==================== 中断处理函数 ====================
def handle_interrupt(signal, frame):
    global current_model, current_epoch, interrupt_save_path
    
    if current_model is not None:
        print("\n\n训练被中断! 正在保存当前模型状态...")
        os.makedirs(os.path.dirname(interrupt_save_path), exist_ok=True)
        
        model_path = interrupt_save_path.replace("_epoch", f"_interrupted_epoch{current_epoch}")
        
        try:
            # 尝试标准保存方式
            current_model.save(model_path)
        except Exception as e:
            print(f"标准保存失败: {str(e)}")
            try:
                # 备选保存方式
                tf.keras.models.save_model(current_model, model_path, save_format='tf')
                print(f"使用TF格式保存成功: {model_path}")
            except Exception as e2:
                print(f"备选保存也失败: {str(e2)}")
                return
        
        # 保存中断信息
        interrupt_info = {
            "epoch": current_epoch,
            "save_path": model_path,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        with open("training_interrupt.json", "w") as f:
            json.dump(interrupt_info, f, indent=2)
        
        print(f"模型已保存到: {model_path}")
    
    sys.exit(0)

# ==================== 模型加载函数 ====================
def load_existing_model(model_dir):
    """加载已训练的模型（优先加载最佳模型）"""
    # 1. 尝试加载最佳模型
    best_model_path, best_acc, best_loss, best_epoch = find_best_model(model_dir)
    if best_model_path and os.path.exists(best_model_path):
        print(f"加载最佳模型: {best_model_path}")
        print(f"训练准确率: {best_acc:.4f}, 训练损失: {best_loss:.4f}, 轮次: {best_epoch}")
        return load_model(best_model_path), best_acc, best_loss, best_epoch
    
    # 2. 尝试加载中断保存的模型
    if os.path.exists("training_interrupt.json"):
        with open("training_interrupt.json") as f:
            interrupt_info = json.load(f)
            model_path = interrupt_info["save_path"]
            if os.path.exists(model_path):
                print(f"加载中断保存的模型: {model_path}")
                return load_model(model_path), 0.0, float('inf'), 0
    
    # 3. 创建新模型
    print("未找到已有模型，创建新模型")
    model = digitx_model2()
    model.compile(
        optimizer=Adam(learning_rate=0.002),
        loss='categorical_crossentropy',
        metrics=['accuracy']
    )
    return model, 0.0, float('inf'), 0

# 注册中断信号处理器
signal.signal(signal.SIGINT, handle_interrupt)

# ==================== 数据生成函数 ====================
def gen_background(size, mu, sigma):
    """生成带噪声的灰度背景"""
    pixel_array = sigma * np.random.randn(size[1], size[0]) + mu
    pixel_array = np.clip(pixel_array, 0, 255).astype(np.uint8)
    return Image.fromarray(pixel_array, 'L')

def rend_char_img(char, angle, img_size, font_size, font_type):
    """渲染字符图像（支持旋转）"""
    img = Image.new('L', img_size, 0)  # 黑色背景
    font = ImageFont.truetype(font_type, size=font_size)
    
    # 用getbbox获取字符尺寸
    bbox = font.getbbox(char)
    char_width = bbox[2] - bbox[0]
    char_height = bbox[3] - bbox[1]
    
    # 居中绘制字符（白色前景）
    drawer = ImageDraw.Draw(img)
    drawer.text(
        ((img_size[0] - char_width) / 2, (img_size[1] - char_height) / 2),
        char,
        font=font,
        fill=255
    )
    del drawer
    
    # 旋转字符（保持黑背景）
    img = img.rotate(angle, expand=False, fillcolor=0)
    return img

def clip_box(box, edge, img_size):
    """调整裁剪框为正方形"""
    left, top, right, bottom = box
    left -= edge[0]
    top -= edge[1]
    right += edge[2]
    bottom += edge[3]
    
    # 调整为正方形
    w, h = right - left, bottom - top
    if w > h:
        top -= (w - h) / 2
        bottom += (w - h) / 2
    else:
        left -= (h - w) / 2
        right += (h - w) / 2
    
    # 确保不超出图像边界
    return (
        max(0, int(left)),
        max(0, int(top)),
        min(img_size[0], int(right)),
        min(img_size[1], int(bottom))
    )

def randomize_and_crop_char_img(char_img, mu, sigma, edge):
    """添加噪声并裁剪字符图像"""
    # 生成随机噪声
    randmizer = sigma * np.random.randn(char_img.size[1], char_img.size[0]) + mu
    pix = np.array(char_img)
    pix[pix > 0] = 255  # 强化字符边缘
    pix = pix * randmizer
    pix = np.clip(pix, 0, 255).astype(int)
    pix = 255 - pix  # 反转颜色
    pix = pix.astype(np.uint8)
    
    # 裁剪
    box = clip_box(char_img.getbbox(), edge, char_img.size)
    croped = Image.fromarray(pix, 'L').crop(box)
    mask = char_img.crop(box)
    return croped, mask

def merge_char_and_background(back_img, char_img, mask):
    """将字符图像合并到背景上"""
    back_arr = np.array(back_img)
    char_arr = np.array(char_img)
    mask_arr = np.array(mask)
    
    # 用掩码合并
    bit_mask = (mask_arr > 0).astype(int)
    pix = char_arr * bit_mask + back_arr * (1 - bit_mask)
    pix = np.clip(pix, 0, 255).astype(np.uint8)
    return Image.fromarray(pix, 'L')

def gen_char_img(char, angle, back_mean, back_var, fore_mean, fore_var, font_path, out_size=640, margin=(0,0,0,0)):
    """生成单个字符的完整图像"""
    # 渲染原始字符
    origi_char_img = rend_char_img(char, angle, (out_size*2, out_size*2), out_size*1.5, font_path)
    # 添加噪声并裁剪
    char_img, mask = randomize_and_crop_char_img(
        origi_char_img,
        (255.0 - fore_mean) / 255.0,
        fore_var / 255.0,
        margin
    )
    # 生成背景并合并
    background = gen_background(char_img.size, back_mean, back_var)
    merged_img = merge_char_and_background(background, char_img, mask)
    
    # 数据增强：随机模糊
    if np.random.rand() > 0.7:  # 30%概率应用模糊
        blur_radius = np.random.uniform(0.5, 2.0)
        merged_img = merged_img.filter(ImageFilter.GaussianBlur(blur_radius))
    
    # 数据增强：随机对比度调整
    enhancer = ImageEnhance.Contrast(merged_img)
    factor = np.random.uniform(0.7, 1.3)
    merged_img = enhancer.enhance(factor)
    
    # 调整为输出尺寸
    return merged_img.resize((out_size, out_size), Image.BILINEAR)

# ==================== 增强的数据生成函数 ====================
def gen_batch_examples(batch_size, img_size, font_dir, output_path=None):
    """批量生成1-8的字符图像（增强易混淆数字样本）"""
    char_list = ['1', '2', '3', '4', '5', '6', '7', '8']
    font_list = glob.glob(os.path.join(font_dir, "*.ttf"))
    
    # 检查字体文件是否存在
    if not font_list:
        raise FileNotFoundError(f"字体目录 {font_dir} 中未找到.ttf文件")
    
    # === 关键修改1：全面调整类别权重 ===
    # 增加1、2、3、7、8的采样概率，减少5的采样概率
    char_weights = [1.5, 1.5, 1.5, 1.0, 0.5, 1.0, 1.5, 1.5]  # 重点关注1、2、3、7、8
    char_weights = np.array(char_weights) / np.sum(char_weights)
    
    X = np.zeros((batch_size, img_size, img_size), dtype=np.float32)
    Y = np.zeros((batch_size, 1), dtype=np.int32)
    
    # 生成随机参数
    max_angle = 30  # 更大旋转角度
    margin_list = [-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5]  # 更大边距范围
    margin_prob = [0.07, 0.08, 0.09, 0.1, 0.11, 0.1, 0.11, 0.1, 0.09, 0.08, 0.07]
    
    for i in range(batch_size):
        # === 关键修改2：按权重采样 ===
        y = np.random.choice(len(char_list), p=char_weights)
        char = char_list[y]
        
        # === 关键修改3：对易混淆数字增强参数 ===
        if char in ['1', '2', '3', '5', '7', '8']:
            # 更大角度范围
            angle = 45 * (np.random.uniform() * 2 - 1)  # -45°~45°
            
            # 更大背景亮度范围
            back_mean = np.random.randint(20, 240)
            
            # 更强噪声
            back_var = back_mean * np.random.uniform(0.4, 0.8)
            
            # 前景参数（保证与背景对比度）
            fore_mean = np.random.randint(20, 240)
            if abs(back_mean - fore_mean) < 60:  # 确保足够对比度
                # 对于1和7，使用更高对比度
                if char in ['1', '7']:
                    fore_mean = 30 if back_mean > 150 else 220
                else:
                    fore_mean = back_mean // 2 if back_mean > 128 else back_mean + 100
            
            fore_var = fore_mean * np.random.uniform(0.4, 0.8)
            
            # 更大边距变化
            margin = np.random.choice([-8, -6, -4, -2, 0, 2, 4, 6, 8], size=4, p=[0.02,0.07,0.12,0.17,0.24,0.17,0.12,0.07,0.02]).tolist()
       
        else:
            # 普通数字使用原始参数
            angle = max_angle * (np.random.uniform() * 2 - 1)
            back_mean = np.random.randint(30, 220)
            back_var = back_mean * np.random.uniform(0.3, 0.6)
            fore_mean = np.random.randint(20, 230)
            
            if abs(back_mean - fore_mean) < 50:
                fore_mean = back_mean // 2 if back_mean > 128 else back_mean + 100
            
            fore_var = fore_mean * np.random.uniform(0.3, 0.6)
            margin = np.random.choice(margin_list, size=4, p=margin_prob).tolist()
        
        # 随机选择字体
        font_path = font_list[np.random.randint(len(font_list))]
        
        # === 关键修改4：对特定数字添加额外处理 ===
        # 1: 增加垂直拉伸和变细
        if char == '1':
            orig_size = (img_size * 2, img_size * 2)
            char_img = rend_char_img(char, angle, orig_size, int(img_size * 1.8), font_path)
            # 垂直拉伸
            new_height = int(char_img.height * 1.2)
            char_img = char_img.resize((char_img.width, new_height), Image.LANCZOS)
            # 裁剪回原始尺寸
            left = (char_img.width - orig_size[0]) // 2
            top = (char_img.height - orig_size[1]) // 2
            char_img = char_img.crop((left, top, left + orig_size[0], top + orig_size[1]))
        # 7: 增加底部横杠
        elif char == '7':
            char_img = rend_char_img(char, angle, (img_size*2, img_size*2), img_size*1.5, font_path)
            # 添加底部横杠
            draw = ImageDraw.Draw(char_img)
            bbox = char_img.getbbox()
            if bbox:
                bar_y = bbox[3] - int(0.05 * img_size)
                bar_height = int(0.08 * img_size)
                draw.rectangle([bbox[0], bar_y, bbox[2], bar_y + bar_height], fill=255)
        # 8: 增加中间分隔线
        elif char == '8':
            char_img = rend_char_img(char, angle, (img_size*2, img_size*2), img_size*1.5, font_path)
            # 添加中间分隔线
            draw = ImageDraw.Draw(char_img)
            bbox = char_img.getbbox()
            if bbox:
                mid_y = (bbox[1] + bbox[3]) // 2
                line_height = int(0.03 * img_size)
                draw.rectangle([bbox[0], mid_y, bbox[2], mid_y + line_height], fill=0)
        else:
            # 正常渲染
            char_img = rend_char_img(char, angle, (img_size*2, img_size*2), img_size*1.5, font_path)
        
        # 添加噪声并裁剪
        char_img, mask = randomize_and_crop_char_img(
            char_img,
            (255.0 - fore_mean) / 255.0,
            fore_var / 255.0,
            margin
        )
        
        # 生成背景并合并
        background = gen_background(char_img.size, back_mean, back_var)
        merged_img = merge_char_and_background(background, char_img, mask)
        
        # 数据增强：随机模糊
        if np.random.rand() > 0.7:  # 30%概率应用模糊
            blur_radius = np.random.uniform(0.5, 2.0)
            merged_img = merged_img.filter(ImageFilter.GaussianBlur(blur_radius))
        
        # 数据增强：随机对比度调整
        enhancer = ImageEnhance.Contrast(merged_img)
        factor = np.random.uniform(0.7, 1.3)
        merged_img = enhancer.enhance(factor)
        
        # 调整为输出尺寸
        final_img = merged_img.resize((img_size, img_size), Image.BILINEAR)
        
        X[i] = np.array(final_img)
        Y[i, 0] = y
        
        # 保存图像（可选）
        if output_path:
            os.makedirs(output_path, exist_ok=True)
            final_img.save(os.path.join(output_path, f"{i}_{char}.png"))
    
    return X, Y

def gen_confuse_batch(batch_size, img_size, font_dir, focus_chars=None):
    """专门生成易混淆数字的加强样本"""
    if focus_chars is None:
        focus_chars = ['1', '2', '3', '5', '7', '8']  # 默认关注所有易混淆数字
    font_list = glob.glob(os.path.join(font_dir, "*.ttf"))
    
    if not font_list:
        raise FileNotFoundError(f"字体目录 {font_dir} 中未找到.ttf文件")
    
    X = np.zeros((batch_size, img_size, img_size), dtype=np.float32)
    Y = np.zeros((batch_size, 1), dtype=np.int32)
    
    # 标签映射
    char_to_label = {char: idx for idx, char in enumerate(['1', '2', '3', '4', '5', '6', '7', '8'])}
    
    for i in range(batch_size):
        # 随机选择混淆字符
        char = np.random.choice(focus_chars)
        y = char_to_label[char]
        
        # 极端增强参数
        angle = 60 * (np.random.uniform() * 2 - 1)  # -60°~60°
        back_mean = np.random.randint(10, 245)
        back_var = back_mean * np.random.uniform(0.5, 1.0)
        fore_mean = np.random.randint(10, 245)
        
        # 确保对比度
        contrast_threshold = 70
        if char in ['1', '7']:  # 1和7需要更高对比度
            contrast_threshold = 80
        
        if abs(back_mean - fore_mean) < contrast_threshold:
            fore_mean = 30 if back_mean > 150 else 220
        
        fore_var = fore_mean * np.random.uniform(0.5, 1.0)
        margin = np.random.choice([-10, -7, -3, 0, 3, 7, 10], size=4, p=[0.1,0.15,0.2,0.1,0.2,0.15,0.1]).tolist()
        
        # 随机选择字体
        font_path = font_list[np.random.randint(len(font_list))]
        
        # 生成图像（使用主函数中的增强处理）
        char_img = rend_char_img(char, angle, (img_size*2, img_size*2), img_size*1.5, font_path)
        
        # 添加噪声并裁剪
        char_img, mask = randomize_and_crop_char_img(
            char_img,
            (255.0 - fore_mean) / 255.0,
            fore_var / 255.0,
            margin
        )
        
        # 生成背景并合并
        background = gen_background(char_img.size, back_mean, back_var)
        merged_img = merge_char_and_background(background, char_img, mask)
        
        # 调整为输出尺寸
        final_img = merged_img.resize((img_size, img_size), Image.BILINEAR)
        
        X[i] = np.array(final_img)
        Y[i, 0] = y
    
    return X, Y

# 添加load_error_samples函数实现
def load_error_samples(error_dir, img_size=640):
    """加载错误样本图像和标签"""
    X_error = []
    Y_error = []
    
    # 遍历错误样本目录
    for filename in os.listdir(error_dir):
        if filename.startswith("error_") and filename.endswith(".png"):
            # 解析文件名获取真实标签 (格式: error_<id>_true<label>_pred<pred>.png)
            match = re.search(r"_true(\d)_", filename)
            if match:
                true_label = int(match.group(1)) - 1  # 转换为0-7的标签
                img_path = os.path.join(error_dir, filename)
                
                # 加载并处理图像
                img = Image.open(img_path).convert('L')
                img = img.resize((img_size, img_size), Image.BILINEAR)
                img_array = np.array(img, dtype=np.float32) / 255.0
                
                X_error.append(img_array)
                Y_error.append(true_label)
    
    return np.array(X_error), np.array(Y_error)

# ==================== 增强的训练函数 ====================
def continue_training(model, model_dir, epochs=50, batch_size=16, font_dir="./fonts", 
                     initial_best_acc=0.0, initial_best_loss=float('inf'), initial_best_epoch=0,
                     confuse_focus=False, focus_chars=None):
    """继续训练模型（增强易混淆数字训练）"""
    global current_model, current_epoch, interrupt_save_path
    
    print(f"开始继续训练，共{epochs}轮...")
    if confuse_focus:
        focus_desc = "所有易混淆数字" if focus_chars is None else f"指定数字({','.join(focus_chars)})"
        print(f"混淆数字专注模式: 开启 - {focus_desc}")
    else:
        print(f"混淆数字专注模式: 关闭")
    
    # 模型路径
    model_path = os.path.join(model_dir, "model_checkpoint.keras")
    interrupt_save_path = model_path

    # 创建目录
    os.makedirs(model_dir, exist_ok=True)
    log_dir = os.path.join(model_dir, "training_logs")
    os.makedirs(log_dir, exist_ok=True)
    
    # 创建CSV日志文件
    log_file = os.path.join(log_dir, "training_log.csv")
    checkpoint_log_file = os.path.join(log_dir, "checkpoint_log.csv")
    confusion_log_file = os.path.join(log_dir, "confusion_log.csv")
    
    # 初始化日志文件
    if not os.path.exists(log_file):
        with open(log_file, "w") as f:
            f.write("epoch,train_loss,train_acc,lr,time,confuse_ratio\n")
    
    if not os.path.exists(checkpoint_log_file):
        with open(checkpoint_log_file, "w") as f:
            f.write("epoch,train_loss,train_acc,lr,time\n")
    
    # 初始化混淆日志
    if not os.path.exists(confusion_log_file):
        with open(confusion_log_file, "w") as f:
            f.write("epoch,true1_pred1,true1_pred5,true1_pred8,true7_pred1,true7_pred5,true7_pred7,true7_pred8,true8_pred5,true8_pred8,true2_pred5,true3_pred5\n")
    
    # 初始化最佳指标
    best_train_acc = initial_best_acc
    best_train_loss = initial_best_loss
    best_epoch = initial_best_epoch
    
    if best_epoch > 0:
        print(f"历史最佳模型: 轮次 {best_epoch}, 训练准确率 {best_train_acc:.4f}, 训练损失 {best_train_loss:.4f}")
    else:
        print("未找到历史最佳模型，从头开始记录")
    
    # === 关键修改1：动态学习率 ===
    lr_schedule = tf.keras.optimizers.schedules.ExponentialDecay(
        initial_learning_rate=0.0001,
        decay_steps=100,
        decay_rate=0.96,
        staircase=True)
    
    model.compile(
        optimizer=Adam(learning_rate=lr_schedule),
        loss='categorical_crossentropy',
        metrics=['accuracy']
    )
    
    # 训练计时
    start_time = time.time()
    current_model = model
    
    # 混淆矩阵记录
    from sklearn.metrics import confusion_matrix
    
    try:
        for epoch in range(epochs):
            current_epoch = epoch
            epoch_start = time.time()
            
            # === 关键修改2：混淆数字加强训练 ===
            if confuse_focus:
                # 混淆数字专注模式
                X_train, Y_train = gen_confuse_batch(batch_size, 640, font_dir, focus_chars)
            else:
                # 正常模式：主批次 + 混淆批次
                X_main, Y_main = gen_batch_examples(batch_size, 640, font_dir)
                
                # 额外生成混淆数字批次
                confuse_size = max(4, batch_size // 3)  # 增加混淆样本比例
                X_confuse, Y_confuse = gen_confuse_batch(confuse_size, 640, font_dir)
                
                # 合并数据
                X_train = np.concatenate([X_main, X_confuse])
                Y_train = np.concatenate([Y_main, Y_confuse])
            
            X_train = X_train / 255.0
            X_train = X_train[..., np.newaxis]
            Y_train_oh = to_categorical(Y_train, num_classes=8)
            
            # 训练一轮
            history = model.fit(
                X_train, Y_train_oh,
                batch_size=batch_size,
                epochs=1,
                verbose=1
            )
            
            # 获取训练指标
            train_loss = history.history['loss'][0]
            train_acc = history.history['accuracy'][0]
            lr = float(tf.keras.backend.get_value(model.optimizer.learning_rate))
            epoch_time = time.time() - epoch_start
            
            # === 关键修改3：扩展混淆矩阵分析 ===
            confuse_ratio = 0.0
            if not confuse_focus and (epoch + 1) % 5 == 0:
                # 使用部分样本进行混淆矩阵分析
                sample_size = min(200, len(X_train))  # 增加样本量
                indices = np.random.choice(len(X_train), sample_size, replace=False)
                X_sample = X_train[indices]
                Y_true = Y_train[indices].flatten()
                
                Y_pred = model.predict(X_sample, verbose=0)
                Y_pred_classes = np.argmax(Y_pred, axis=1)
                
                cm = confusion_matrix(Y_true, Y_pred_classes, labels=range(8))
                
                # 提取关键混淆情况
                confuse_data = {
                    # 1的混淆情况
                    'true1_pred1': cm[0, 0],
                    'true1_pred5': cm[0, 4],
                    'true1_pred8': cm[0, 7],
                    
                    # 7的混淆情况
                    'true7_pred1': cm[6, 0],
                    'true7_pred5': cm[6, 4],
                    'true7_pred7': cm[6, 6],
                    'true7_pred8': cm[6, 7],
                    
                    # 8的混淆情况
                    'true8_pred5': cm[7, 4],
                    'true8_pred8': cm[7, 7],
                    
                    # 2和3被识别为5的情况
                    'true2_pred5': cm[1, 4],
                    'true3_pred5': cm[2, 4]
                }
                
                # 计算综合混淆比例
                confuse_errors = (
                    confuse_data['true1_pred5'] + confuse_data['true1_pred8'] +
                    confuse_data['true7_pred1'] + confuse_data['true7_pred5'] + confuse_data['true7_pred8'] +
                    confuse_data['true8_pred5'] +
                    confuse_data['true2_pred5'] + confuse_data['true3_pred5']
                )
                
                confuse_total = (
                    np.sum(cm[0, :]) +  # 1的总样本
                    np.sum(cm[6, :]) +  # 7的总样本
                    np.sum(cm[7, :]) +  # 8的总样本
                    np.sum(cm[1, :]) +  # 2的总样本
                    np.sum(cm[2, :])    # 3的总样本
                )
                
                confuse_ratio = confuse_errors / max(1, confuse_total)
                
                # 记录混淆日志
                with open(confusion_log_file, "a") as f:
                    f.write(f"{epoch+1}," + ",".join(str(confuse_data[k]) for k in confuse_data) + "\n")
                
                print(f"\n混淆数字分析 (轮次 {epoch+1}):")
                print(f"  1->5/8: {confuse_data['true1_pred5']+confuse_data['true1_pred8']}次")
                print(f"  7->1/5/8: {confuse_data['true7_pred1']+confuse_data['true7_pred5']+confuse_data['true7_pred8']}次")
                print(f"  8->5: {confuse_data['true8_pred5']}次")
                print(f"  2/3->5: {confuse_data['true2_pred5']+confuse_data['true3_pred5']}次")
                print(f"  综合混淆比例: {confuse_ratio:.2f}")
            
            # 记录训练日志
            with open(log_file, "a") as f:
                f.write(f"{epoch+1},{train_loss:.6f},{train_acc:.6f},{lr:.8f},{epoch_time:.2f},{confuse_ratio:.4f}\n")
            
            # 基于训练准确率保存最佳模型
            if train_acc > best_train_acc or (train_acc == best_train_acc and train_loss < best_train_loss):
                best_train_acc = train_acc
                best_train_loss = train_loss
                best_epoch = epoch + 1
                
                # 保存最佳模型
                best_model_path = save_best_model(model, model_dir, best_epoch, best_train_acc, best_train_loss)
                
                # 保存最佳模型指标
                best_metrics = {
                    "epoch": best_epoch,
                    "train_accuracy": best_train_acc,
                    "train_loss": best_train_loss,
                    "learning_rate": lr
                }
                with open(os.path.join(log_dir, "best_model_metrics.json"), "w") as f:
                    json.dump(best_metrics, f, indent=2)
                
                print(f"发现新最佳模型! 训练准确率: {best_train_acc:.4f} 训练损失: {best_train_loss:.4f}")

            # 每轮显示进度
            print(f"轮次: {epoch+1}/{epochs} | 训练准确率: {train_acc:.4f} | 训练损失: {train_loss:.4f}")
            
            # 定期保存检查点
            if (epoch + 1) % 10 == 0 or (epoch + 1) == epochs:
                checkpoint_path = os.path.join(model_dir, f"checkpoint_epoch{epoch+1}.keras")
                model.save(checkpoint_path)
                
                # 保存检查点指标
                checkpoint_metrics = {
                    "epoch": epoch + 1,
                    "train_accuracy": train_acc,
                    "train_loss": train_loss,
                    "learning_rate": lr,
                    "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
                }
                with open(os.path.join(log_dir, f"checkpoint_{epoch+1}_metrics.json"), "w") as f:
                    json.dump(checkpoint_metrics, f, indent=2)
                
                # 记录到检查点日志
                with open(checkpoint_log_file, "a") as f:
                    f.write(f"{epoch+1},{train_loss:.6f},{train_acc:.6f},{lr:.8f},{epoch_time:.2f}\n")
                
                print(f"检查点保存: 第 {epoch+1} 轮 | 训练准确率: {train_acc:.4f} | 训练损失: {train_loss:.4f}")
            
            # 每轮时间估算
            elapsed_time = time.time() - start_time          
            print(f"已用时: {elapsed_time/60:.1f}分钟")
    
    except Exception as e:
        print(f"训练过程中发生错误: {str(e)}")
        print("尝试保存当前模型...")
        error_model_path = model_path.replace(".keras", f"_error_epoch{current_epoch}.keras")
        model.save(error_model_path)
        print(f"模型已保存到: {error_model_path}")
        raise e
    
    finally:
        # 清除全局引用
        current_model = None
        current_epoch = 0
    
    # 最终保存
    final_model_path = os.path.join(model_dir, "final_model.keras")
    model.save(final_model_path)
    
    # 保存最终模型指标
    final_metrics = {
        "total_epochs": epochs,
        "final_train_accuracy": train_acc,
        "final_train_loss": train_loss,
        "best_train_accuracy": best_train_acc,
        "best_train_loss": best_train_loss,
        "best_epoch": best_epoch,
    }
    with open(os.path.join(log_dir, "final_model_metrics.json"), "w") as f:
        json.dump(final_metrics, f, indent=2)
    
    print(f"训练完成! 最佳模型: 第 {best_epoch} 轮 | 训练准确率: {best_train_acc:.4f} | 训练损失: {best_train_loss:.4f}")
    print(f"日志保存至: {log_dir}")
    
    return model

# ==================== 模型结构增强 ====================
def digitx_model2(input_shape=(640, 640, 1)):
    """增强的NIN架构+注意力机制"""
    X_input = Input(shape=input_shape, name='input')
    
    # 第一个卷积块 - 增加通道数
    X = Conv2D(64, (7, 7), strides=(2, 2), padding='same', name='conv0')(X_input)
    X = BatchNormalization(axis=3, name='bn0')(X)
    X = Activation('relu')(X)
    X = MaxPooling2D((2, 2), name='max_pool0')(X)
    X = Conv2D(32, (1, 1), padding='same', name='nin0')(X)
    
    # 第二个卷积块 - 增加通道数
    X = Conv2D(96, (5, 5), strides=(2, 2), padding='same', name='conv1')(X)
    X = BatchNormalization(axis=3, name='bn1')(X)
    X = Activation('relu')(X)
    X = MaxPooling2D((2, 2), name='max_pool1')(X)
    X = Conv2D(48, (1, 1), padding='same', name='nin1')(X)
    
    # 第三个卷积块
    X = Conv2D(128, (3, 3), strides=(1, 1), padding='same', name='conv2')(X)
    X = BatchNormalization(axis=3, name='bn2')(X)
    X = Activation('relu')(X)
    
    # === 新增注意力机制 ===
    # 通道注意力
    channel_att = GlobalAveragePooling2D()(X)
    channel_att = Dense(32, activation='relu')(channel_att)
    channel_att = Dense(128, activation='sigmoid')(channel_att)
    channel_att = Reshape((1, 1, 128))(channel_att)
    X = Multiply()([X, channel_att])
    
    # 空间注意力
    spatial_att = Conv2D(1, (1,1), activation='sigmoid')(X)
    X = Multiply()([X, spatial_att])
    
    # 添加Dropout正则化
    X = Dropout(0.5)(X)
    
    # 输出层
    X = Conv2D(64, (1, 1), padding='same', name='nin2')(X)
    X = GlobalAveragePooling2D(name='gap')(X)
    X = BatchNormalization(axis=1, name='bn3')(X)
    X = Dense(32, activation='relu')(X)  # 新增全连接层
    X = Dropout(0.3)(X)
    X = Dense(8, activation='softmax', name='softmax_out')(X)
    
    return Model(inputs=X_input, outputs=X, name='digitx_model_enhanced')

  # ==================== 测试函数 ====================
def test_model(model, test_size=100, font_dir="./fonts", error_dir="./error_samples_640"):
    """测试模型性能（640×640输入）"""
    print(f"开始测试，共{test_size}个样本...")
    
    # 生成测试数据
    X_test, Y_test = gen_batch_examples(test_size, 640, font_dir)
    X_test = X_test / 255.0
    X_test = X_test[..., np.newaxis]
    Y_test = Y_test.flatten()
    
    # 预测
    Y_pred = model.predict(X_test, verbose=1)
    Y_pred_classes = np.argmax(Y_pred, axis=1)
    
    # 计算准确率
    accuracy = np.mean(Y_pred_classes == Y_test) * 100
    print(f"测试准确率: {accuracy:.2f}%")
    
    # 创建错误样本目录
    if error_dir:
        os.makedirs(error_dir, exist_ok=True)
        error_count = 0
        
        # 保存错误样本
        for i in range(len(Y_test)):
            if Y_pred_classes[i] != Y_test[i]:
                # 保存错误样本图像
                img = Image.fromarray((X_test[i] * 255).astype(np.uint8).squeeze(), 'L')
                img.save(os.path.join(error_dir, f"error_{i}_true{Y_test[i]}_pred{Y_pred_classes[i]}.png"))
                error_count += 1
        
        print(f"保存了 {error_count} 个错误样本到 {error_dir}")

    return accuracy

# ==================== 增强的主函数 ====================
if __name__ == "__main__":
    # 配置参数
    MODEL_DIR = "models"  # 模型保存目录
    FONT_DIR = "./fonts"  # 字体目录
    EPOCHS = 1000  # 训练轮数
    BATCH_SIZE = 16  # 批量大小
    
    # 确保字体目录存在
    os.makedirs(FONT_DIR, exist_ok=True)
    
    # 加载模型（优先加载基于训练准确率的最佳模型）
    model, best_acc, best_loss, best_epoch = load_existing_model(MODEL_DIR)
    
    # 如果模型不存在，使用增强的模型结构
    if best_epoch == 0:
        print("创建新的增强模型...")
        model = digitx_model2(input_shape=(640, 640, 1))
        model.compile(
            optimizer=Adam(learning_rate=0.002),
            loss='categorical_crossentropy',
            metrics=['accuracy']
        )
    
    # === 四阶段训练策略 ===
    
    # 第一阶段：100轮基础训练
    print("\n===== 第一阶段：基础训练 =====")
    model = continue_training(
        model,
        model_dir=MODEL_DIR,
        epochs=100,
        batch_size=BATCH_SIZE,
        font_dir=FONT_DIR,
        initial_best_acc=best_acc,
        initial_best_loss=best_loss,
        initial_best_epoch=best_epoch
    )
    
    # 第二阶段：50轮混淆数字专注训练（所有易混淆数字）
    print("\n===== 第二阶段：混淆数字专注训练 =====")
    # 降低学习率
    model.compile(
        optimizer=Adam(learning_rate=0.00005),
        loss='categorical_crossentropy',
        metrics=['accuracy']
    )
    
    model = continue_training(
        model,
        model_dir=MODEL_DIR,
        epochs=50,
        batch_size=BATCH_SIZE,
        font_dir=FONT_DIR,
        confuse_focus=True,
        focus_chars=['1', '2', '3', '5', '7', '8']  # 专注所有易混淆数字
    )
    
    # 第三阶段：30轮特定数字强化训练
    print("\n===== 第三阶段：特定数字强化训练 =====")
    # 进一步降低学习率
    model.compile(
        optimizer=Adam(learning_rate=0.00001),
        loss='categorical_crossentropy',
        metrics=['accuracy']
    )
    
    # 分组训练特定数字
    focus_groups = [
        ['1', '8'],  # 解决1和8混淆
        ['5', '8'],  # 解决5和8混淆
        ['7']        # 强化7的特征
    ]
    
    for group in focus_groups:
        print(f"\n--- 专注训练: {', '.join(group)} ---")
        model = continue_training(
            model,
            model_dir=MODEL_DIR,
            epochs=10,  # 每组10轮
            batch_size=BATCH_SIZE,
            font_dir=FONT_DIR,
            confuse_focus=True,
            focus_chars=group
        )
    
    # 第四阶段：20轮微调训练
    print("\n===== 第四阶段：微调训练 =====")
    model.compile(
        optimizer=Adam(learning_rate=0.000001),
        loss='categorical_crossentropy',
        metrics=['accuracy']
    )
    
    model = continue_training(
        model,
        model_dir=MODEL_DIR,
        epochs=20,
        batch_size=BATCH_SIZE,
        font_dir=FONT_DIR
    )
    
    # 测试模型
    test_accuracy = test_model(
        model,
        test_size=500,  # 使用500个样本进行测试
        font_dir=FONT_DIR,
        error_dir="./error_samples_enhanced"
    )
    
    # 打印最终测试结果
    print(f"最终测试准确率: {test_accuracy:.2f}%")
    
    # 错误样本再利用
    X_error, Y_error = load_error_samples("./error_samples_enhanced")
    if len(X_error) > 0:
        print("\n===== 错误样本强化训练 =====")
        X_error = X_error[..., np.newaxis]  # 添加通道维度
        Y_error_oh = to_categorical(Y_error, num_classes=8)
        
        # 仅训练错误样本
        model.fit(
            X_error, Y_error_oh,
            batch_size=8,
            epochs=15,  # 增加轮数
            verbose=1
        )
        
        # 重新测试
        test_accuracy = test_model(
            model,
            test_size=200,  # 快速测试
            font_dir=FONT_DIR,
            error_dir="./error_samples_final"
        )
        print(f"强化训练后测试准确率: {test_accuracy:.2f}%")
    
    # 保存最终模型
    final_model_path = os.path.join(MODEL_DIR, "digitx_final_model.keras")
    model.save(final_model_path)
    print(f"最终模型已保存至: {final_model_path}")

  