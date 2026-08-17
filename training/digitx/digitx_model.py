from keras import layers
from keras import optimizers
from keras.layers import Input, Dense, Activation, BatchNormalization, Flatten, Conv2D, MaxPooling2D, GlobalAveragePooling2D, Dropout
from keras.models import Model
from keras.initializers import glorot_uniform
from keras.utils import to_categorical
import numpy as np
import os
from PIL import Image, ImageFont, ImageDraw
import glob
import random

# ===== 图像生成函数（修改为支持640×480） =====
def gen_background(size, mu, sigma):
    pixel_array = sigma * np.random.randn(size[1], size[0]) + mu
    pixel_array[pixel_array > 255] = 255
    pixel_array[pixel_array < 0] = 0
    pixel_array = pixel_array.astype(np.uint8)
    return Image.fromarray(pixel_array, 'L')

def rend_char_img(char, angle, img_size, font_size, font_type):
    img = Image.new('L', img_size, 0)
    font = ImageFont.truetype(font_type, size=font_size)
    
    # 兼容新版本Pillow
    bbox = font.getbbox(char)
    char_size = (bbox[2] - bbox[0], bbox[3] - bbox[1])

    drawer = ImageDraw.Draw(img)
    drawer.text(((img_size[0] - char_size[0]) / 2, (img_size[1] - char_size[1]) / 2), 
                char, font=font, fill=255)
    del drawer
    img = img.rotate(angle, expand=False, fillcolor=0)
    return img

def clip_box(box, edge, img_size):
    left, top, right, bottom = box
    left -= edge[0]
    top -= edge[1]
    right += edge[2]
    bottom += edge[3]
    w = right - left
    h = bottom - top
    if w > h:
        top -= (w - h) / 2
        bottom += (w - h) / 2
    elif w < h:
        left -= (h - w) / 2
        right += (h - w) / 2
    if left < 0:
        left = 0
    if top < 0:
        top = 0
    if right > img_size[0]:
        right = img_size[0]
    if bottom > img_size[1]:
        bottom = img_size[1]
    return (left, top, right, bottom)

def randomize_and_crop_char_img(char_img, mu, sigma, edge):
    randmizer = sigma * np.random.randn(char_img.size[1], char_img.size[0]) + mu
    pix = np.array(char_img)
    pix[pix > 0] = 255  # 禁用半色调
    pix = pix * randmizer
    pix[pix > 255] = 255
    pix[pix < 0] = 0
    pix = pix.astype(int)
    pix = 255 - pix  # 切换前景和背景颜色
    pix = pix.astype(np.uint8)
    box = clip_box(char_img.getbbox(), edge, char_img.size)
    croped = Image.fromarray(pix, 'L').crop(box)
    mask = char_img.crop(box)
    return croped, mask

def merge_char_and_background(back_img, char_img, mask):
    back_arr = np.array(back_img)
    char_arr = np.array(char_img)
    mask_arr = np.array(mask)
    bit_mask = (mask_arr > 0).astype(int)
    pix = char_arr * bit_mask + back_arr * (1 - bit_mask)
    pix[pix > 255] = 255
    pix[pix < 0] = 0
    pix = pix.astype(np.uint8)
    merged_img = Image.fromarray(pix, 'L')
    return merged_img

def gen_char_img(char, angle, back_mean, back_var, fore_mean, fore_var, font_path, out_size=640, margin=(0, 0, 0, 0)):  # 修改默认尺寸为640
    origi_char_img = rend_char_img(char, angle, (out_size*2, out_size*2), out_size, font_path)  # 调整基础尺寸
    char_img_with_rand, mask = randomize_and_crop_char_img(origi_char_img, 
                                                           (255.0 - fore_mean) / 255.0, 
                                                           fore_var / 255.0, margin)
    background_img = gen_background(char_img_with_rand.size, back_mean, back_var)
    merged_img = merge_char_and_background(background_img, char_img_with_rand, mask)
    out_img = merged_img.resize((out_size, out_size), Image.BILINEAR)  # 调整为640x640
    return out_img

def gen_batch_examples(batch_size, img_size, font_dir, output_path=None):
    '''生成1-8的字符图像'''
    char_list = ['1', '2', '3', '4', '5', '6', '7', '8']  # 仅生成1-8
    max_angle = 10
    min_back_mean = 60
    max_back_var_ratio = 0.3
    max_fore_mean = 200
    max_fore_var_ratio = 0.3
    font_list = glob.glob(os.path.join(font_dir, "*.ttf"))

    margin_list = [0, 1, 2, 3, 4, 5, -1, -2, -3]
    probability = [0.2, 0.2, 0.1, 0.08, 0.09, 0.07, 0.1, 0.1, 0.06]

    X = np.zeros((batch_size, img_size, img_size))
    Y = np.zeros((batch_size, 1))

    if output_path and not os.path.exists(output_path):
        os.makedirs(output_path)

    for i in range(batch_size):
        y = np.random.randint(len(char_list))
        char = char_list[y]
        angle = max_angle * (np.random.uniform() * 2.0 - 1.0)
        
        back_mean = np.random.randint(min_back_mean, 256)
        back_var_ratio = np.random.uniform() * max_back_var_ratio
        back_var = back_mean * back_var_ratio

        fore_mean = np.random.randint(0, max_fore_mean)
        if back_mean - fore_mean < 30:
            fore_mean = back_mean / 2
        fore_var_ratio = np.random.uniform() * max_fore_var_ratio
        fore_var = fore_mean * fore_var_ratio

        font_path = font_list[np.random.randint(len(font_list))]
        margin = np.random.choice(margin_list, size=4, p=probability).tolist()

        char_img = gen_char_img(char, angle, back_mean, back_var, fore_mean, fore_var, 
                               font_path, img_size, margin)
        X[i, :, :] = np.array(char_img)
        Y[i, 0] = y  # 标签0-7对应字符1-8

        if output_path:
            char_img.save(os.path.join(output_path, f"{i}.png"))

    return X, Y

# ===== 模型定义（修改为适应640×480） =====
def digitx_model2(input_shape):
    '''使用NIN和全局平均池化的模型，调整为适应640×480输入'''
    X_input = Input(input_shape, name='input')

    # 第一组卷积+池化（减少特征图尺寸）
    X = Conv2D(32, (5, 5), strides=(2, 2), padding='same', name='conv0')(X_input)  # 步长改为2x2
    X = BatchNormalization(axis=3, name='bn0')(X)
    X = Activation('relu')(X)
    X = MaxPooling2D((2, 2), name='max_pool0')(X)  # 池化后尺寸约为160x160
    X = Conv2D(16, (1, 1), strides=(1, 1), padding='same', name='nin0')(X)

    # 第二组卷积+池化
    X = Conv2D(32, (3, 3), strides=(2, 2), padding='same', name='conv1')(X)  # 步长改为2x2
    X = BatchNormalization(axis=3, name='bn1')(X)
    X = Activation('relu')(X)
    X = MaxPooling2D((2, 2), name='max_pool1')(X)  # 池化后尺寸约为40x40
    X = Conv2D(16, (1, 1), strides=(1, 1), padding='same', name='nin1')(X)

    # 第三组卷积（更深的特征提取）
    X = Conv2D(32, (3, 3), strides=(2, 2), padding='same', name='conv2')(X)  # 步长改为2x2
    X = BatchNormalization(axis=3, name='bn2')(X)
    X = Activation('relu')(X)

    # 修改输出通道为8（对应1-8）
    X = Conv2D(8, (1, 1), strides=(1, 1), padding='same', name='nin2')(X)

    X = GlobalAveragePooling2D()(X)
    X = BatchNormalization(axis=1, name='bn3')(X)
    X = Activation('softmax', name="softmax_out")(X)

    model = Model(inputs=X_input, outputs=X, name='digitx_model2')
    return model

def create_model():
    '''创建并编译模型'''
    model = digitx_model2((640, 640, 1))  # 修改输入尺寸为640x640x1
    opt = optimizers.Adam(learning_rate=0.001)  # 降低学习率（高分辨率需要更精细训练）
    model.compile(optimizer=opt, loss='categorical_crossentropy', metrics=['accuracy'])
    model.summary()
    return model

# ===== 训练和评估函数（调整批量大小和内存管理） =====
def train(model, epochs=100, batch_size=32, font_dir="./fonts"):  # 减小批量大小（640x640更占内存）
    '''训练模型'''
    for epoch in range(epochs):
        # 生成训练数据
        X_train, Y_train = gen_batch_examples(batch_size, 640, font_dir)  # 使用640尺寸
        X_train /= 255.0
        X_train = np.reshape(X_train, (batch_size, 640, 640, 1))
        Y_train_oh = to_categorical(Y_train, num_classes=8)  # 8个类别(1-8)

        # 生成验证数据
        X_val, Y_val = gen_batch_examples(batch_size // 4, 640, font_dir)  # 小批量验证
        X_val /= 255.0
        X_val = np.reshape(X_val, (len(X_val), 640, 640, 1))
        Y_val_oh = to_categorical(Y_val, num_classes=8)

        # 训练一个epoch
        history = model.fit(X_train, Y_train_oh, 
                           batch_size=batch_size,
                           epochs=1,
                           validation_data=(X_val, Y_val_oh),
                           verbose=1)
    return model

def test(model, test_size=100, font_dir="./fonts", save_error_dir=None):  # 减小测试集大小
    '''测试模型并计算准确率'''
    # 生成测试数据
    X_test, Y_test = gen_batch_examples(test_size, 640, font_dir)  # 使用640尺寸
    X_test /= 255.0
    X_test = np.reshape(X_test, (test_size, 640, 640, 1))
    
    # 预测
    Y_pred = model.predict(X_test)
    Y_pred_classes = np.argmax(Y_pred, axis=1)
    Y_test = Y_test.reshape(-1).astype(int)
    
    # 计算准确率
    accuracy = np.mean(Y_pred_classes == Y_test)
    print(f"测试准确率: {accuracy * 100:.2f}%")
    
    # 保存错误样本
    if save_error_dir and accuracy < 1.0:
        os.makedirs(save_error_dir, exist_ok=True)
        error_count = 0
        for i in range(test_size):
            if Y_pred_classes[i] != Y_test[i]:
                true_label = Y_test[i] + 1  # 转换为1-8
                pred_label = Y_pred_classes[i] + 1
                
                # 保存图像
                img = Image.fromarray((X_test[i].reshape(640, 640) * 255).astype(np.uint8), 'L')
                img.save(os.path.join(save_error_dir, f"error_{i}_{true_label}_{pred_label}.png"))
                error_count += 1
                
        print(f"已保存 {error_count} 个错误样本到 {save_error_dir}")
    
    return accuracy

# ===== 主程序 =====
if __name__ == "__main__":
    # 创建模型
    model = create_model()
    
    # 训练模型
    model = train(model, epochs=100, font_dir="./fonts")  # 减少训练轮次（高分辨率训练更耗时）
    
    # 保存最终模型
    model.save("digit_model_640_final.keras")
    print("最终模型已保存")
    
    # 测试模型
    test(model, test_size=100, font_dir="./fonts", save_error_dir="./error_samples_640")