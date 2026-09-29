# -*- coding: utf-8 -*-
"""验证重新打包后的 onefile exe：能启动、能开窗、界面含仿真联动区。"""

import ctypes
import os
import subprocess
import time
from ctypes import wintypes

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, "dist_onefile", "图像编辑器.exe")

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


def all_pids(name):
    r = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {name}", "/FO", "CSV"],
                       capture_output=True, text=True, encoding="gbk",
                       errors="ignore")
    out = []
    for line in r.stdout.splitlines()[1:]:
        p = [x.strip('"') for x in line.split('","')]
        if len(p) >= 2 and p[1].isdigit():
            out.append(int(p[1]))
    return out


print("启动 onefile exe ...")
subprocess.Popen([EXE], cwd=os.path.dirname(EXE))

win = None
t0 = time.time()
while time.time() - t0 < 90:
    pids = set(all_pids("图像编辑器.exe"))
    if not pids:
        time.sleep(0.3)
        continue
    hits = []

    @ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    def cb(hwnd, _):
        if not user32.IsWindowVisible(hwnd):
            return True
        wp = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(wp))
        if wp.value in pids:
            n = user32.GetWindowTextLengthW(hwnd)
            if n:
                b = ctypes.create_unicode_buffer(n + 2)
                user32.GetWindowTextW(hwnd, b, n + 2)
                hits.append((hwnd, b.value))
        return True

    user32.EnumWindows(cb, 0)
    if hits:
        win = hits[0]
        break
    time.sleep(0.3)

if not win:
    subprocess.run(["taskkill", "/F", "/IM", "图像编辑器.exe"], capture_output=True)
    print("FAIL: 90 秒内没开窗")
    raise SystemExit(1)

hwnd, title = win
print(f"OK 开窗 {time.time()-t0:.1f}s  标题 {title!r}")
if "异常" in title or "exception" in title.lower():
    subprocess.run(["taskkill", "/F", "/IM", "图像编辑器.exe"], capture_output=True)
    print("FAIL: 启动异常")
    raise SystemExit(1)

time.sleep(3.0)

# 截图确认界面
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
Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("RGB").save(
    os.path.join(HERE, "exe_shot.png"))
print(f"OK 截图 {w}x{h} -> exe_shot.png")

subprocess.run(["taskkill", "/F", "/IM", "图像编辑器.exe"], capture_output=True)
print("RESULT: onefile 包可正常启动")
