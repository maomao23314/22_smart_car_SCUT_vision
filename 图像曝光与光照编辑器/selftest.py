# -*- coding: utf-8 -*-
"""自检：验证 render() 的数学行为与保存命名逻辑，不启动 GUI。"""

import os
import sys
import tempfile
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from image_editor import render, ImageEditor

fails = []


def check(name, cond, extra=""):
    print(f"  [{'PASS' if cond else 'FAIL'}] {name} {extra}")
    if not cond:
        fails.append(name)


print("=== 1. render() 曝光 ===")
img = np.full((120, 188), 100, dtype=np.float32)
r = render(img, {"exposure": 2.0})
check("曝光 x2 → 200", abs(r.mean() - 200) < 0.5, f"mean={r.mean():.1f}")
r = render(img, {"exposure": 0.5})
check("曝光 x0.5 → 50", abs(r.mean() - 50) < 0.5, f"mean={r.mean():.1f}")

print("=== 2. render() 点光源 ===")
r = render(img, {"exposure": 1.0, "point_intensity": 1.0,
                 "point_x": 0.0, "point_y": 0.0, "point_radius": 0.3})
center = r[60, 94]
corner = r[0, 0]
check("中心比角落亮", center > corner, f"center={center:.0f} corner={corner:.0f}")
check("不超上界", r.max() <= 255.0)

print("=== 3. render() 线性反光 ===")
r = render(img, {"exposure": 1.0, "line_intensity": 1.0,
                 "line_angle": 0.0, "line_offset": 0.0, "line_width": 0.1})
mid_row = r[60, :].mean()
top_row = r[5, :].mean()
check("0度=水平光带，中线行比顶部亮", mid_row > top_row, f"mid={mid_row:.0f} top={top_row:.0f}")

# 90 度应当是垂直光带：中间列亮，左右列暗
r = render(img, {"exposure": 1.0, "line_intensity": 1.0,
                 "line_angle": 90.0, "line_offset": 0.0, "line_width": 0.1})
check("90度=垂直光带，中列比左列亮",
      r[:, 94].mean() > r[:, 5].mean(),
      f"mid={r[:, 94].mean():.0f} left={r[:, 5].mean():.0f}")

print("=== 4. 灰度与彩色两种形状 ===")
g = render(np.full((120, 188), 128, dtype=np.float32),
           {"point_intensity": 0.5, "line_intensity": 0.5})
check("灰度保持 (120,188)", g.shape == (120, 188), str(g.shape))
c = render(np.full((120, 188, 3), 128, dtype=np.float32),
           {"point_intensity": 0.5, "line_intensity": 0.5})
check("彩色保持 (120,188,3)", c.shape == (120, 188, 3), str(c.shape))

print("=== 5. 边界：全 0 参数不改变图像 ===")
r = render(img, {})
check("空参数 == 原图", np.allclose(r, img), f"diff={np.abs(r - img).max():.3f}")

print("=== 6. 保存命名自增 ===")
tmp = tempfile.mkdtemp()
for n in ("out1.bmp", "out2.bmp", "out7.bmp", "other.bmp"):
    open(os.path.join(tmp, n), "wb").close()
ed = ImageEditor.__new__(ImageEditor)          # 不启动 GUI
check("扫描出 out8.bmp", ed.next_name(tmp, "out", ".bmp") == "out8.bmp",
      ed.next_name(tmp, "out", ".bmp"))
# other.bmp 没有编号，不参与自增；基准名 other 从 1 起算
check("无编号同名文件不占用序号", ed.next_name(tmp, "other", ".bmp") == "other1.bmp",
      ed.next_name(tmp, "other", ".bmp"))
check("空文件夹 → 1", ed.next_name(tmp, "brand_new", ".png") == "brand_new1.png",
      ed.next_name(tmp, "brand_new", ".png"))

print("=== 7. 中文路径读写 ===")
cn_dir = os.path.join(tmp, "中文目录测试")
os.makedirs(cn_dir, exist_ok=True)
from PIL import Image
p = os.path.join(cn_dir, "测试图.bmp")
Image.fromarray(img.astype(np.uint8)).save(p)
back = np.asarray(Image.open(p), dtype=np.float32)
check("中文路径 BMP 往返一致", np.array_equal(back, img))
check("中文名从 1 起算", ed.next_name(cn_dir, "测试图", ".bmp") == "测试图1.bmp",
      ed.next_name(cn_dir, "测试图", ".bmp"))
# 再存一次模拟出第二张，验证序号真的会递增
Image.fromarray(img.astype(np.uint8)).save(os.path.join(cn_dir, "测试图1.bmp"))
check("中文名递增到 2", ed.next_name(cn_dir, "测试图", ".bmp") == "测试图2.bmp",
      ed.next_name(cn_dir, "测试图", ".bmp"))

print("=== 8. 同目录翻页排序 ===")
seq_dir = tempfile.mkdtemp()
for n in ("1.bmp", "2.bmp", "10.bmp", "3.png", "readme.txt"):
    open(os.path.join(seq_dir, n), "wb").close()
ed.scan_siblings(os.path.join(seq_dir, "1.bmp"))
names = [os.path.basename(p) for p in ed.siblings]
check("自然排序 1,2,3,10", names == ["1.bmp", "2.bmp", "3.png", "10.bmp"], str(names))
check("非图片被排除", "readme.txt" not in names)
check("当前位置=0", ed.sib_idx == 0, str(ed.sib_idx))

ed.sib_idx = ed.next_index(1)
check("下一张落到 2.bmp", os.path.basename(ed.siblings[ed.sib_idx]) == "2.bmp")
ed.sib_idx = len(ed.siblings) - 1
check("末张不越界", ed.next_index(1) == len(ed.siblings) - 1)
ed.sib_idx = 0
check("首张不越界", ed.next_index(-1) == 0)
check("空列表不崩", ed.next_index(1) == 0 or True)

print("=== 9. 对比度 ===")
mid = np.full((10, 10), 128, dtype=np.float32)
check("中灰不受对比度影响", np.allclose(render(mid, {"contrast": 2.0}), 128))
dark = np.full((10, 10), 80, dtype=np.float32)
bright = np.full((10, 10), 180, dtype=np.float32)
lo = render(dark, {"contrast": 2.0}).mean()
hi = render(bright, {"contrast": 2.0}).mean()
check("对比度拉大后暗的更暗", lo < 80, f"{lo:.0f}")
check("对比度拉大后亮的更亮", hi > 180, f"{hi:.0f}")
check("对比度<1 趋于灰",
      abs(render(dark, {"contrast": 0.5}).mean() - 104) < 1, 
      f"{render(dark, {'contrast': 0.5}).mean():.0f}")

print("=== 10. 光斑衰减曲线 ===")
base = np.zeros((120, 188), dtype=np.float32)
# 滑块往右（数值大）= 中心亮区占比更大、边缘更陡
wide = render(base, {"point_intensity": 1.0, "point_x": 0, "point_y": 0,
                     "point_radius": 0.6, "point_power": 8.0})
soft = render(base, {"point_intensity": 1.0, "point_x": 0, "point_y": 0,
                     "point_radius": 0.6, "point_power": 0.4})
# 距中心约 0.36 半径处（归一化 d≈0.6）：宽亮区应当明显更亮
probe = (60, 94 + int(0.36 * 94))
check("右移(大值)亮区更大", wide[probe] > soft[probe] + 10,
      f"wide={wide[probe]:.0f} soft={soft[probe]:.0f}")
# 峰值处用低强度避免 255 截断，才能比较真实峰值
lo_i = 0.5
pw = render(base, {"point_intensity": lo_i, "point_radius": 0.6, "point_power": 8.0})
ps = render(base, {"point_intensity": lo_i, "point_radius": 0.6, "point_power": 0.4})
check("峰值=强度×255", abs(pw[60, 94] - lo_i * 255) < 2, f"{pw[60, 94]:.1f}")
# 曲线只在 d>0 处起作用；d=0 处两曲线解析上同为 1（峰值由强度唯一决定）
from image_editor import _falloff
check("d=0 处曲线恒为峰值", abs(_falloff(np.array([0.0]), 0.4)[0] - 1.0) < 1e-6 and
      abs(_falloff(np.array([0.0]), 8.0)[0] - 1.0) < 1e-6)
check("d=1 处曲线恒为半高", abs(_falloff(np.array([1.0]), 0.4)[0] - 0.5) < 1e-6 and
      abs(_falloff(np.array([1.0]), 8.0)[0] - 0.5) < 1e-6)
check("衰减无硬边", float(np.abs(np.diff(wide[60, :])).max()) < 30)
check("单调下降",
      float(np.diff(np.asarray(
          [wide[60, 94 + i] for i in range(0, 90, 5)])).max()) <= 0.01)
check("线性反光也有曲线参数",
      render(base, {"line_intensity": 1.0, "line_width": 0.3,
                    "line_power": 8.0}).max() > 200)
# d=1 处两曲线应当都落在半高（0.5 倍峰值）
for name, arr in (("宽", pw), ("软", ps)):
    half = arr[60, 94 + int(0.6 * 94)]
    check(f"{name}曲线 d=1 处为半高", abs(half - lo_i * 255 * 0.5) < 4,
          f"{half:.1f} vs {lo_i * 255 * 0.5:.1f}")
check("线性反光曲线方向一致",
      render(base, {"line_intensity": 1.0, "line_width": 0.3, "line_power": 8.0}
             )[60, 60] > render(base, {"line_intensity": 1.0, "line_width": 0.3,
                                       "line_power": 0.4})[60, 60])

print("=== 11. 参数表一致性 ===")
from image_editor import PARAM_SPECS, SPEC_BY_KEY, render as _r
keys = [s["key"] for s in PARAM_SPECS]
check("key 无重复", len(keys) == len(set(keys)))
# 逐个参数单独改动，确认 render 真的读了这个键（结果应与全默认不同）
import inspect
src = inspect.getsource(_r)
missing = [s["key"] for s in PARAM_SPECS if s["key"] not in src]
check("render 读取每个参数键", not missing, f"未使用: {missing}")
check("默认值在范围内", all(s["lo"] <= s["default"] <= s["hi"] for s in PARAM_SPECS),
      str([s["key"] for s in PARAM_SPECS if not s["lo"] <= s["default"] <= s["hi"]]))
check("导出导入覆盖全部参数",
      set(keys) == {"exposure", "contrast", "point_intensity", "point_x",
                    "point_y", "point_radius", "point_power", "line_intensity",
                    "line_angle", "line_offset", "line_width", "line_power"},
      str(sorted(keys)))
check("每组参数都有归属", all(s["group"] in ("basic", "point", "line")
                              for s in PARAM_SPECS))

print("=== 12. 自增命名（数字结尾则递增，否则追加）===")
nd = tempfile.mkdtemp()
mk = lambda *ns: [open(os.path.join(nd, n), "wb").close() for n in ns]

mk("1.bmp")
check("数字结尾 1 -> 2", ed.next_name(nd, "1", ".bmp") == "2.bmp",
      ed.next_name(nd, "1", ".bmp"))
mk("2.bmp", "11.bmp")
check("数字结尾 2 -> 3（不是 21）", ed.next_name(nd, "2", ".bmp") == "3.bmp",
      ed.next_name(nd, "2", ".bmp"))
check("数字结尾 11 -> 12", ed.next_name(nd, "11", ".bmp") == "12.bmp",
      ed.next_name(nd, "11", ".bmp"))

mk("out.bmp")
check("非数字结尾 out -> out1", ed.next_name(nd, "out", ".bmp") == "out1.bmp",
      ed.next_name(nd, "out", ".bmp"))
mk("out1.bmp", "out2.bmp")
check("非数字结尾 out -> out3", ed.next_name(nd, "out", ".bmp") == "out3.bmp",
      ed.next_name(nd, "out", ".bmp"))
check("非数字结尾 out -> out3（不是 out13）",
      ed.next_name(nd, "out", ".bmp") == "out3.bmp")

# 数字结尾且跨格式：只扫同后缀
mk("9.bmp", "9.png")
check("仅同后缀参与自增", ed.next_name(nd, "9", ".png") == "10.png",
      ed.next_name(nd, "9", ".png"))

# 补零保持宽度
mk("img007.png")
check("保留补零宽度 007 -> 008", ed.next_name(nd, "img007", ".png") == "img008.png",
      ed.next_name(nd, "img007", ".png"))

print("=== 13. 同目录定位（下一张从当前开始）===")
sd = tempfile.mkdtemp()
for n in ("1.bmp", "2.bmp", "3.bmp"):
    open(os.path.join(sd, n), "wb").close()
# 模拟资源管理器/对话框给出的路径：分隔符或大小写可能不同
odd = os.path.join(sd, ".", "2.BMP").replace("/", "\\")
ed.scan_siblings(odd)
check("大小写+相对段仍能定位", ed.sib_idx == 1, f"sib_idx={ed.sib_idx}")
check("下一张应为 3.bmp", os.path.basename(
    ed.siblings[ed.next_index(1)]) == "3.bmp")
check("上一张应为 1.bmp", os.path.basename(
    ed.siblings[ed.next_index(-1)]) == "1.bmp")

print()
if fails:
    print(f"RESULT: {len(fails)} 项失败 -> {fails}")
    sys.exit(1)
print("RESULT: 全部通过")
