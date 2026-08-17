import os
import torch
import torch.nn as nn
import torch.nn.utils.prune as prune
import subprocess
import cv2
import numpy as np
from ultralytics import YOLO
from tqdm import tqdm

# =====================================
# 1. 模型剪枝（结构化通道剪枝）
# =====================================

def prune_yolov8_model(model_path, prune_ratio=0.3, save_path="pruned_yolov8.pt"):
    """修正：仅对卷积层权重进行结构化剪枝，跳过偏置参数"""
    model = YOLO(model_path)
    conv_layers = [m for m in model.model.modules() if isinstance(m, nn.Conv2d)]
    
    print(f"找到 {len(conv_layers)} 个卷积层，开始剪枝...")
    for layer in tqdm(conv_layers, desc="剪枝中"):
        # 只对weight进行结构化剪枝（关键修改）
        prune.ln_structured(layer, name="weight", amount=prune_ratio, n=1, dim=0)
        prune.remove(layer, "weight")  # 永久移除被剪枝的权重
        
        # 不对bias进行剪枝，避免一维张量错误
        
    model.save(save_path)
    print(f"剪枝完成，模型保存至: {save_path}")
    return save_path

# =====================================
# 2. 剪枝后微调
# =====================================

def finetune_pruned_model(pruned_model_path, data_yaml, epochs=10, lr=0.0001, batch=16, device=0):
    model = YOLO(pruned_model_path)
    print(f"开始微调剪枝后的模型，学习率: {lr}, 轮数: {epochs}")
    model.train(
        data=data_yaml,
        epochs=epochs,
        lr0=lr,
        batch=batch,
        device=device
    )
    finetuned_path = pruned_model_path.replace(".pt", "_finetuned.pt")
    model.save(finetuned_path)
    print(f"微调完成，模型保存至: {finetuned_path}")
    return finetuned_path

# =====================================
# 3. 模型量化（用命令行调用OpenVINO mo工具）
# =====================================

def quantize_model(finetuned_model_path, imgsz=640, save_dir="openvino_model"):
    model = YOLO(finetuned_model_path)
    onnx_path = finetuned_model_path.replace(".pt", ".onnx")
    model.export(format="onnx", imgsz=imgsz, simplify=True)
    print(f"已导出为ONNX格式: {onnx_path}")
    
    os.makedirs(save_dir, exist_ok=True)
    
    try:
        mo_cmd = f"mo --input_model {onnx_path} --input_shape [1,3,{imgsz},{imgsz}] --output_dir {save_dir} --data_type FP16"
        result = subprocess.run(
            mo_cmd,
            shell=True,
            check=True,
            capture_output=True,
            text=True
        )
        print("OpenVINO模型转换成功:")
        print(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"模型转换失败: {e.stderr}")
        raise
    
    ir_xml_path = os.path.join(save_dir, os.path.basename(onnx_path).replace(".onnx", ".xml"))
    print(f"OpenVINO IR模型已保存至: {ir_xml_path}")
    return ir_xml_path
# =====================================
# 主函数
# =====================================

def main():
    # 配置参数（替换为你的实际路径）
    original_model_path = "runs/train/exp/weights/best.pt"  # 你的YOLOv8模型
    data_yaml = "datasets/data.yaml"  # 你的数据集配置文件
    test_image = "test.jpg"   # 测试图像路径
    prune_ratio = 0.2         # 剪枝比例（建议从0.2开始）
    
    # 执行流程
    pruned_model = prune_yolov8_model(original_model_path, prune_ratio)
    finetuned_model = finetune_pruned_model(pruned_model, data_yaml)
    ir_model = quantize_model(finetuned_model)

if __name__ == "__main__":
    main()