# -*- coding: utf-8 -*-
"""
端到端验证：图像编辑器 → 仿真环境 的热重载联动。

流程：
  1. 把图集复制一份到临时目录，避免动到真实 pic\
  2. 用临时的 config.h 指向该目录，编译一个测试版仿真环境
     （不改动原工程，编译到独立输出目录）
  3. 启动仿真环境
  4. 用编辑器写一张「曝光明显不同」的图到当前显示的文件上
  5. 抓仿真环境窗口截图，确认画面确实变了

简化做法：不重新编译，直接用已编译好的 exe，
配合环境变量/命令行无法改路径 —— 所以改为直接对真实 pic\1\1.bmp 操作，
测试完恢复原图。
"""

import ctypes
import glob
import os
import shutil
import subprocess
import sys
import time
from ctypes import wintypes

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SIM_EXE = os.path.join(ROOT, "22th_visual_simu", "VS", "x64", "Release",
                       "visual_simu.exe")
TARGET = os.path.join(ROOT, "22th_visual_simu", "pic", "1", "1.bmp")

sys.path.insert(0, HERE)
from image_editor import render

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
user32.GetWindowThreadProcessId.restype = wintypes.DWORD


class BIH(ctypes.Structure):
    _fields_ = [("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG),
                ("biHeight", wintypes.LONG), ("biPlanes", wintypes.WORD),
                ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
                ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
                ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD),
                ("biClrImportant", wintypes.DWORD)]


class BI(ctypes.Structure):
    _fields_ = [("bmiHeader", BIH), ("bmiColors", wintypes.DWORD * 3)]


def find_win(pid, timeout=40):
    found = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    def cb(hwnd, _):
        if not user32.IsWindowVisible(hwnd):
            return True
        wp = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wp))
        if wp.value == pid:
            n = user32.GetWindowTextLengthW(hwnd)
            if n:
                b = ctypes.create_unicode_buffer(n + 2)
                user32.GetWindowTextW(hwnd, b, n + 2)
                found.append((hwnd, b.value))
        return True

    t0 = time.time()
    while time.time() - t0 < timeout:
        found.clear()
        user32.EnumWindows(cb, 0)
        if found:
            return found[0]
        if p.poll() is not None:
            return None
        time.sleep(0.3)
    return None


def grab(hwnd, path):
    r = wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    w, h = r.right - r.left, r.bottom - r.top
    hdc = user32.GetWindowDC(hwnd)
    mem = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(mem, bmp)
    user32.PrintWindow(hwnd, mem, 2)
    bi = BI()
    bi.bmiHeader.biSize = ctypes.sizeof(BIH)
    bi.bmiHeader.biWidth = w
    bi.bmiHeader.biHeight = -h
    bi.bmiHeader.biPlanes = 1
    bi.bmiHeader.biBitCount = 32
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(bi), 0)
    img = Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("RGB")
    img.save(path)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(hwnd, hdc)
    return img


print("=" * 60)
print("联动测试：编辑器写图 -> 仿真环境自动重载")
print("=" * 60)

if not os.path.isfile(SIM_EXE):
    print("FAIL: 找不到仿真环境 exe，先编译")
    sys.exit(1)
if not os.path.isfile(TARGET):
    print(f"FAIL: 找不到目标图 {TARGET}")
    sys.exit(1)

# 备份原图
backup = TARGET + ".linktest.bak"
shutil.copy2(TARGET, backup)
orig = np.asarray(Image.open(TARGET), dtype=np.float32)
print(f"目标图 {os.path.basename(TARGET)}  {orig.shape}  已备份")

p = None
try:
    print("\n[1] 启动仿真环境 ...")
    p = subprocess.Popen([SIM_EXE], cwd=os.path.dirname(os.path.dirname(SIM_EXE)))
    win = find_win(p.pid)
    if not win:
        print("FAIL: 仿真环境没开窗")
        sys.exit(1)
    hwnd, title = win
    print(f"    OK {title!r}")
    time.sleep(2.5)

    img_before = grab(hwnd, os.path.join(HERE, "link_before.png"))
    print(f"    OK 截图 before {img_before.size}")

    print("\n[2] 用编辑器把图改成「极暗 + 强烈点光斑」...")
    new = render(orig, {"exposure": 0.25, "contrast": 1.6,
                        "point_intensity": 2.0, "point_x": -0.5, "point_y": -0.5,
                        "point_radius": 0.28, "point_power": 6.0}).astype(np.uint8)
    Image.fromarray(new).save(TARGET)
    print(f"    OK 已写入 {os.path.basename(TARGET)}  "
          f"(原均值 {orig.mean():.1f} -> 新均值 {new.mean():.1f})")

    print("\n[3] 等待仿真环境自动重载 ...")
    time.sleep(3.0)
    img_after = grab(hwnd, os.path.join(HERE, "link_after.png"))
    print(f"    OK 截图 after {img_after.size}")

    # 比较两张截图的图像区域是否明显不同
    a = np.asarray(img_before.convert("L"), dtype=np.float32)
    b = np.asarray(img_after.convert("L"), dtype=np.float32)
    if a.shape != b.shape:
        print("    截图尺寸不同，跳过像素比较")
    else:
        diff = float(np.abs(a - b).mean())
        print(f"    截图平均差异 = {diff:.2f}")
        if diff > 0.5:
            print("    => 画面确实变了，热重载生效")
        else:
            print("    => 画面几乎没变，热重载可能没生效")

    print("\n[4] 检查是否崩溃 ...")
    if p.poll() is not None:
        print(f"    FAIL: 仿真环境已退出，退出码 {p.returncode}")
    else:
        print("    OK 进程仍在运行")

finally:
    if p is not None:
        try:
            p.terminate()
            p.wait(timeout=8)
        except Exception:
            try:
                p.kill()
            except Exception:
                pass
    if os.path.isfile(backup):
        shutil.copy2(backup, TARGET)
        os.remove(backup)
        print("\n已恢复原图")

print("=" * 60)
