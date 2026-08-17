import cv2
import os
import sys

def resize_image(input_path, output_path, size=(640, 640)):
    img = cv2.imread(input_path)
    resized = cv2.resize(img, size)
    cv2.imwrite(output_path, resized)

if __name__ == "__main__":
    in_dir = "InPictures"
    out_dir = "OutPictures"

    for fname in os.listdir(in_dir):
        in_path = os.path.join(in_dir, fname)
        out_path = os.path.join(out_dir, fname)
        resize_image(in_path, out_path)
