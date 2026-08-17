import os
import shutil

val_dir = 'datasets/labels/val'
train_dir = 'datasets/labels/train'

if not os.path.exists(train_dir):
    os.makedirs(train_dir)

for fname in os.listdir(val_dir):
    in_path = os.path.join(val_dir, fname)
    if not os.path.isfile(in_path) or not fname.endswith('.txt'):
        continue
    base = os.path.splitext(fname)[0]
    for i in range(5):
        out_name = f"{base}_{i+1}.txt"
        out_path = os.path.join(train_dir, out_name)
        shutil.copyfile(in_path, out_path)
        print(f"复制: {in_path} -> {out_path}")
