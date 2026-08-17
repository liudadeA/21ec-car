import os
from collections import defaultdict

def count_classes_in_yolo_labels(label_dir):
    """统计YOLO标签文件夹中每个类别的出现次数"""
    class_counts = defaultdict(int)
    total_files = 0
    empty_files = 0

    # 检查标签文件夹是否存在
    if not os.path.exists(label_dir):
        print(f"错误：标签文件夹 '{label_dir}' 不存在")
        return None, 0, 0

    # 获取所有txt文件
    txt_files = [f for f in os.listdir(label_dir) if f.endswith('.txt')]

    for txt_file in txt_files:
        total_files += 1
        file_path = os.path.join(label_dir, txt_file)
        
        # 跳过可能存在的classes.txt文件
        if os.path.basename(file_path) == 'classes.txt':
            continue
            
        try:
            with open(file_path, 'r') as f:
                lines = f.readlines()
                
                # 检查文件是否为空
                if not lines:
                    empty_files += 1
                    continue
                    
                # 统计每个类别的出现次数
                for line in lines:
                    parts = line.strip().split()
                    if parts:  # 确保行不为空
                        class_id = int(parts[0])
                        class_counts[class_id] += 1
                        
        except Exception as e:
            print(f"处理文件 {txt_file} 时出错: {e}")

    return class_counts, total_files, empty_files

def main():
    # 直接在代码中指定文件夹路径和类别文件路径
    LABEL_DIR = 'datasets/labels/val'  # 修改为你的标签文件夹路径
    CLASSES_FILE = ''  # 修改为你的类别文件路径（可选）

    # 统计类别
    class_counts, total_files, empty_files = count_classes_in_yolo_labels(LABEL_DIR)
    
    if class_counts is None:
        return

    # 读取类别名称（如果提供了类别文件）
    class_names = {}
    if CLASSES_FILE and os.path.exists(CLASSES_FILE):
        with open(CLASSES_FILE, 'r') as f:
            class_names = {i: name.strip() for i, name in enumerate(f.readlines())}

    # 打印统计结果
    print(f"\n{'类别ID':<10}{'类别名称':<20}{'出现次数':<15}")
    print("-" * 45)
    
    # 按类别ID排序
    for class_id in sorted(class_counts.keys()):
        count = class_counts[class_id]
        class_name = class_names.get(class_id, f'未命名类别 {class_id}')
        print(f"{class_id:<10}{class_name:<20}{count:<15}")
    
    # 打印统计摘要
    print("\n统计摘要:")
    print(f"总标签文件数: {total_files}")
    print(f"空标签文件数: {empty_files}")
    print(f"总类别数: {len(class_counts)}")
    print(f"总标注对象数: {sum(class_counts.values())}")

if __name__ == "__main__":
    main()    