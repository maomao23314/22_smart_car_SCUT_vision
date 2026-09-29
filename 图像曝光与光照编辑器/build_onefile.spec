# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller 打包配置 —— 单文件模式（--onefile）。

把 _internal 里的全部依赖压进 exe 自身，产出一个独立的 .exe，
双击即用，旁边没有依赖文件夹。

代价（权衡后仍推荐，理由见下方注释）：
  - 启动时要先把内部依赖解压到 %TEMP%\\_MEIxxxxx，首次启动慢 2~5 秒
  - 每次运行都会在临时目录留一份，退出后由 PyInstaller 自行清理
  - 单文件体积 = onedir 总体积再压缩一层，通常略小于 onedir 目录

实测对比（本项目）：
  onedir : 58.4 MB 目录 + 启动约 1 秒
  onefile: 见构建输出，启动约 3~4 秒

Tcl/Tk 的处理与 onedir 版一致：venv 不含 _tkinter.pyd 与 tcl/tk dll，
必须从 base 解释器显式补进来，否则启动报
「DLL load failed while importing _tkinter」。
"""

import os

# ---------------------------------------------------------------------------
# Tcl/Tk 资源（来自提供 Tcl/Tk 的 base 解释器）
# ---------------------------------------------------------------------------
BASE = r"D:\TOOLS\anaconda3"          # 换机器打包时改这里
TCL_BIN = os.path.join(BASE, "Library", "bin")
TCL_LIB = os.path.join(BASE, "Library", "lib")

extra_binaries = [
    (os.path.join(TCL_BIN, "tcl86t.dll"), "."),
    (os.path.join(TCL_BIN, "tk86t.dll"), "."),
]
extra_datas = [
    (os.path.join(TCL_LIB, "tcl8.6"), "tcl8.6"),
    (os.path.join(TCL_LIB, "tk8.6"), "tk8.6"),
]

EXCLUDES = [
    # 科学计算大件（只用 numpy 的逐元素运算）
    "scipy", "pandas", "matplotlib", "sympy", "sklearn", "skimage",
    "numba", "llvmlite", "cython", "pythran",
    # GUI 框架（界面用的是 Tkinter）
    "PyQt5", "PyQt6", "PySide2", "PySide6", "wx", "qtpy",
    # anaconda 生态
    "conda", "anaconda_navigator", "jupyter", "notebook", "IPython",
    "ipykernel", "nbformat", "nbconvert",
    # 测试框架
    "pytest", "nose", "hypothesis",
    # 用不到的图像格式与显示支持
    "PIL.ImageQt", "PIL.ImageShow", "PIL.ImageGrab",
    "PIL._avif", "PIL._webp",
    # TLS：程序不联网
    "ssl", "_ssl",
]

a = Analysis(
    ["image_editor.py"],
    pathex=[],
    binaries=extra_binaries,
    datas=extra_datas,
    hiddenimports=["_tkinter", "tkinter", "tkinter.filedialog",
                   "tkinter.messagebox", "tkinter.ttk"],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=EXCLUDES,
    noarchive=False,
)

# 剔除无用二进制
DROP_BIN = ("libcrypto-3-x64", "libssl-3-x64", "libwebp", "_avif", "_webp",
            "_imagingcms")


def _drop(dest: str) -> bool:
    low = dest.lower().replace("/", "\\")
    if low.endswith(".pyc"):
        return True
    base = low.rsplit("\\", 1)[-1]
    return any(base.startswith(t) for t in DROP_BIN)


a.binaries = [(d_, s_, k_) for (d_, s_, k_) in a.binaries if not _drop(d_)]

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,          # onefile：依赖打进 exe
    a.datas,
    [],
    name="图像曝光与光照编辑器",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    runtime_tmpdir=None,     # None = 用系统 %TEMP%
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,
)
