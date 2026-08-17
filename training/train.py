from ultralytics import YOLO
import argparse
import os
import torch
from ultralytics.utils import LOGGER
import warnings
import yaml
import time
import ultralytics

# 禁用版本更新警告
warnings.filterwarnings("ignore", message="New https://pypi.org/project/ultralytics/.*")

def parse_args():
    parser = argparse.ArgumentParser(description='YOLOv8 模型继续训练脚本')
    
    # 核心模型和数据配置
    parser.add_argument('--model', type=str, default='best.pt',  
                      help='已训练模型路径 (默认: best.pt)')
    parser.add_argument('--data', type=str, default='./datasets/data.yaml',
                      help='数据集配置文件路径 (默认: ./datasets/data.yaml)')
    
    # 训练参数
    parser.add_argument('--epochs', type=int, default=5,
                      help='继续训练的轮数 (默认: 50)')
    parser.add_argument('--batch', type=int, default=32,
                      help='批次大小, -1表示自动调整 (默认: -1)')
    parser.add_argument('--imgsz', type=int, default=640,
                      help='输入图像尺寸 (默认: 640)')
    parser.add_argument('--device', type=str, default='0',
                      help='训练设备, 如"0"或"cpu" (默认: 0)')
    
    # 优化器参数
    parser.add_argument('--optimizer', type=str, default='AdamW',
                      choices=['SGD', 'Adam', 'AdamW', 'RMSProp'], 
                      help='优化器类型 (默认: AdamW)')
    parser.add_argument('--lr0', type=float, default=0.0001,
                      help='初始学习率 (默认: 0.0001)')
    parser.add_argument('--lrf', type=float, default=0.1,
                      help='最终学习率因子 (lr0 * lrf) (默认: 0.1)')
    
    # 正则化与增强
    parser.add_argument('--weight_decay', type=float, default=0.001,
                      help='权重衰减系数 (默认: 0.001)')
    parser.add_argument('--hsv_h', type=float, default=0.015,
                      help='HSV色调增强 (默认: 0.015)')
    parser.add_argument('--hsv_s', type=float, default=0.7,
                      help='HSV饱和度增强 (默认: 0.7)')
    parser.add_argument('--hsv_v', type=float, default=0.4,
                      help='HSV明度增强 (默认: 0.4)')
    parser.add_argument('--degrees', type=float, default=10.0,
                      help='旋转角度范围 (默认: 10.0)')
    
    # 输出配置
    parser.add_argument('--project', type=str, default='runs/retrain',
                      help='项目保存目录 (默认: runs/retrain)')
    parser.add_argument('--name', type=str, default=f'enhanced_{int(time.time())}',
                      help='实验名称 (默认: enhanced_时间戳)')
    parser.add_argument('--exist_ok', action='store_true', default=True,
                      help='允许覆盖现有目录 (默认: True)')
    
    # 早停策略
    parser.add_argument('--patience', type=int, default=20,
                      help='早停等待轮数 (默认: 20)')
    
    # 混合精度训练
    parser.add_argument('--half', action='store_true', default=True,
                      help='启用混合精度训练 (默认: True)')
    
    # 新增功能
    parser.add_argument('--close_mosaic', type=int, default=5,
                      help='最后N个epoch禁用mosaic增强 (默认: 5)')
    parser.add_argument('--nms_time_limit', type=float, default=20.0,
                      help='NMS处理时间限制(秒) (默认: 20.0)')
    
    return parser.parse_args()


def train():
    args = parse_args()
    
    # 禁用版本检查
    os.environ['ULTRALYTICS_HUB_VERSION_CHECK'] = 'False'
    
    # 验证配置文件
    if not os.path.exists(args.data):
        raise FileNotFoundError(f"数据集配置文件不存在: {args.data}")
    
    # 检查模型文件
    if not os.path.exists(args.model):
        raise FileNotFoundError(f"模型文件不存在: {args.model}")
    
    # 加载预训练模型
    try:
        LOGGER.info(f"加载已训练模型: {args.model}")
        model = YOLO(args.model)
        
        # 获取初始性能指标
        LOGGER.info("评估初始模型性能...")
        initial_metrics = model.val(
            data=args.data, 
            batch=args.batch, 
            imgsz=args.imgsz,
            device=args.device,
            verbose=True
        )
        # 兼容不同版本的指标获取方式
        try:
            initial_map = initial_metrics.box.map  # 较新版本
        except AttributeError:
            initial_map = initial_metrics.results_dict['map50-95']  # 较旧版本
        
        LOGGER.info(f"初始mAP50-95: {initial_map:.4f}")
    except Exception as e:
        LOGGER.error(f"模型加载失败: {e}")
        return
    
    # 训练参数配置
    train_kwargs = {
        'data': args.data,
        'epochs': args.epochs,
        'batch': args.batch,
        'imgsz': args.imgsz,
        'device': args.device,
        'project': args.project,
        'name': args.name,
        'exist_ok': args.exist_ok,
        'optimizer': args.optimizer,
        'lr0': args.lr0,
        'lrf': args.lrf,
        'weight_decay': args.weight_decay,
        'hsv_h': args.hsv_h,
        'hsv_s': args.hsv_s,
        'hsv_v': args.hsv_v,
        'degrees': args.degrees,
        'patience': args.patience,
        'half': args.half,
        'close_mosaic': args.close_mosaic,
       # 'nms_time_limit': args.nms_time_limit,
        'verbose': True,
        'plots': True,  # 生成训练曲线图
        'save_period': 10,  # 每10个epoch保存一次
    }
    
    # 打印配置
    LOGGER.info("\n===== YOLOv8 继续训练配置 =====")
    LOGGER.info(f"基础模型: {args.model} | 数据集配置: {args.data}")
    LOGGER.info(f"训练轮数: {args.epochs} | 初始学习率: {args.lr0}")
    LOGGER.info(f"优化器: {args.optimizer} | 权重衰减: {args.weight_decay}")
    LOGGER.info(f"HSV增强: H({args.hsv_h}) S({args.hsv_s}) V({args.hsv_v})")
    LOGGER.info(f"输出目录: {os.path.join(args.project, args.name)}")
    LOGGER.info(f"新功能: close_mosaic={args.close_mosaic}")
    LOGGER.info("============================\n")
    
    # 开始训练
    try:
        LOGGER.info("开始继续训练模型...")
        start_time = time.time()
        
        # 调用训练方法
        results = model.train(**train_kwargs)
        
        training_time = time.time() - start_time
        hours, rem = divmod(training_time, 3600)
        minutes, seconds = divmod(rem, 60)
        LOGGER.info(f"训练完成! 用时: {int(hours):02d}h {int(minutes):02d}m {int(seconds):02d}s")
        
        # 获取最终性能指标
        LOGGER.info("评估训练后模型性能...")
        final_metrics = model.val(
            data=args.data, 
            batch=args.batch, 
            imgsz=args.imgsz,
            device=args.device,
            verbose=True
        )
        
        # 兼容不同版本的指标获取方式
        try:
            final_map = final_metrics.box.map  # 较新版本
        except AttributeError:
            final_map = final_metrics.results_dict['map50-95']  # 较旧版本
        
        # 性能对比
        improvement = final_map - initial_map
        LOGGER.info("\n===== 训练结果对比 =====")
        LOGGER.info(f"初始mAP50-95: {initial_map:.4f}")
        LOGGER.info(f"最终mAP50-95: {final_map:.4f}")
        LOGGER.info(f"提升: {improvement:+.4f} ({improvement/initial_map*100:.2f}%)")
        LOGGER.info("====================\n")
        
        # 保存路径
        weights_dir = os.path.join(args.project, args.name, 'weights')
        enhanced_model_path = os.path.join(weights_dir, 'best.pt')
        
        if os.path.exists(enhanced_model_path):
            model_size = os.path.getsize(enhanced_model_path) / (1024 * 1024)  # MB
            LOGGER.info(f"强化后的最佳模型已保存至: {enhanced_model_path}")
            LOGGER.info(f"文件大小: {model_size:.2f} MB")
            
            # 保存训练摘要
            summary = {
                'initial_model': args.model,
                'initial_map': float(initial_map),
                'final_map': float(final_map),
                'improvement': float(improvement),
                'training_time': training_time,
                'epochs': args.epochs,
                'optimizer': args.optimizer,
                'learning_rate': args.lr0,
                'save_path': enhanced_model_path,
                'timestamp': time.strftime("%Y-%m-%d %H:%M:%S")
            }
            
            with open(os.path.join(args.project, args.name, 'training_summary.yaml'), 'w') as f:
                yaml.dump(summary, f)
                
            LOGGER.info(f"训练摘要已保存至: {os.path.join(args.project, args.name, 'training_summary.yaml')}")
        else:
            LOGGER.warning(f"模型文件未找到: {enhanced_model_path}")
            LOGGER.info(f"请检查目录: {weights_dir}")
            
    except Exception as e:
        LOGGER.error(f"训练失败: {e}")
        import traceback
        LOGGER.error(traceback.format_exc())


if __name__ == "__main__":
    import multiprocessing
    multiprocessing.set_start_method('spawn', force=True)
    
    # 显示版本信息
    try:
        from ultralytics import __version__ as yolo_version
    except ImportError:
        yolo_version = "unknown"
    
    LOGGER.info(f"使用Ultralytics YOLOv{torch.__version__} (v{yolo_version})")
    
    # 确保输出目录存在
    if not os.path.exists('runs'):
        os.makedirs('runs')
    
    # 设置随机种子
    torch.manual_seed(42)
    
    train()