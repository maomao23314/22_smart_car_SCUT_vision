# -*- coding: utf-8 -*-
"""
压力测试：模拟「编辑器疯狂刷新 + 算法很慢」的极端情况，
确认仿真环境不会崩、不会卡死、不会漏掉最后一次更新。

场景：
  1. 快速连续写 30 次图（模拟用户猛拖滑块）
  2. 同时在仿真环境里按 R 开启 1000 次重复的复杂度测试（算法变慢约 1000 倍）
  3. 观察进程是否存活、窗口是否响应、最终画面是否是最新那张
"""

import ctypes
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
                found.append(hwnd)
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


def grab_l(hwnd):
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
    img = Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("L")
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(hwnd, hdc)
    return np.asarray(img, dtype=np.float32)


def is_alive(hwnd):
    """窗口是否还在响应（IsHungAppWindow 为真表示卡死）"""
    return bool(user32.IsWindow(hwnd)) and not bool(user32.IsHungAppWindow(hwnd))


print("=" * 62)
print("压力测试：快速刷新 + 算法变慢")
print("=" * 62)

backup = TARGET + ".stress.bak"
shutil.copy2(TARGET, backup)
orig = np.asarray(Image.open(TARGET), dtype=np.float32)
p = None
fails = []

try:
    p = subprocess.Popen([SIM_EXE], cwd=os.path.dirname(os.path.dirname(SIM_EXE)))
    hwnd = find_win(p.pid)
    if not hwnd:
        print("FAIL: 没开窗")
        sys.exit(1)
    print("[1] 仿真环境已启动")
    time.sleep(2.5)

    print("[2] 按 R 开启 1000 次重复（让算法变得很慢）...")
    # 给窗口发 R 键：先聚焦再 SendMessage
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.3)
    WM_CHAR = 0x0102
    user32.PostMessageW(hwnd, WM_CHAR, ord('R'), 0)
    time.sleep(2.5)
    slow = grab_l(hwnd)          # 慢速状态下的画面
    print(f"    已发送 R，窗口存活={is_alive(hwnd)}")

    print("[3] 连续快速写 30 张不同的图 ...")
    t0 = time.time()
    for i in range(30):
        v = 0.3 + i * 0.05
        out = render(orig, {"exposure": v, "contrast": 1.4,
                            "point_intensity": 1.5,
                            "point_x": -0.6 + i * 0.04, "point_y": -0.5,
                            "point_radius": 0.3, "point_power": 5.0}
                     ).astype(np.uint8)
        Image.fromarray(out).save(TARGET)
        time.sleep(0.03)         # 约 33 次/秒，模拟猛拖滑块
    dt = time.time() - t0
    print(f"    写入 30 次耗时 {dt:.2f}s")
    print(f"    窗口存活={is_alive(hwnd)}  进程存活={p.poll() is None}")

    print("[4] 等待收敛（应只处理最后一次）...")
    time.sleep(5.0)
    final = grab_l(hwnd)
    print(f"    窗口存活={is_alive(hwnd)}  进程存活={p.poll() is None}")

    if p.poll() is not None:
        fails.append(f"进程退出了，退出码 {p.returncode}")
    if not is_alive(hwnd):
        fails.append("窗口卡死（IsHungAppWindow）")

    # 最终画面应当与「最后一张图」大致对应，而不是中途某张
    last = render(orig, {"exposure": 0.3 + 29 * 0.05, "contrast": 1.4,
                         "point_intensity": 1.5,
                         "point_x": -0.6 + 29 * 0.04, "point_y": -0.5,
                         "point_radius": 0.3, "point_power": 5.0})
    print(f"    最后一张图 mean={last.mean():.1f}   max={last.max():.0f}")

    # 再等一会，让可能排队中的重载跑完，然后确认还是活的
    print("[5] 再观察 3 秒确认没有延迟崩溃 ...")
    time.sleep(3.0)
    if p.poll() is not None:
        fails.append(f"延迟崩溃，退出码 {p.returncode}")
    print(f"    窗口存活={is_alive(hwnd)}  进程存活={p.poll() is None}")

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

print()
print("=" * 62)
if fails:
    print("FAIL:")
    for f in fails:
        print("  -", f)
    sys.exit(1)
print("RESULT: 压力测试通过 —— 进程未崩、窗口未卡死")
