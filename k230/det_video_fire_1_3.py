# -*- coding: utf-8 -*-
'''
火焰实时检测（已修复"无火时残留检测框"问题）

基于勘智在线平台导出的 det_video_1_3.py 修改
适配硬件：正点原子 K230D + 2.4 寸 MIPI 屏
适配固件：CanMV v1.3.0（nncase 2.9.0）

修复思路：
    1. 每帧先清空 OSD 图层，擦掉上一帧画上去的框和文字
    2. DetectionApp 在没有检测到目标时会复用上一帧的 scores/boxes，
       通过"连续多帧检测框与分数完全一致"判定为 SDK 残留结果，此时不再绘制
    3. 既解决残框，也解决分数值卡住不更新
'''

import os, gc
from libs.PlatTasks import DetectionApp
from libs.PipeLine import PipeLine
from libs.Utils import *

# ==================== 可调参数 ====================
display_mode = "lcd"        # 2.4 寸 MIPI 屏；若黑屏改成 "st7701"
rgb888p_size = [640, 360]   # 摄像头输入分辨率，K230D 用 720p 会掉帧

# 连续多少帧结果完全相同，就判定为 SDK 复用的残留结果（停止显示）
# 调大更保守（真实静止火焰不容易被误清），调小响应更快
STALE_FRAMES = 5

# ==================== 模型配置 ====================
root_path = "/sdcard/mp_deployment_source/"
deploy_conf = read_json(root_path + "deploy_config.json")

kmodel_path = root_path + deploy_conf["kmodel_path"]
labels = deploy_conf["categories"]
confidence_threshold = deploy_conf["confidence_threshold"]
nms_threshold = deploy_conf["nms_threshold"]
model_input_size = deploy_conf["img_size"]
nms_option = deploy_conf["nms_option"]
model_type = deploy_conf["model_type"]

anchors = []
if model_type == "AnchorBaseDet":
    anchors = (deploy_conf["anchors"][0]
               + deploy_conf["anchors"][1]
               + deploy_conf["anchors"][2])

inference_mode = "video"
debug_mode = 0

# ==================== 初始化 ====================
pl = PipeLine(rgb888p_size=rgb888p_size, display_mode=display_mode)
pl.create()
display_size = pl.get_display_size()

det_app = DetectionApp(inference_mode, kmodel_path, labels, model_input_size, anchors,
                       model_type, confidence_threshold, nms_threshold,
                       rgb888p_size, display_size, debug_mode=debug_mode)
det_app.config_preprocess()


def clear_osd():
    """清空 OSD 图层，擦掉上一帧画的框和文字"""
    try:
        pl.osd_img.clear()
        return
    except Exception:
        pass
    # 退路：用全透明色整块覆盖
    try:
        w = pl.osd_img.width()
        h = pl.osd_img.height()
        pl.osd_img.draw_rectangle(0, 0, w - 1, h - 1, color=(0, 0, 0, 0), fill=True)
    except Exception:
        pass


def result_key(scores, boxes, n):
    """把检测结果转成可比较的元组，用来判断结果是否被 SDK 复用"""
    try:
        key = []
        for i in range(n):
            b = boxes[i]
            s = round(float(scores[i]), 3)
            key.append((int(b[0]), int(b[1]), int(b[2]), int(b[3]), s))
        return tuple(key)
    except Exception:
        return None


last_key = None
same_cnt = 0

# ==================== 主循环 ====================
while True:
    with ScopedTiming("total", 1):
        img = pl.get_frame()            # 采集一帧
        res = det_app.run(img)          # 推理

        # 兼容 SDK 返回 dict 的情况
        if isinstance(res, dict):
            scores = res.get("scores", [])
            boxes = res.get("boxes", [])
        else:
            scores, boxes = [], []
        n = min(len(scores), len(boxes))

        # 判断本帧结果是否与上一帧完全相同
        cur_key = result_key(scores, boxes, n) if n > 0 else None
        if cur_key is None:
            same_cnt = 0
            last_key = None
        elif cur_key == last_key:
            same_cnt += 1
        else:
            same_cnt = 0
            last_key = cur_key

        # 没有检测结果，或结果连续多帧完全不变（SDK 残留），都不画
        is_stale = (n == 0) or (same_cnt >= STALE_FRAMES)

        clear_osd()                     # 先擦掉上一帧的绘制内容
        if not is_stale:
            det_app.draw_result(pl.osd_img, res)
        pl.show_image()

        gc.collect()

# 异常退出时清理
det_app.deinit()
pl.destroy()
