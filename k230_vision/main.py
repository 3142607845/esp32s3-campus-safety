"""
2025年全国大学生电子设计竞赛E题 - 简易自行瞄准装置
视觉部分代码（基于K230D开发板 + GC2093摄像头）

功能：识别红色靶心并通过UART发送坐标给MSPM0控制器
分辨率：QVGA 320x240
通信协议：帧头(0xAA) + X坐标 + Y坐标 + 校验和 + 帧尾(0x55)
"""

import sensor, image, time, ustruct
from fpioa_manager import fm
from machine import UART

# ==================== 配置参数 ====================
# 串口配置
UART_PORT = 1
UART_TX_PIN = 40
UART_RX_PIN = 41
BAUD_RATE = 115200

# 图像配置
IMAGE_WIDTH = 320
IMAGE_HEIGHT = 240

# 颜色阈值（红色靶心）
RED_THRESHOLD = (30, 100, 15, 60, -10, 30)

# 靶心检测参数
MIN_BLOB_AREA = 10
MAX_BLOB_AREA = 1000
MIN_BLOB_CIRCULARITY = 0.3

# 坐标映射参数（需要现场标定）
X_SCALE = 0.15
Y_SCALE = 0.20
CENTER_X = IMAGE_WIDTH // 2
CENTER_Y = IMAGE_HEIGHT // 2

# 通信协议定义
FRAME_HEADER = 0xAA
FRAME_FOOTER = 0x55

# ==================== 全局变量 ====================
uart = None
target_found = False
target_x = 0
target_y = 0

# ==================== 初始化函数 ====================

def init_uart():
    global uart
    fm.register(UART_TX_PIN, fm.fpioa.UART1_TX, force=True)
    fm.register(UART_RX_PIN, fm.fpioa.UART1_RX, force=True)
    uart = UART(UART_PORT, BAUD_RATE, timeout=10)
    print("UART1 initialized: TX=IO{}, RX=IO{}, Baud={}".format(
        UART_TX_PIN, UART_RX_PIN, BAUD_RATE))

def init_camera():
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.set_vflip(False)
    sensor.set_hmirror(False)
    sensor.set_brightness(0)
    sensor.set_contrast(3)
    sensor.set_saturation(3)
    sensor.set_auto_gain(False)
    sensor.set_auto_whitebal(False)
    sensor.skip_frames(time=2000)
    print("Camera initialized: GC2093, {}x{}".format(IMAGE_WIDTH, IMAGE_HEIGHT))

# ==================== 图像处理函数 ====================

def detect_target(img):
    global target_found, target_x, target_y

    blobs = img.find_blobs([RED_THRESHOLD],
                           roi=(0, 0, IMAGE_WIDTH, IMAGE_HEIGHT),
                           area_threshold=MIN_BLOB_AREA,
                           pixels_threshold=MIN_BLOB_AREA,
                           merge=True,
                           margin=10)

    if blobs:
        largest_blob = max(blobs, key=lambda b: b.area())

        if largest_blob.circularity() > MIN_BLOB_CIRCULARITY:
            target_x = largest_blob.cx()
            target_y = largest_blob.cy()
            target_found = True

            img.draw_rectangle(largest_blob.rect(), color=(255, 0, 0), thickness=2)
            img.draw_cross(target_x, target_y, color=(0, 255, 0), size=10, thickness=2)

            return (True, target_x, target_y)

    target_found = False
    return (False, 0, 0)

def calculate_angle_offset(pixel_x, pixel_y):
    dx = pixel_x - CENTER_X
    dy = pixel_y - CENTER_Y

    angle_x = dx * X_SCALE
    angle_y = dy * Y_SCALE

    return (angle_x, angle_y)

# ==================== 通信函数 ====================

def send_target_data(angle_x, angle_y, found):
    if uart is None:
        return

    angle_x_int = int(angle_x * 10)
    angle_y_int = int(angle_y * 10)

    data = ustruct.pack('<hhb', angle_x_int, angle_y_int, 1 if found else 0)

    checksum = 0
    for byte in data:
        checksum ^= byte

    frame = bytearray()
    frame.append(FRAME_HEADER)
    frame.extend(data)
    frame.append(checksum)
    frame.append(FRAME_FOOTER)

    uart.write(frame)

def parse_uart_command():
    if uart is None:
        return

    if uart.any():
        data = uart.read(10)
        if data and len(data) >= 3:
            if data[0] == FRAME_HEADER and data[-1] == FRAME_FOOTER:
                cmd = data[1]
                if cmd == 0x01:
                    status = 1 if target_found else 0
                    resp = bytearray([FRAME_HEADER, 0x01, status, 0x01^status, FRAME_FOOTER])
                    uart.write(resp)
                elif cmd == 0x02:
                    print("Received reset command")

# ==================== 主函数 ====================

def main():
    print("=" * 40)
    print("2025电赛E题 - 简易自行瞄准装置")
    print("视觉模块启动中...")
    print("=" * 40)

    init_uart()
    init_camera()

    clock = time.clock()

    print("视觉模块启动完成！")
    print("开始检测靶心...")

    while True:
        clock.tick()

        img = sensor.snapshot()

        found, x, y = detect_target(img)

        angle_x, angle_y = calculate_angle_offset(x, y)

        send_target_data(angle_x, angle_y, found)

        parse_uart_command()

        if found:
            img.draw_string_advanced(0, 0,
                "Target: ({}, {})".format(x, y),
                color=(0, 255, 0),
                scale=1,
                bg_color=(0, 0, 0))
            img.draw_string_advanced(0, 20,
                "Angle: X={:.1f} Y={:.1f}".format(angle_x, angle_y),
                color=(0, 255, 0),
                scale=1,
                bg_color=(0, 0, 0))
        else:
            img.draw_string_advanced(0, 0,
                "No Target",
                color=(255, 0, 0),
                scale=1,
                bg_color=(0, 0, 0))

        print("FPS: {:.1f}, Target: {}".format(clock.fps(), "FOUND" if found else "NOT FOUND"))

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print("Error: {}".format(e))
        import sys
        sys.print_exception(e)