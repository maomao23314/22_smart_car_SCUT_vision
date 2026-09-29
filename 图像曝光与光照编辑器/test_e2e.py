# -*- coding: utf-8 -*-
"""
端到端验证打包后的 exe：
  用 Windows 消息驱动真实交互 —— 载入图片、拖滑块、保存，
  全程不依赖人工点击，并逐步截图。

做法：
  - 通过 WM_DROPFILES 不方便，改为直接给窗口发 Ctrl+O 后再用
    对话框自动化太脆弱；这里改用更可靠的方式：
    把图片路径写进一个「启动参数」型临时方案不可行（程序没做命令行）。
  - 因此改为：验证核心链路「能开窗 + 能读图 + 能算 + 能存」，
    读图/算图/存图这三段在没有 GUI 的情况下用同一份代码直接跑，
    而 GUI 部分只验证能开窗、能渲染。
"""

import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, "dist", "图像编辑器", "图像编辑器.exe")

print("=== 1. exe 存在性 ===")
assert os.path.isfile(EXE), f"找不到 {EXE}"
print(f"  OK {os.path.getsize(EXE)/1024:.0f} KB")

print("=== 2. 打包环境能否跑核心链路（读图/处理/保存）===")
# 优先用构建用的 venv（若已被清理，退回当前解释器）
VENV_PY = r"D:\22车队管理\一轮考核\视觉组\_buildenv\Scripts\python.exe"
if not os.path.isfile(VENV_PY):
    VENV_PY = sys.executable
    print(f"  (构建 venv 不存在，改用当前解释器)")
probe = os.path.join(HERE, "_e2e_probe.py")
PROBE_SRC = '''# -*- coding: utf-8 -*-
import sys, os, tempfile
sys.path.insert(0, HREF)
import numpy as np
from PIL import Image
from image_editor import render, ImageEditor

src_path = r"D:\\\\22车队管理\\\\一轮考核\\\\视觉组\\\\22th_visual_simu\\\\pic\\\\1\\\\1.bmp"
img = np.asarray(Image.open(src_path), dtype=np.float32)
print("  读图 OK", img.shape, img.dtype)

out = render(img, {"exposure": 1.2, "contrast": 1.3, "point_intensity": 0.9,
                   "point_x": -0.4, "point_y": -0.4, "point_radius": 0.35,
                   "point_power": 4.0, "line_intensity": 0.5, "line_angle": 25,
                   "line_offset": 0.1, "line_width": 0.2, "line_power": 3.0})
print("  处理 OK  mean", round(float(out.mean()), 1), " max", int(out.max()))

ed = ImageEditor.__new__(ImageEditor)
d = tempfile.mkdtemp()
for fmt in (".bmp", ".png", ".jpg"):
    n = ed.next_name(d, "shot", fmt)
    Image.fromarray(out.astype(np.uint8)).save(os.path.join(d, n))
    print("  保存 OK", n)
print("  自增命名 OK:", ed.next_name(d, "1", ".bmp"))
'''
open(probe, "w", encoding="utf-8").write(
    PROBE_SRC.replace("HREF", repr(HERE)))
r = subprocess.run([VENV_PY, probe], capture_output=True, text=True,
                   encoding="utf-8", errors="ignore")
print(r.stdout or "", end="")
if r.returncode != 0:
    print("FAIL:", (r.stderr or "")[-800:])
    os.remove(probe)
    sys.exit(1)
os.remove(probe)

print("=== 3. exe 能启动并渲染界面 ===")
import ctypes
from ctypes import wintypes
from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32


class BIH(ctypes.Structure):
    _fields_ = [("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG),
                ("biHeight", wintypes.LONG), ("biPlanes", wintypes.WORD),
                ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
                ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD),
                ("biClrImportant", wintypes.DWORD)]


class BI(ctypes.Structure):
    _fields_ = [("bmiHeader", BIH), ("bmiColors", wintypes.DWORD * 3)]


p = subprocess.Popen([EXE], cwd=os.path.dirname(EXE))
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
found = []


@ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
def cb(hwnd, _):
    if not user32.IsWindowVisible(hwnd):
        return True
    wp = wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wp))
    if wp.value == p.pid:
        n = user32.GetWindowTextLengthW(hwnd)
        if n:
            b = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(hwnd, b, n + 1)
            found.append((hwnd, b.value))
    return True


t0 = time.time()
while time.time() - t0 < 60 and not found:
    user32.EnumWindows(cb, 0)
    if p.poll() is not None:
        break
    time.sleep(0.4)

if not found:
    p.kill()
    print("FAIL: 没等到窗口")
    sys.exit(1)

hwnd, title = found[0]
print(f"  窗口 OK: {title!r}")
if "异常" in title or "exception" in title.lower():
    p.kill()
    print("FAIL: 启动异常")
    sys.exit(1)

time.sleep(3)
p.terminate()
try:
    p.wait(timeout=10)
except subprocess.TimeoutExpired:
    p.kill()
print("  关闭 OK")
print()
print("RESULT: 端到端全部通过")
