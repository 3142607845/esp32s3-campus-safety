"""
最小化摄像头测试脚本
用于验证GC2093在K230D上的基本采集功能
"""

import sensor, image, time

print("Camera Test Starting...")

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.set_auto_gain(False)
sensor.set_auto_whitebal(False)
sensor.skip_frames(time=2000)

print("Camera initialized successfully!")
print("Resolution: {}x{}".format(sensor.width(), sensor.height()))

clock = time.clock()
count = 0

while count < 100:
    clock.tick()
    img = sensor.snapshot()
    count += 1
    
    if count % 10 == 0:
        print("Frame: {}, FPS: {:.1f}".format(count, clock.fps()))

print("Test completed!")