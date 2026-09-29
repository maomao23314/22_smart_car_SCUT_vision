# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller 打包配置。

【体积的关键】
不要在 Anaconda 环境里打包。Anaconda 的 numpy 链接 Intel MKL，
光数学内核 dll 就 633 MB，而本程序从未做过矩阵乘法。
改用官方 PyPI 的 numpy（OpenBLAS 版，约 31 MB）后，
未压缩产物从 680 MB 直接降到 ~45 MB，且不需要任何手工裁 dll。

本 spec 只用「排除确实用不到的重型包」这一种安全手段。
经验教训：不要排除标准库（http / xmlrpc / distutils 等），
        否则 pkg_resources、urllib 会连锁失败，exe 启动即崩。
"""

import os

# ---------------------------------------------------------------------------
# Tcl/Tk 资源
# ---------------------------------------------------------------------------
# venv 只复制了 python.exe，没有复制 _tkinter.pyd 和 Tcl/Tk 的 dll 与脚本库；
# 它们留在 base 解释器（这里是 Anaconda）里。PyInstaller 的 hook 在 venv 下
# 找不到这些文件，产物会以「DLL load failed while importing _tkinter」启动即崩。
# 下面显式补上：2 个 dll + tcl8.6 / tk8.6 脚本目录。
BASE = r"D:\TOOLS\anaconda3"          # venv 的 home，即提供 Tcl/Tk 的解释器
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
    # 科学计算大件（本程序只用 numpy 的逐元素运算）
    "scipy", "pandas", "matplotlib", "sympy", "sklearn", "skimage",
    "numba", "llvmlite", "cython", "pythran",
    # GUI 框架（界面用的是 Tkinter）
    "PyQt5", "PyQt6", "PySide2", "PySide6", "wx", "qtpy",
    # anaconda 生态
    "conda", "anaconda_navigator", "jupyter", "notebook", "IPython",
    "ipykernel", "nbformat", "nbconvert",
    # 测试框架
    "pytest", "nose", "hypothesis",
    # 图像处理里用不到的格式/显示支持
    "PIL.ImageQt", "PIL.ImageShow", "PIL.ImageGrab",
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

DROP_BINARIES = (
    # 用不到的图像编解码器（只保留 BMP/PNG/JPEG）
    "PIL\\_avif", "_avif.cp", "PIL\\_webp", "_webp.cp",
    "libwebp", "PIL\\_imagingcms",
    # TLS 相关：程序不联网（仿真联动将来用本地文件/本地 socket，不需要 OpenSSL）
    "libcrypto-3-x64", "libssl-3-x64",
    # Tcl/Tk 里用不到的解释器与包管理器
    "tclpip", "tkpip",
)
DROP_PREFIX_BIN = ("_avif", "_webp", "libwebp", "libcrypto-3", "libssl-3")


def _drop(dest: str) -> bool:
    low = dest.lower().replace("/", "\\")
    if low.endswith(".pyc"):
        return True
    base = low.rsplit("\\", 1)[-1]
    if base.startswith(DROP_PREFIX_BIN):
        return True
    return any(tok.lower() in low for tok in DROP_BINARIES)


a.binaries = [(d_, s_, k_) for (d_, s_, k_) in a.binaries if not _drop(d_)]

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="图像曝光与光照编辑器",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,             # 关 UPX：对已压缩的 dll 收益小，且易被杀软误报
    console=False,         # 不弹黑框
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=False,
    name="图像曝光与光照编辑器",
)
