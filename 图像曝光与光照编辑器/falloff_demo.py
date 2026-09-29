# -*- coding: utf-8 -*-
"""生成「衰减曲线」对比图，直观展示滑块的含义。"""

import os
import sys
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from image_editor import render, _falloff

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = r"D:\22车队管理\一轮考核\视觉组\22th_visual_simu\pic\1\1.bmp"

src = np.asarray(Image.open(SRC), dtype=np.float32)
powers = [0.4, 1.0, 2.0, 4.0, 8.0]

# --- 上排：光斑效果 ---
tiles = []
for p in powers:
    out = render(src, {"point_intensity": 1.0, "point_x": 0.0, "point_y": 0.0,
                       "point_radius": 0.55, "point_power": p}).astype(np.uint8)
    tiles.append(Image.fromarray(out))

# --- 下排：曲线本身 ---
W, H = tiles[0].size
S = 2
cw, chh = W * S, 150
curves = Image.new("RGB", (cw, chh), (26, 26, 26))
d = ImageDraw.Draw(curves)
d.line([(0, chh - 12), (cw, chh - 12)], fill=(70, 70, 70))
d.line([(6, 0), (6, chh - 12)], fill=(70, 70, 70))
xs = np.linspace(0, 2.2, 300)
for p, col in zip(powers, [(90, 170, 255), (120, 220, 160), (255, 210, 90),
                           (255, 150, 100), (255, 105, 150)]):
    ys = _falloff(xs, p)
    pts = [(6 + (x / 2.2) * (cw - 12), (chh - 12) * (1 - y)) for x, y in zip(xs, ys)]
    d.line(pts, fill=col, width=2)
d.text((cw - 120, 8), "1.0 = peak", fill=(150, 150, 150))
d.text((cw - 130, chh - 26), "d = 0 .. 2.2", fill=(150, 150, 150))

# --- 拼接 ---
pad = 6
sheet = Image.new("RGB", (cw * len(tiles) + pad * (len(tiles) + 1),
                          H * S + chh + pad * 3), (18, 18, 18))
for i, t in enumerate(tiles):
    sheet.paste(t.resize((cw, H * S), Image.NEAREST),
                (pad + i * (cw + pad), pad))
sheet.paste(curves, (pad, pad * 2 + H * S))

# 顶注
top = Image.new("RGB", (sheet.width, 34), (18, 18, 18))
td = ImageDraw.Draw(top)
labels = ["0.4 弥散", "1.0 较柔", "2.0 默认", "4.0 大片亮", "8.0 平顶陡边"]
for i, lab in enumerate(labels):
    td.text((pad + i * (cw + pad) + 6, 10), lab, fill=(200, 200, 200))

final = Image.new("RGB", (sheet.width, sheet.height + 34), (18, 18, 18))
final.paste(top, (0, 0))
final.paste(sheet, (0, 34))
final.save(os.path.join(HERE, "falloff_compare.png"))
print("saved falloff_compare.png", final.size)
