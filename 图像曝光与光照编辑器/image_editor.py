# -*- coding: utf-8 -*-
"""
================================================================================
 图像曝光与光照编辑器  (Image Exposure & Lighting Editor)
--------------------------------------------------------------------------------
 华南理工大学 智能车队  22 届招新 · 竞速组视觉方向
--------------------------------------------------------------------------------
 本工具是「22 届视觉仿真环境」的配套造图工具，用来给仿真环境合成各种
 光照条件下的赛道图，方便视觉组同学调试阈值、边线、中线等算法。

 为什么要它：
   仿真环境的图集是固定的，但真实赛道的光照千变万化——逆光、炫光、
   水面反光、黄昏、隧道口明暗交替……不去专门造这些图，算法在赛场上
   就会翻车。本工具让你在一张图上快速模拟这些情况。

 功能：
   1. 读取图像并显示（BMP / PNG / JPG），支持同目录翻页
   2. 右侧控件（三个并列的可折叠区块）：
        - 基础光照：曝光（模拟曝光时间）、对比度
        - 光斑：点状扩散光斑（模拟阳光直射）、线性条状高光（模拟反光）
        - 高级参数：光斑位置 / 半径 / 衰减曲线、反光角度 / 条宽
   3. 底部保存条：自由命名 + 自增 + 覆盖询问，支持 BMP / PNG / JPG
   4. 仿真联动：处理结果可直接写回仿真环境正在显示的图片，
      仿真环境会自动重新读图并重跑算法（见 env\\hot_reload.c）

 技术栈：Python + Tkinter + numpy + Pillow
   —— 全部为可自由使用的开源组件，无需安装任何商业软件。

 设计要点（为后续替换实现留的口子）：
   - render() 是纯函数：输入原图 ndarray 和参数字典，输出处理后的 ndarray
     将来换 C++/ImGui 或改公式，只动这一个函数
   - 所有界面参数由 PARAM_SPECS 一张表驱动，加参数只需加一行
   - 预览与保存走同一套 render()，保证所见即所得

 作者：华南理工大学智能车队 22 届视觉组
 许可：GPL-3.0（见仓库根目录 LICENSE）
================================================================================
"""

import os
import re
import time
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

import numpy as np
from PIL import Image, ImageTk


# ============================================================================
# 图像处理核心（纯函数，不依赖任何 GUI）
# ============================================================================

def render(img: np.ndarray, params: dict) -> np.ndarray:
    """
    对图像施加光照/曝光效果。

    img    : float32 数组，形状 (H, W, 3) 或 (H, W)，取值 0~255
    params : 参数字典，见下面各键
    返回   : 与输入同形状的 float32 数组，已裁剪到 0~255

    光照模型（按顺序叠加）：
      1) 曝光    —— 整体乘性增益，模拟曝光时间
      2) 对比度  —— 围绕中灰做 S 曲线拉伸
      3) 点光源  —— 距离场衰减，中心最亮，模拟阳光光斑
      4) 线性反光 —— 沿法线方向的投影距离做衰减，模拟条状高光
    """
    out = img.astype(np.float32).copy()
    is_gray = (out.ndim == 2)
    h, w = out.shape[:2]

    # --- 1. 曝光：整体亮度增益 -------------------------------------------------
    gain = params.get("exposure", 1.0)
    out *= gain

    # --- 2. 对比度：围绕中灰 128 做线性拉伸 ------------------------------------
    # contrast = 1 不变；>1 更硬（暗的更暗亮的更亮）；<1 更灰
    contrast = params.get("contrast", 1.0)
    if abs(contrast - 1.0) > 1e-6:
        out = (out - 128.0) * contrast + 128.0

    # 预计算坐标网格（归一化到 -1 ~ 1，与分辨率无关）
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    nx = (xs / max(w - 1, 1)) * 2.0 - 1.0
    ny = (ys / max(h - 1, 1)) * 2.0 - 1.0

    # --- 3. 点光源：点状扩散光斑 -----------------------------------------------
    p_intensity = params.get("point_intensity", 0.0)
    if p_intensity > 0.0:
        cx = params.get("point_x", 0.0)      # -1 ~ 1，0 为画面中心
        cy = params.get("point_y", 0.0)
        radius = max(params.get("point_radius", 0.5), 0.01)
        power = params.get("point_power", 2.0)

        d = np.sqrt((nx - cx) ** 2 + (ny - cy) ** 2) / radius
        spot = _falloff(d, power) * p_intensity

        if is_gray:
            out += spot * 255.0
        else:
            out += spot[..., None] * 255.0

    # --- 4. 线性反光：条状高光 -------------------------------------------------
    l_intensity = params.get("line_intensity", 0.0)
    if l_intensity > 0.0:
        angle = np.deg2rad(params.get("line_angle", 45.0))
        offset = params.get("line_offset", 0.0)     # -1 ~ 1，光带位置
        width = max(params.get("line_width", 0.15), 0.01)
        power = params.get("line_power", 2.0)

        # 角度是「光带本身的走向」：0° = 水平光带，90° = 垂直光带。
        # 光带沿 (cos a, sin a) 延伸，所以衰减方向是它的法线 (-sin a, cos a)。
        d = np.abs((-nx * np.sin(angle) + ny * np.cos(angle)) - offset) / width
        band = _falloff(d, power) * l_intensity

        if is_gray:
            out += band * 255.0
        else:
            out += band[..., None] * 255.0

    return np.clip(out, 0.0, 255.0)


def _falloff(d: np.ndarray, power: float) -> np.ndarray:
    """
    统一衰减曲线，d 是归一化距离（0 = 光斑中心，1 = 标称边界）。

    power 就是界面上的「衰减曲线」滑块（0.3 ~ 8），含义实测如下
    （下表是「亮区 >75% 峰值」所覆盖的半径，可见贡献更直观）：

        power = 0.4   亮区半径 0.13   只有中心一小点亮，四周全靠渐隐 —— 弥散、阴天
        power = 1.0   亮区半径 0.33   较柔和
        power = 2.0   亮区半径 0.58   默认
        power = 4.0   亮区半径 0.76   中心一大片都很亮，边缘较快跌落
        power = 8.0   亮区半径 0.87   接近平顶，边缘陡然切断 —— 强聚光、水面反光

    即：滑块往右拖 = 中心亮区占比变大、边缘越陡。

    公式 1/(1+d^power) 的性质：
      - d=0 恒为 1（峰值），d=1 恒为 0.5，与 power 无关
        → 调曲线只改变亮区形状，不改变整体亮度观感
      - 单调连续可导，没有硬边
    """
    power = max(power, 0.05)
    return 1.0 / (1.0 + np.power(d, power))


# ============================================================================
# 参数定义表（单一数据源）
# ----------------------------------------------------------------------------
# 界面上的滑块、数值框、单个重置按钮、批量重置、导出/导入，全部由这张表生成。
# 加一个新参数只需要在这里加一行。
#
#   key      参数名（与 render() 的 params 键一致）
#   label    界面标签
#   lo/hi    滑块范围
#   step     数值框步进
#   default  默认值（也是单个重置按钮的目标值）
#   fmt      数值显示格式
#   unit     单位后缀
# ============================================================================

PARAM_SPECS = [
    # ---- 基础（常驻显示）----
    dict(key="exposure", label="曝光（整体亮度）", lo=0.1, hi=4.0, step=0.05,
         default=1.0, fmt="{:.2f}", unit="x", group="basic"),
    dict(key="contrast", label="对比度", lo=0.2, hi=3.0, step=0.05,
         default=1.0, fmt="{:.2f}", unit="x", group="basic"),
    dict(key="point_intensity", label="点光源强度（阳光光斑）", lo=0.0, hi=2.0,
         step=0.05, default=0.0, fmt="{:.2f}", unit="", group="basic"),
    dict(key="line_intensity", label="线性反光强度（条状高光）", lo=0.0, hi=2.0,
         step=0.05, default=0.0, fmt="{:.2f}", unit="", group="basic"),

    # ---- 点光源（高级）----
    dict(key="point_x", label="水平位置", lo=-1.0, hi=1.0, step=0.01,
         default=0.0, fmt="{:.2f}", unit="", group="point"),
    dict(key="point_y", label="垂直位置", lo=-1.0, hi=1.0, step=0.01,
         default=-0.3, fmt="{:.2f}", unit="", group="point"),
    dict(key="point_radius", label="扩散半径", lo=0.05, hi=1.5, step=0.01,
         default=0.45, fmt="{:.2f}", unit="", group="point"),
    dict(key="point_power", label="衰减曲线（右=亮区大·边缘陡）", lo=0.3, hi=8.0,
         step=0.1, default=2.0, fmt="{:.2f}", unit="", group="point"),

    # ---- 线性反光（高级）----
    dict(key="line_angle", label="角度", lo=0.0, hi=180.0, step=1.0,
         default=45.0, fmt="{:.1f}", unit="°", group="line"),
    dict(key="line_offset", label="偏移", lo=-1.0, hi=1.0, step=0.01,
         default=0.0, fmt="{:.2f}", unit="", group="line"),
    dict(key="line_width", label="条宽", lo=0.02, hi=0.8, step=0.01,
         default=0.15, fmt="{:.2f}", unit="", group="line"),
    dict(key="line_power", label="衰减曲线（右=亮带宽·边缘陡）", lo=0.3, hi=8.0,
         step=0.1, default=2.0, fmt="{:.2f}", unit="", group="line"),
]

SPEC_BY_KEY = {s["key"]: s for s in PARAM_SPECS}


# ============================================================================
# 主界面
# ============================================================================

# 配色（深色主题，让灰度图当主体）
BG_APP = "#2b2b2b"          # 窗口底色
BG_PANEL = "#333333"        # 右侧面板
BG_CANVAS = "#1a1a1a"       # 画布（比图更暗，形成衬托）
FG_TEXT = "#e0e0e0"
FG_DIM = "#9a9a9a"
ACCENT = "#4a9eff"


class ImageEditor:

    ZOOM_LEVELS = (1, 2, 3, 4, 6, 8)    # 放大倍率档位（188x120 这类小图整数倍最好看）

    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("图像曝光与光照编辑器 —— 华南理工大学智能车队 22 届视觉组")
        root.geometry("1240x780")
        root.minsize(1000, 640)
        root.configure(bg=BG_APP)

        self.src: np.ndarray | None = None       # 原图，float32，(H,W) 或 (H,W,3)
        self.src_path: str = ""
        self.siblings: list = []                 # 同目录下的图片列表
        self.sib_idx: int = -1
        self.zoom: int = 3                       # 当前放大倍率
        self.view_dx = 0                         # 拖动平移偏移
        self.view_dy = 0
        self._pan_from = None
        self.tk_img = None                       # 防止被 GC
        self._job = None                         # 防抖定时器
        self._upload_job = None                  # 自动上传防抖定时器
        self.show_advanced = tk.BooleanVar(value=True)
        self.show_footer = tk.BooleanVar(value=True)

        self._init_style()
        self._build_ui()
        self._bind_keys()

    # ------------------------------------------------------------------
    # 样式
    # ------------------------------------------------------------------
    def _init_style(self):
        st = ttk.Style()
        try:
            st.theme_use("clam")             # clam 才允许自定义配色
        except tk.TclError:
            pass
        st.configure(".", background=BG_PANEL, foreground=FG_TEXT,
                     fieldbackground="#3d3d3d", bordercolor="#4a4a4a")
        st.configure("TFrame", background=BG_PANEL)
        st.configure("TLabel", background=BG_PANEL, foreground=FG_TEXT)
        st.configure("Dim.TLabel", background=BG_PANEL, foreground=FG_DIM)
        st.configure("Head.TLabel", background=BG_PANEL, foreground=FG_TEXT,
                     font=("Microsoft YaHei UI", 11, "bold"))
        st.configure("Sub.TLabel", background=BG_PANEL, foreground=ACCENT,
                     font=("Microsoft YaHei UI", 9, "bold"))
        st.configure("Brand.TLabel", background=BG_PANEL, foreground="#c8a45a",
                     font=("Microsoft YaHei UI", 9))
        st.configure("TButton", background="#454545", foreground=FG_TEXT, borderwidth=0,
                     focuscolor="none", padding=(10, 5))
        st.map("TButton", background=[("active", "#565656"), ("pressed", "#3a3a3a")])
        st.configure("Accent.TButton", background=ACCENT, foreground="#ffffff")
        st.map("Accent.TButton", background=[("active", "#5aabff"), ("pressed", "#3a8ee0")])
        st.configure("TCheckbutton", background=BG_PANEL, foreground=FG_TEXT, focuscolor="none")
        st.map("TCheckbutton", background=[("active", BG_PANEL)])
        st.configure("TRadiobutton", background=BG_PANEL, foreground=FG_TEXT, focuscolor="none")
        st.map("TRadiobutton", background=[("active", BG_PANEL)])
        st.configure("TEntry", fieldbackground="#3d3d3d", foreground=FG_TEXT,
                     insertcolor=FG_TEXT, borderwidth=0, padding=4)
        st.configure("Num.TEntry", fieldbackground="#2a2a2a", foreground="#7fc4ff",
                     insertcolor=FG_TEXT, borderwidth=0, padding=2)
        st.configure("TScale", background=BG_PANEL, troughcolor="#252525")
        st.configure("TSeparator", background="#4a4a4a")
        st.configure("TLabelframe", background=BG_PANEL, foreground=FG_TEXT, borderwidth=0)
        st.configure("TLabelframe.Label", background=BG_PANEL, foreground=FG_DIM)

    # ------------------------------------------------------------------
    # 界面搭建
    # ------------------------------------------------------------------
    def _build_ui(self):
        # ===== 顶部工具条 =====
        top = ttk.Frame(self.root, padding=(10, 8))
        top.pack(side="top", fill="x")

        ttk.Button(top, text="打开图像", style="Accent.TButton",
                   command=self.open_image).pack(side="left")
        ttk.Separator(top, orient="vertical").pack(side="left", fill="y", padx=10)

        self.btn_prev = ttk.Button(top, text="◀ 上一张", command=lambda: self.step(-1),
                                   state="disabled")
        self.btn_prev.pack(side="left")
        self.btn_next = ttk.Button(top, text="下一张 ▶", command=lambda: self.step(1),
                                   state="disabled")
        self.btn_next.pack(side="left", padx=6)

        self.lbl_counter = ttk.Label(top, text="", style="Dim.TLabel")
        self.lbl_counter.pack(side="left", padx=8)

        # 右侧：缩放
        ttk.Button(top, text="－", width=3,
                   command=lambda: self.zoom_by(-1)).pack(side="right")
        self.lbl_zoom = ttk.Label(top, text="300%", width=6, anchor="center")
        self.lbl_zoom.pack(side="right")
        ttk.Button(top, text="＋", width=3,
                   command=lambda: self.zoom_by(1)).pack(side="right")
        ttk.Label(top, text="缩放", style="Dim.TLabel").pack(side="right", padx=(0, 6))
        ttk.Button(top, text="适应", command=self.zoom_fit).pack(side="right", padx=(0, 10))

        # 中部：车队标识（占位，不与两侧控件抢空间）
        ttk.Label(top, text="华南理工大学智能车队 · 22 届招新 · 竞速组视觉",
                  style="Brand.TLabel").pack(side="right", padx=(0, 18))

        # ===== 底部（状态 + 保存），先pack保证不被挤掉 =====
        self._build_footer()

        # ===== 主体：左画布 + 右参数 =====
        body = ttk.Frame(self.root)
        body.pack(side="top", fill="both", expand=True, padx=10, pady=(0, 8))

        right = ttk.Frame(body, width=340)
        right.pack(side="right", fill="y", padx=(10, 0))
        right.pack_propagate(False)

        # 左：画布容器（深色衬托，居中显示图像）
        self.canvas_wrap = tk.Frame(body, bg=BG_CANVAS)
        self.canvas_wrap.pack(side="left", fill="both", expand=True)

        self.canvas = tk.Canvas(self.canvas_wrap, bg=BG_CANVAS, highlightthickness=0)
        self.canvas.pack(fill="both", expand=True)
        self.canvas.bind("<Configure>", lambda e: self.schedule_render())
        # 滚轮缩放
        self.canvas.bind("<MouseWheel>",
                         lambda e: self.zoom_by(1 if e.delta > 0 else -1))
        self.canvas.bind("<Button-4>", lambda e: self.zoom_by(1))
        self.canvas.bind("<Button-5>", lambda e: self.zoom_by(-1))
        # 左键拖动平移
        self.canvas.bind("<ButtonPress-1>", self._pan_start)
        self.canvas.bind("<B1-Motion>", self._pan_move)
        self.canvas.bind("<ButtonRelease-1>", self._pan_end)
        self.canvas.bind("<Double-Button-1>", lambda e: self.reset_view())

        self._build_panel(right)

        # 右侧面板随处滚轮都能滚动高级区（不用非去拖滚动条）
        self._bind_wheel_scroll(right)

    # ------------------------------------------------------------------
    def _build_footer(self):
        footer = tk.Frame(self.root, bg=BG_PANEL)
        footer.pack(side="bottom", fill="x", padx=10, pady=(0, 10))

        # 保存区
        save = ttk.LabelFrame(footer, text=" 保存 ", padding=(10, 6))
        save.pack(fill="x")

        # 第一行：文件夹
        r0 = ttk.Frame(save)
        r0.pack(fill="x", pady=2)
        ttk.Label(r0, text="文件夹", width=6, style="Dim.TLabel").pack(side="left")
        self.v_dir = tk.StringVar(value="")
        ttk.Entry(r0, textvariable=self.v_dir).pack(side="left", fill="x", expand=True, padx=6)
        ttk.Button(r0, text="浏览", command=self.choose_dir).pack(side="left")

        # 第二行：文件名 + 选项 + 保存
        r1 = ttk.Frame(save)
        r1.pack(fill="x", pady=2)
        ttk.Label(r1, text="文件名", width=6, style="Dim.TLabel").pack(side="left")
        self.v_name = tk.StringVar(value="out")
        ttk.Entry(r1, textvariable=self.v_name, width=18).pack(side="left", padx=6)
        self.v_auto = tk.BooleanVar(value=True)
        self.v_over = tk.BooleanVar(value=False)
        ttk.Checkbutton(r1, text="自增", variable=self.v_auto).pack(side="left", padx=(4, 0))
        ttk.Checkbutton(r1, text="覆盖", variable=self.v_over).pack(side="left", padx=(4, 0))

        ttk.Label(r1, text="格式", style="Dim.TLabel").pack(side="left", padx=(14, 4))
        self.v_fmt = tk.StringVar(value=".bmp")
        for t in (".bmp", ".png", ".jpg"):
            ttk.Radiobutton(r1, text=t, value=t, variable=self.v_fmt).pack(side="left")

        ttk.Button(r1, text="保存", style="Accent.TButton",
                   command=self.save_image).pack(side="right")

        # 状态行
        r2 = ttk.Frame(save)
        r2.pack(fill="x", pady=(4, 0))
        self.status = ttk.Label(r2, text="就绪", style="Dim.TLabel")
        self.status.pack(side="left")
        self.info = ttk.Label(r2, text="尚未载入图像", style="Dim.TLabel")
        self.info.pack(side="right")

    # ------------------------------------------------------------------
    def _build_panel(self, parent):
        """
        右侧参数面板。

        布局：标题栏 + 三个【并列】的可折叠区块
            ▼ 基础光照   （曝光 / 对比度 / 两个光斑强度）
            ▶ 高级参数   （光斑位置、形状、衰减曲线）
            ▶ 仿真联动   （目标路径、自动上传）

        三者是平行关系，不再把联动塞进高级参数里面。
        整个区块列表可滚动，窗口压扁时不会丢控件。
        """
        head = ttk.Frame(parent)
        head.pack(fill="x", pady=(0, 4))
        ttk.Label(head, text="图像调整", style="Head.TLabel").pack(side="left")
        ttk.Button(head, text="全部重置", width=9,
                   command=self.reset_params).pack(side="right")

        self.vars: dict = {}

        # ---- 可滚动容器：装三个并列区块 ----
        holder = ttk.Frame(parent)
        holder.pack(fill="both", expand=True)

        self.adv_canvas = tk.Canvas(holder, bg=BG_PANEL,
                                    highlightthickness=0, bd=0)
        scroll = ttk.Scrollbar(holder, orient="vertical",
                               command=self.adv_canvas.yview)
        self.adv_canvas.configure(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        self.adv_canvas.pack(side="left", fill="both", expand=True)

        self.adv = ttk.Frame(self.adv_canvas)
        self._adv_win = self.adv_canvas.create_window((0, 0), window=self.adv, anchor="nw")
        self.adv.bind("<Configure>", lambda e: self.adv_canvas.configure(
            scrollregion=self.adv_canvas.bbox("all")))
        self.adv_canvas.bind("<Configure>", lambda e: self.adv_canvas.itemconfig(
            self._adv_win, width=e.width))
        self.adv_canvas.bind("<MouseWheel>", lambda e: self.adv_canvas.yview_scroll(
            -1 if e.delta > 0 else 1, "units"))
        self.adv.bind("<MouseWheel>", lambda e: self.adv_canvas.yview_scroll(
            -1 if e.delta > 0 else 1, "units"))

        # ================= 区块 1：基础光照 =================
        self.sec_basic_expanded = tk.BooleanVar(value=True)
        btn1, body1 = self._collapsible(self.adv, "基础光照", self.sec_basic_expanded)
        self.btn_basic = btn1
        for spec in PARAM_SPECS:
            if spec["group"] == "basic":
                self._param_row(body1, spec, top=6)

        # ================= 区块 2：高级参数 =================
        self.show_advanced = tk.BooleanVar(value=False)
        btn2, body2 = self._collapsible(self.adv, "高级参数", self.show_advanced,
                                        top=10)
        self.btn_adv = btn2
        self.adv_holder = body2          # 兼容旧引用名

        self._group_title(body2, "点光斑 位置与形状", top=2)
        for spec in PARAM_SPECS:
            if spec["group"] == "point":
                self._param_row(body2, spec)

        self._group_title(body2, "线性光斑 方向与形状", top=12)
        for spec in PARAM_SPECS:
            if spec["group"] == "line":
                self._param_row(body2, spec)

        # 导出 / 导入参数（属于参数区，跟着高级参数走）
        ttk.Separator(body2).pack(fill="x", pady=(12, 8))
        row = ttk.Frame(body2)
        row.pack(fill="x")
        ttk.Button(row, text="导出参数", command=self.export_params).pack(
            side="left", fill="x", expand=True)
        ttk.Button(row, text="导入参数", command=self.import_params).pack(
            side="left", fill="x", expand=True, padx=(6, 0))

        # 参数变化 → 重绘
        for var in self.vars.values():
            var.trace_add("write", lambda *_: self.schedule_render())

        # ================= 区块 3：仿真联动（与上面并列）=================
        self.sec_link_expanded = tk.BooleanVar(value=False)
        btn3, body3 = self._collapsible(self.adv, "仿真联动", self.sec_link_expanded,
                                        top=10)
        self.btn_link = btn3
        self.link_body = body3
        self._build_link_content(body3)

        # 底部留白：滚到底时最后一个控件不贴边
        ttk.Frame(self.adv, height=10).pack(fill="x")

    # ------------------------------------------------------------------
    # 可折叠区块
    # ------------------------------------------------------------------
    def _collapsible(self, parent, title, state_var, top=0):
        """
        生成一个可折叠区块：标题（带 ▼/▶ 箭头，点击切换）+ 内容容器。
        返回 (标题控件, 内容容器)。内容默认按 state_var 的初值决定展开与否。

        用 Label 手绘而不是 ttk.Checkbutton：clam 主题会给 Checkbutton
        强制画一个系统勾选框，观感与折叠箭头不符。
        """
        exp = bool(state_var.get())
        btn = tk.Label(parent, text=("▼  " if exp else "▶  ") + title,
                       bg=BG_PANEL, fg=(ACCENT if exp else FG_TEXT),
                       font=("Microsoft YaHei UI", 9, "bold"),
                       cursor="hand2", anchor="w")
        btn.pack(fill="x", pady=(top, 2))

        body = ttk.Frame(parent)

        def toggle(_e=None):
            new = not state_var.get()
            state_var.set(new)
            if new:
                body.pack(fill="x")
                btn.config(text="▼  " + title, fg=ACCENT)
            else:
                body.pack_forget()
                btn.config(text="▶  " + title, fg=FG_TEXT)

        btn.bind("<Button-1>", toggle)
        if exp:
            body.pack(fill="x")
        return btn, body

    def _toggle_advanced(self):
        """按一下「高级参数」标题（供快捷键/代码调用）"""
        self.btn_adv.event_generate("<Button-1>")

    def _toggle_link(self):
        """按一下「仿真联动」标题"""
        self.btn_link.event_generate("<Button-1>")

    def _group_title(self, parent, text, top=6):
        ttk.Label(parent, text=text, style="Sub.TLabel").pack(anchor="w", pady=(top, 2))

    def _param_row(self, parent, spec, top=8):
        """
        一个参数 = 标题行（标签 + 数值输入框 + 重置按钮）+ 滑块。
        数值框可以直接键入精确值，回车或失焦生效。
        """
        key = spec["key"]
        var = tk.DoubleVar(value=spec["default"])
        self.vars[key] = var

        box = ttk.Frame(parent)
        box.pack(fill="x", pady=(top, 0))

        # ---- 标题行：标签 | 可编辑数值 | 重置 ----
        head = ttk.Frame(box)
        head.pack(fill="x")
        ttk.Label(head, text=spec["label"]).pack(side="left")

        # 单个重置按钮（↺）：恢复该参数的默认值
        rst = tk.Label(head, text="↺", bg=BG_PANEL, fg=FG_DIM,
                       font=("Segoe UI Symbol", 11), cursor="hand2", width=2)
        rst.pack(side="right", padx=(6, 0))
        rst.bind("<Button-1>", lambda e, k=key: self.reset_one(k))
        rst.bind("<Enter>", lambda e, w=rst: w.config(fg=ACCENT))
        rst.bind("<Leave>", lambda e, w=rst: w.config(fg=FG_DIM))

        # 可编辑数值框
        ent = ttk.Entry(head, width=7, justify="right", style="Num.TEntry")
        ent.pack(side="right", padx=(4, 0))
        if spec["unit"]:
            ttk.Label(head, text=spec["unit"], style="Dim.TLabel").pack(
                side="right", padx=(0, 3))

        def sync_entry(*_):
            """滑块动了 → 刷新输入框（正在编辑时不打断）"""
            if self.root.focus_get() is ent:
                return
            ent.delete(0, "end")
            ent.insert(0, spec["fmt"].format(var.get()))

        def commit(_=None):
            """输入框提交 → 更新变量，并夹到合法范围"""
            try:
                v = float(ent.get())
            except ValueError:
                sync_entry()
                return
            v = max(spec["lo"], min(spec["hi"], v))
            var.set(v)
            sync_entry()

        ent.bind("<Return>", commit)
        ent.bind("<FocusOut>", commit)
        var.trace_add("write", sync_entry)
        sync_entry()

        # ---- 滑块 ----
        ttk.Scale(box, variable=var, from_=spec["lo"], to=spec["hi"],
                  orient="horizontal").pack(fill="x", pady=(1, 0))

    def _build_link_content(self, body):
        """
        仿真联动区的内容（标题由 _collapsible 生成，与「高级参数」并列）。

        工作方式（纯文件监听，两端不直接通信）：
          编辑器把处理结果写到「一张正在被仿真环境显示的图片」上，
          仿真环境监测该文件的时间戳变化，一变就重新读图并重跑算法。
          所以这里要配的就是「写到哪里」以及「什么时候自动写」。

        仿真环境侧对应实现：env\\hot_reload.c
        """
        self.link_body = body

        # --- 目标文件夹 ---
        r0 = ttk.Frame(body)
        r0.pack(fill="x", pady=(4, 2))
        ttk.Label(r0, text="目标文件夹", style="Dim.TLabel").pack(anchor="w")
        r0b = ttk.Frame(body)
        r0b.pack(fill="x")
        self.v_link_dir = tk.StringVar(value="")
        ttk.Entry(r0b, textvariable=self.v_link_dir).pack(
            side="left", fill="x", expand=True)
        ttk.Button(r0b, text="浏览", width=6,
                   command=self.choose_link_dir).pack(side="left", padx=(4, 0))

        # --- 目标文件名 ---
        r1 = ttk.Frame(body)
        r1.pack(fill="x", pady=(6, 0))
        ttk.Label(r1, text="目标文件名", style="Dim.TLabel").pack(side="left")
        self.v_link_name = tk.StringVar(value="1")
        ttk.Entry(r1, textvariable=self.v_link_name, width=10).pack(
            side="left", padx=(4, 0))
        ttk.Label(r1, text=".bmp", style="Dim.TLabel").pack(side="left")

        # --- 自动上传 ---
        r2 = ttk.Frame(body)
        r2.pack(fill="x", pady=(8, 0))
        self.v_auto_upload = tk.BooleanVar(value=False)
        ttk.Checkbutton(r2, text="停止操作后自动上传",
                        variable=self.v_auto_upload,
                        command=self._on_auto_upload_toggle).pack(anchor="w")

        r3 = ttk.Frame(body)
        r3.pack(fill="x", pady=(2, 0))
        ttk.Label(r3, text="    等待", style="Dim.TLabel").pack(side="left")
        self.v_wait_ms = tk.IntVar(value=800)
        self.spin_wait = ttk.Spinbox(r3, from_=100, to=10000, increment=100,
                                     textvariable=self.v_wait_ms, width=7)
        self.spin_wait.pack(side="left", padx=4)
        ttk.Label(r3, text="毫秒无操作后上传", style="Dim.TLabel").pack(side="left")

        # --- 手动上传 ---
        ttk.Button(body, text="立即上传到仿真环境",
                   command=self.upload_now).pack(fill="x", pady=(8, 2))

        self.lbl_link_state = ttk.Label(body, text="未启用", style="Dim.TLabel")
        self.lbl_link_state.pack(anchor="w")

        # 未启用时禁用细节控件，避免误以为在生效
        self._set_link_widgets_enabled(False)

    def _set_link_widgets_enabled(self, on):
        state = "normal" if on else "disabled"
        try:
            self.spin_wait.config(state=state)
        except Exception:
            pass

    def _on_auto_upload_toggle(self):
        on = self.v_auto_upload.get()
        self._set_link_widgets_enabled(on)
        if on and not self.v_link_dir.get().strip():
            # 没设目标目录时给个合理默认：仿真环境的图集目录
            guess = self._guess_sim_dir()
            if guess:
                self.v_link_dir.set(guess)
        self._update_link_state()

    def _guess_sim_dir(self):
        """猜测仿真环境的图片目录。相对编辑器所在位置找 22th_visual_simu\\pic\\1。"""
        here = os.path.dirname(os.path.abspath(__file__))
        for up in range(4):
            base = os.path.abspath(os.path.join(here, *([".."] * up)))
            cand = os.path.join(base, "22th_visual_simu", "pic", "1")
            if os.path.isdir(cand):
                return cand
        return ""

    def _update_link_state(self):
        if not hasattr(self, "lbl_link_state"):
            return
        if self.v_auto_upload.get():
            ms = self.v_wait_ms.get()
            self.lbl_link_state.config(
                text=f"已启用：停手 {ms} ms 后自动上传", foreground="#7fc4ff")
        else:
            self.lbl_link_state.config(text="未启用自动上传", foreground=FG_DIM)

    def choose_link_dir(self):
        d = filedialog.askdirectory(title="选择仿真环境的图集目录")
        if d:
            self.v_link_dir.set(d)

    def link_target(self):
        """返回联动目标文件的完整路径，未配置时返回 None"""
        d = self.v_link_dir.get().strip()
        n = self.v_link_name.get().strip()
        if not d or not n:
            return None
        if not os.path.isdir(d):
            return None
        if not n.lower().endswith(self.IMG_EXT):
            n += ".bmp"
        return os.path.join(d, n)

    def upload_now(self):
        """把当前处理结果写到联动目标文件（仿真环境会监测到并自动重载）"""
        target = self.link_target()
        if target is None:
            messagebox.showwarning("联动未配置",
                                   "请先填写有效的目标文件夹和文件名。")
            return False
        if self.src is None:
            self.status.config(text="没有可上传的图像")
            return False
        try:
            result = render(self.src, self.params())
            Image.fromarray(result.astype(np.uint8)).save(target)
        except Exception as e:
            messagebox.showerror("上传失败", str(e))
            return False

        self.status.config(text=f"已上传 → {os.path.basename(target)}")
        if hasattr(self, "lbl_link_state"):
            self.lbl_link_state.config(
                text=f"上次上传 {time.strftime('%H:%M:%S')}", foreground="#7fc4ff")
        return True

    def _schedule_auto_upload(self):
        """参数变动后重新计时；停手达到设定时长才真正上传"""
        if not self.v_auto_upload.get():
            return
        if self._upload_job:
            self.root.after_cancel(self._upload_job)
        try:
            ms = max(100, int(self.v_wait_ms.get()))
        except (tk.TclError, ValueError):
            ms = 800
        self._upload_job = self.root.after(ms, self._do_auto_upload)

    def _do_auto_upload(self):
        self._upload_job = None
        self.upload_now()

    def reset_one(self, key):
        """单个参数恢复默认值"""
        spec = SPEC_BY_KEY[key]
        self.vars[key].set(spec["default"])
        self.status.config(text=f"{spec['label']} 已重置")

    # ------------------------------------------------------------------
    # 键盘与缩放
    # ------------------------------------------------------------------
    def _bind_wheel_scroll(self, widget):
        """
        给右侧面板整棵子树绑滚轮：鼠标停在哪都能滚动高级参数区。
        输入框/滑块除外，免得滚轮误改数值。
        """
        def on_wheel(e):
            if not self.show_advanced.get():
                return
            self.adv_canvas.yview_scroll(-1 if e.delta > 0 else 1, "units")

        def walk(w):
            for child in w.winfo_children():
                if isinstance(child, (ttk.Entry, ttk.Scale, ttk.Combobox)):
                    continue
                child.bind("<MouseWheel>", on_wheel, add="+")
                walk(child)
        walk(widget)

    # ------------------------------------------------------------------
    # 拖动平移
    # ------------------------------------------------------------------
    def _pan_start(self, e):
        self._pan_from = (e.x, e.y, self.view_dx, self.view_dy)
        self.canvas.config(cursor="fleur")

    def _pan_move(self, e):
        if not getattr(self, "_pan_from", None):
            return
        x0, y0, dx0, dy0 = self._pan_from
        self.view_dx = dx0 + (e.x - x0)
        self.view_dy = dy0 + (e.y - y0)
        self.render_now()

    def _pan_end(self, e):
        self._pan_from = None
        self.canvas.config(cursor="")

    def reset_view(self):
        """双击画布：位移归零并重新适应窗口"""
        self.view_dx = self.view_dy = 0
        self.zoom_fit()

    def _bind_keys(self):
        self.root.bind("<Left>", lambda e: self.step(-1))
        self.root.bind("<Right>", lambda e: self.step(1))
        self.root.bind("<Control-o>", lambda e: self.open_image())
        self.root.bind("<Control-s>", lambda e: self.save_image())
        self.root.bind("<plus>", lambda e: self.zoom_by(1))
        self.root.bind("<minus>", lambda e: self.zoom_by(-1))
        self.root.bind("<Escape>", lambda e: self.root.destroy())
        # 焦点在输入框里时不要抢方向键
        self.root.bind_class("TEntry", "<Left>", lambda e: None)

    def zoom_by(self, d):
        try:
            i = self.ZOOM_LEVELS.index(self.zoom)
        except ValueError:
            i = 0
        i = max(0, min(len(self.ZOOM_LEVELS) - 1, i + d))
        if self.ZOOM_LEVELS[i] != self.zoom:
            self.zoom = self.ZOOM_LEVELS[i]
            self.render_now()

    def zoom_fit(self):
        """按画布大小选一个刚好放得下的整数倍率"""
        if self.src is None:
            return
        h, w = self.src.shape[:2]
        cw = max(self.canvas.winfo_width() - 24, 64)
        ch = max(self.canvas.winfo_height() - 24, 64)
        best = 1
        for z in self.ZOOM_LEVELS:
            if w * z <= cw and h * z <= ch:
                best = z
        # 画布比图还小时退回 1 倍，避免 0 倍画不出来
        if w > cw or h > ch:
            best = 1
        self.zoom = best
        self.render_now()

    # ------------------------------------------------------------------
    # 参数
    # ------------------------------------------------------------------
    def params(self) -> dict:
        return {k: v.get() for k, v in self.vars.items()}

    def set_params(self, p: dict):
        """导入参数用：只覆盖认识的键，并夹到合法范围"""
        for k, val in p.items():
            var = self.vars.get(k)
            spec = SPEC_BY_KEY.get(k)
            if var is None or spec is None:
                continue
            try:
                v = float(val)
            except (TypeError, ValueError):
                continue
            var.set(max(spec["lo"], min(spec["hi"], v)))

    def reset_params(self):
        """全部参数回到默认值"""
        for key, var in self.vars.items():
            var.set(SPEC_BY_KEY[key]["default"])
        self.status.config(text="全部参数已重置")

    def export_params(self):
        import json
        path = filedialog.asksaveasfilename(
            title="导出参数", defaultextension=".json",
            filetypes=[("参数文件", "*.json")])
        if not path:
            return
        try:
            with open(path, "w", encoding="utf-8") as f:
                json.dump(self.params(), f, ensure_ascii=False, indent=2)
            self.status.config(text=f"参数已导出 → {os.path.basename(path)}")
        except OSError as e:
            messagebox.showerror("导出失败", str(e))

    def import_params(self):
        import json
        path = filedialog.askopenfilename(title="导入参数",
                                          filetypes=[("参数文件", "*.json")])
        if not path:
            return
        try:
            with open(path, encoding="utf-8") as f:
                self.set_params(json.load(f))
            self.status.config(text=f"参数已导入 ← {os.path.basename(path)}")
        except (OSError, ValueError) as e:
            messagebox.showerror("导入失败", str(e))

    # ------------------------------------------------------------------
    # 读图、翻页与预览
    # ------------------------------------------------------------------
    IMG_EXT = (".bmp", ".png", ".jpg", ".jpeg")

    def open_image(self):
        path = filedialog.askopenfilename(
            title="选择图像",
            filetypes=[("图像", "*.bmp *.png *.jpg *.jpeg"), ("全部", "*.*")])
        if not path:
            return
        self.load(path)

    def scan_siblings(self, path: str):
        """扫描同目录下的图片，按文件名自然排序，供上一张/下一张翻页"""
        folder = os.path.dirname(path)
        try:
            names = [f for f in os.listdir(folder)
                     if f.lower().endswith(self.IMG_EXT)]
        except OSError:
            names = []

        def sort_key(n):
            # 让 2.bmp 排在 10.bmp 前面（纯字典序会反过来）
            return [int(t) if t.isdigit() else t.lower()
                    for t in re.split(r"(\d+)", n)]

        names.sort(key=sort_key)
        folder_n = os.path.normcase(os.path.abspath(folder))
        self.siblings = [os.path.join(folder, n) for n in names]

        # 用规范化后的绝对路径比较，避免分隔符/大小写差异导致匹配失败
        # （匹配失败会让 sib_idx 变成 -1，表现为「下一张永远跳到第一张」）
        target = os.path.normcase(os.path.abspath(path))
        self.sib_idx = -1
        for i, p in enumerate(self.siblings):
            if os.path.normcase(os.path.abspath(p)) == target:
                self.sib_idx = i
                break

        # 兜底：按文件名再比一次（应对短路径/软链接等）
        if self.sib_idx < 0:
            base = os.path.basename(path).lower()
            for i, p in enumerate(self.siblings):
                if os.path.basename(p).lower() == base:
                    self.sib_idx = i
                    break

        # 仍未找到时，退化为「排序后插入位置」，保证翻页从合理位置开始
        if self.sib_idx < 0:
            self.sib_idx = 0
        _ = folder_n

    def next_index(self, d: int) -> int:
        """返回翻页后的下标；越界时原样返回当前下标。纯计算，便于测试。"""
        if not self.siblings:
            return self.sib_idx
        i = self.sib_idx + d
        return i if 0 <= i < len(self.siblings) else self.sib_idx

    def step(self, d):
        i = self.next_index(d)
        if i != self.sib_idx:
            self.load(self.siblings[i])

    def load(self, path: str):
        try:
            im = Image.open(path)
            im.load()
            if im.mode not in ("L", "RGB"):
                im = im.convert("RGB")
            self.src = np.asarray(im, dtype=np.float32)
            self.cur_mode = im.mode
        except Exception as e:
            messagebox.showerror("读取失败", f"{os.path.basename(path)}\n{e}")
            return

        self.src_path = path
        self.view_dx = self.view_dy = 0        # 换图时复位平移
        self.scan_siblings(path)

        h, w = self.src.shape[:2]
        # 大图默认按画布适应，小图给个好看的整数倍
        if max(w, h) <= 256:
            self.zoom = 3
            self.zoom_fit()
        else:
            self.zoom = 1
            self.zoom_fit()

        self.info.config(text=f"{os.path.basename(path)}   {w}×{h}   {self.cur_mode}")
        if len(self.siblings) > 1:
            self.lbl_counter.config(
                text=f"第 {self.sib_idx + 1} / {len(self.siblings)} 张")
            self.btn_prev.config(state="normal" if self.sib_idx > 0 else "disabled")
            self.btn_next.config(
                state="normal" if self.sib_idx < len(self.siblings) - 1 else "disabled")
        else:
            self.lbl_counter.config(text="")
            self.btn_prev.config(state="disabled")
            self.btn_next.config(state="disabled")

        if not self.v_dir.get():
            self.v_dir.set(os.path.dirname(path))
        if self.v_name.get() == "out":
            self.v_name.set(re.sub(r"\.(bmp|png|jpe?g)$", "",
                                   os.path.basename(path), flags=re.I))

        self.render_now()

    def schedule_render(self):
        """拖动滑块时防抖，避免每个像素级变动都重算"""
        if self._job:
            self.root.after_cancel(self._job)
        self._job = self.root.after(30, self.render_now)
        # 每次参数/图片变动都重置自动上传计时：真正停手之后才上传
        self._schedule_auto_upload()

    def render_now(self):
        self._job = None
        if not hasattr(self, "canvas"):
            return
        cw = self.canvas.winfo_width()
        ch = self.canvas.winfo_height()

        self.canvas.delete("all")
        self.lbl_zoom.config(text=f"{self.zoom * 100:.0f}%")

        if self.src is None or cw < 10 or ch < 10:
            if self.src is None:
                self.canvas.create_text(
                    cw // 2 or 300, ch // 2 or 200,
                    text="点击左上角「打开图像」载入一张图\n支持 BMP / PNG / JPG",
                    fill="#5a5a5a", font=("Microsoft YaHei UI", 12), justify="center")
            return

        h, w = self.src.shape[:2]
        tw, th = max(1, w * self.zoom), max(1, h * self.zoom)

        # 全分辨率计算（188x120 只有 2 万像素，无需降采样预览）
        result = render(self.src, self.params())
        pil = Image.fromarray(result.astype(np.uint8))
        if (tw, th) != (w, h):
            pil = pil.resize((tw, th), Image.NEAREST if self.zoom >= 2 else Image.BILINEAR)

        cx, cy = cw // 2 + self.view_dx, ch // 2 + self.view_dy

        # 图像超出画布时，只截取可见的一块交给 Tk，避免画巨大图卡顿
        disp = pil
        if tw > cw or th > ch:
            # 可见区域在「缩放后图像坐标系」里的范围
            x0 = int(max(0, (tw / 2) - cx))
            y0 = int(max(0, (th / 2) - cy))
            x1 = int(min(tw, (tw / 2) - cx + cw))
            y1 = int(min(th, (th / 2) - cy + ch))
            if x1 <= x0 or y1 <= y0:
                return          # 完全拖出可视范围
            disp = pil.crop((x0, y0, x1, y1))
            left = cx - tw // 2 + x0
            top = cy - th // 2 + y0
        else:
            # 未裁剪：整图按中心摆放
            dw, dh = disp.size
            left = cx - dw // 2
            top = cy - dh // 2

        self.tk_img = ImageTk.PhotoImage(disp)
        dw, dh = disp.size
        # 细边框标出图像实际范围，暗部与背景色接近时也能看清
        self.canvas.create_rectangle(left - 1, top - 1, left + dw, top + dh,
                                     outline="#4a9eff", width=1)
        self.canvas.create_image(left, top, image=self.tk_img, anchor="nw")

    # ------------------------------------------------------------------
    # 保存
    # ------------------------------------------------------------------
    def choose_dir(self):
        d = filedialog.askdirectory(title="选择保存文件夹")
        if d:
            self.v_dir.set(d)

    def next_name(self, folder: str, base: str, ext: str) -> str:
        """
        自增命名。规则：
          - 名字「不是数字结尾」→ 在末尾追加序号： out → out1, out2, out3
          - 名字「是数字结尾」  → 递增这个数字：   1 → 2, 10 → 11；img007 → img008
            （保留原有的补零位数，方便按文件名排序）

        用扫描磁盘而不是内存计数器，避免程序重启后重名。
        """
        m = re.search(r"(\d+)$", base)
        if m:
            # 数字结尾：递增尾号，保留补零宽度。
            # 只统计「同前缀 + 同位数」的文件，否则 base=2 会被 11.bmp 顶到 12.bmp
            digits = m.group(1)
            prefix = base[:m.start()]
            width = len(digits)
            existing = self._scan_numbers(folder, prefix, ext, rf"\d{{{width}}}$")
            # 同族里都没有时，至少从基准数字本身起步
            n = max([int(digits)] + existing) + 1
            tail = str(n).zfill(width)
            return f"{prefix}{tail}{ext}"

        # 非数字结尾：追加序号
        existing = self._scan_numbers(folder, base, ext, r"\d+$")
        return f"{base}{max([0] + existing) + 1}{ext}"

    def _scan_numbers(self, folder, prefix, ext, num_pat):
        """扫出 folder 里形如 <prefix><数字><ext> 的所有数字"""
        pat = re.compile(rf"^{re.escape(prefix)}({num_pat[:-1]}){re.escape(ext)}$", re.I)
        out = []
        try:
            for fn in os.listdir(folder):
                m = pat.match(fn)
                if m:
                    out.append(int(m.group(1)))
        except OSError:
            pass
        return out

    def save_image(self):
        if self.src is None:
            messagebox.showwarning("提示", "请先打开一张图像")
            return

        folder = self.v_dir.get().strip()
        base = self.v_name.get().strip()
        if not folder or not os.path.isdir(folder):
            messagebox.showwarning("提示", "保存文件夹无效")
            return
        if not base:
            messagebox.showwarning("提示", "请填写文件名")
            return

        ext = self.v_fmt.get()

        # 按磁盘现状决定文件名（保存始终走全分辨率）
        name = self.next_name(folder, base, ext) if self.v_auto.get() else base + ext
        target = os.path.join(folder, name)

        if os.path.exists(target) and not self.v_over.get():
            ok = messagebox.askyesno("文件已存在", f"{name} 已存在，要覆盖吗？")
            if not ok:
                self.status.config(text="已取消保存")
                return

        try:
            result = render(self.src, self.params())
            pil = Image.fromarray(result.astype(np.uint8))
            if ext in (".jpg", ".jpeg"):
                pil = pil.convert("RGB")
            pil.save(target)
        except Exception as e:
            messagebox.showerror("保存失败", str(e))
            return

        self.status.config(text=f"已保存 → {name}")
        if self.v_auto.get():
            self.v_name.set(base)   # 名字框保持基准名，编号由磁盘递增


def main():
    root = tk.Tk()
    ImageEditor(root)
    root.mainloop()


if __name__ == "__main__":
    main()
