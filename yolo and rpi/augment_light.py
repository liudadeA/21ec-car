import cv2
import os
import numpy as np

def adjust_brightness(img, factor):
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    hsv = np.array(hsv, dtype=np.float32)
    hsv[...,2] = hsv[...,2] * factor
    hsv[...,2][hsv[...,2]>255] = 255
    hsv = np.array(hsv, dtype=np.uint8)
    return cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)

import random

def augment_lighting(input_dir, output_dir, num_aug=5, min_factor=0.5, max_factor=1.5):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    for fname in os.listdir(input_dir):
        in_path = os.path.join(input_dir, fname)
        if not os.path.isfile(in_path):
            continue
        img = cv2.imread(in_path)
        if img is None:
            continue
        for i in range(num_aug):
            f = random.uniform(min_factor, max_factor)
            out_name = f"{os.path.splitext(fname)[0]}_{i+1}.jpg"
            out_path = os.path.join(output_dir, out_name)
            bright_img = adjust_brightness(img, f)
            cv2.imwrite(out_path, bright_img)

if __name__ == "__main__":
    augment_lighting('datasets/images/val', 'datasets/images/train')
