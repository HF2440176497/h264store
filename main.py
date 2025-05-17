

# 模拟读取图片然后
import os
import time
import cv2
from pathlib import Path
import compressor


def get_image_files(image_path):
    images = []
    for entry in Path(image_path).iterdir():
        if entry.is_file():
            ext = entry.suffix.lower()
            if ext in ['.jpg', '.png', '.bmp']:
                images.append(str(entry))  # 将 Path 对象转换为字符串路径
    return images


if __name__ == "__main__":

    images = get_image_files("./data")

    queue_size = 10
    worker = compressor.PushWork(queue_size, 2432, 2048)
    worker.init()

    current_index = 0
    start_time = time.time()  # 获取当前时间戳

    display_time = 60
    while True:
        elapsed_time = time.time() - start_time
        if elapsed_time > display_time:
            break  # 超过显示时间则退出循环
            
        # 获取当前图片路径
        path = images[current_index]
            
        # 读取图片
        img = cv2.imread(path, cv2.IMREAD_COLOR)
        if img is None:
            print(f"Warning: Failed to read {path}")
            continue
        
        worker.put_data(img)

        current_index = (current_index + 1) % len(images)
        time.sleep(0.2)

    timeout_seconds = 3
    worker.stop(timeout_seconds)
