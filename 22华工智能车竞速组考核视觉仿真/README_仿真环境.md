# 22 届视觉组仿真环境

> 华南理工大学智能车队 22 届招新 · 竞速组视觉方向

**一句话**：在电脑上模拟 MT9V03X 摄像头，你写纯 C 算法处理灰度图，环境把原图和处理结果并排显示出来。

**三件事记住就够了：**

1. **跑起来** → 双击 `一键配置.bat`，然后按 `F11`（Dev-C++）或 `F5`（VS）
2. **写代码** → 只改 `code\camera.c` 里的 `image_process()`，新增 `.c/.h` 不用改工程
3. **看数据** → 输入是 `mt9v03x_image[120][188]`，用 `SCUT_DrawPoint/DrawLine` 画图，用 `SCUT_Log` 打数据

算法是纯 C，**整个 `code\` 目录原样拷到 CH32V307 就能编译**（只依赖 `code\scut_port.h`，
移植说明见第十节）。要显示的图像、尺寸、ROI 都在 `config.h` 里配，算法里不写死。

---

## 一、5 分钟跑起来

### 第一步：配置（只需做一次）

双击工程根目录下的 **`一键配置.bat`**。

它会自动帮你：
1. 扫描 `code\` 目录，生成需要编译的文件清单
2. 检查有没有可用的编译器（Dev-C++ / MinGW，或任意版本的 Visual Studio）
3. 检查 EasyX 图形库装没装
4. 问你要不要立刻编译运行一次（输入 `1` 或 `2` 然后回车）

### 第二步：日常开发

| 方式 | 怎么做 |
|---|---|
| **Dev-C++** | 打开 `视觉仿真环境.dev`，按 `F11`<br>⚠ 新增/删除/改名 `code\` 下的文件后，先双击 `tools\rebuild.bat` |
| **Visual Studio** | 打开 `VS\visual_simu.sln`，选 `Debug\|x64`，按 `F5`（2017 及以上均可） |
| **命令行** | 双击 `tools\build_dev.bat`（用 MinGW）<br>双击 `tools\build_vs.bat`（用 VS，自动找任意版本） |

三种方式随便挑一种，效果完全一样。**不需要改任何项目属性。**

> **关于 `tools\rebuild.bat`（只有 Dev-C++ 需要）**
>
> `code\` 下的文件是靠 `env\code_filelist.h` 自动收集的，那个文件由脚本生成。
> **Visual Studio 每次 F5 都会自动重新生成**，命令行脚本每次也会，所以这两条路你什么都不用管。
>
> **但 Dev-C++ 没有"编译前执行命令"的功能**，所以按 F11 时它不会重新扫描 `code\`。
> 症状是：你新建了 `myalgo.c`，按 F11，却报一堆
> `undefined reference to ...` —— 看起来像代码写错了，其实是新文件没被收录。
>
> 这时双击一次 **`tools\rebuild.bat`**，它会重新扫描 `code\` 并清掉过期的中间文件，
> 然后你照常按 F11 即可。**只有"增删改名文件"这一种情况需要它**，
> 平时改代码内容直接 F11 就行。

### 第三步：改代码

打开 `code\camera.c`，把里面的示例算法换成你自己的。**就这样，没了。**

---

## 二、项目结构

```
22华工智能车竞速组考核视觉仿真\
│
├─ main.cpp                   程序入口 + 两个「接入函数」（你要改）
├─ config.h                   ★ 所有配置项都在这里（你要改）
├─ README_仿真环境.md          本文件
├─ 更新日志.md                 版本改动记录
├─ 一键配置.bat                第一次使用先跑这个
├─ 视觉仿真环境.dev            Dev-C++ 工程文件
│
├─ code\                      ★★ 你的代码就放这里 ★★
│   ├─ camera.c               算法示例（大津法二值化 + 左右扫线）
│   ├─ camera.h               算法对外接口与全局变量声明
│   ├─ perspective.c/.h       逆透视（俯视图）可选示例，默认不启用
│   └─ scut_port.h            ★ 单片机移植兼容层（整个 code\ 只依赖它）
│
├─ env\                       环境本体（一般不用动）
│   ├─ disp_env.hpp           环境对外接口声明
│   ├─ disp_env.cpp           环境实现：窗口/图片/按键/缩放/计时/显示/日志
│   ├─ scut_display.h         ★ 给你调用的接口（画图、打日志、查询）
│   ├─ scut_common_typedef.h  公共类型与颜色定义（SCUT_COLOR_xxx）
│   ├─ simu_env.h             输入图像 + 输出图像数组的声明
│   ├─ simu_env.c             输入/输出图像的定义 + 自动收集 code\ 的挂载点
│   ├─ hot_reload.h/.c        热重载：监测当前图片是否被编辑器改写（见第七节）
│   ├─ code_filelist.h        自动生成：code\ 下所有 .c 的清单（不要手动改）
│   └─ code_headerlist.h      自动生成：code\ 下所有 .h 的清单（不要手动改）
│
├─ VS\                        Visual Studio 工程
│   ├─ visual_simu.sln        双击打开这个
│   ├─ visual_simu.vcxproj    工程设置（Debug/Release 都已配好）
│   └─ visual_simu.vcxproj.user  调试工作目录设置（别删）
│
├─ tools\                     工具脚本（一般不用手动跑）
│   ├─ gen_filelist.ps1       扫描 code\ 生成文件清单
│   ├─ sync_vs_items.ps1      同步 VS 工程的文件列表
│   ├─ sync_dev_units.ps1     同步 Dev-C++ 工程的单元列表
│   ├─ find_msbuild.bat       自动定位任意版本的 MSBuild
│   ├─ build_dev.bat          命令行编译运行（MinGW）
│   ├─ build_vs.bat           命令行编译运行（任意版本 VS）
│   ├─ rebuild.bat            ★ 重扫 code\ + 清理中间文件（Dev-C++ 新增文件后跑一次）
│   └─ verify_mixed.ps1       验证 C/C++ 混合编译是否正常
│
└─ pic\                       测试图集（1~6 组，共 109 张图）
```

---

## 三、你要写的代码在哪

> ### ⚠️ 先记住一件事：`camera.c` 只是「Hello World」，不是答案
>
> `code\camera.c` 里那套**大津法 + 跳变扫线**只是为了演示接口怎么用：
> 输入图从哪拿、结果写哪、怎么画线、怎么打日志。
>
> 它和真正能上赛道的视觉代码**差得很远**：
>
> - 没有处理十字、环岛、坡道、断路、虚线等特殊元素
> - 没有边线滤波 / 补线 / 丢线保护，跳变点一多中线就乱跳
> - 扫线只取第一个跳变点，遇到噪点、反光会直接跑偏
> - 没有透视变换，也没按行加权，远端一点点误差会被放大
> - 固定按整幅图处理，没有分区、没有动态阈值、没有置信度判断
>
> **把它当成「Hello World」，不是「参考答案」。**
> 你需要自己设计算法，或者参考往届开源方案（逐飞、各路校赛开源库等）。
> 我们评的是你的思路和实现，不是你有没有照抄这份示例。

环境只认 **两个函数**，写在 `code\` 目录下：

```c
void image_init(void)       // 开机初始化，调一次。不需要就留空函数体
int  image_process(void)    // 每帧的图像处理主函数，环境自动调用并计时
```

另外 `main.cpp` 里还有 **两个「接入函数」**（在 `main` 函数上面）：

```c
void draw_image_info(void)  // ① 画点画线，把中间结果显示在图上
void show_image_data(void)  // ② 用 SCUT_Log 输出数据
```

> 其实在 `code\` 里也能画、也能打日志，只要 `#include "scut_display.h"`。
> 示例 `camera.c` 就是这么做的。放哪里随你方便。

### 想加新文件？

直接在 `code\` 下新建 `.c` / `.h` 就行，**不用改任何工程设置**。
**也支持子文件夹**，比如 `code\algo\line.c`，一样会被自动收录。

| 你用哪种方式 | 新建文件后要做什么 |
|---|---|
| Visual Studio (F5) | 什么都不用做，每次生成都会自动重扫 |
| 命令行脚本 | 什么都不用做，脚本会先重扫再编译 |
| **Dev-C++ (F11)** | **先双击一次 `tools\rebuild.bat`**，然后照常 F11 |

（文件名以 `_` 开头的会被跳过，可以用来临时屏蔽某个文件。）

> 为什么 Dev-C++ 要多一步：它没有"编译前执行命令"的功能，F11 不会重扫 `code\`。
> VS 工程里有 `DSH_GenFileList` 目标每次自动重跑，命令行脚本里也显式调用了扫描脚本，
> 只有 Dev-C++ 这条路需要你手动跑一次。

---

## 三点五、环境如何找到你的代码（想改名/删示例时必读）

这一节解释环境和你代码之间的"接口约定"，以及**哪些能自由改、哪些有限制**。

### 你的代码怎么被编译进来

环境**不单独编译** `code\*.c`，而是把它们全部 `#include` 进 `env\simu_env.c`
这一个编译单元。清单文件是 `env\code_filelist.h`，由 `tools\gen_filelist.ps1` 生成：

```c
/* env\code_filelist.h —— 自动生成，不要手改 */
#include "../code/camera.c"
#include "../code/perspective.c"
     ↑ 你新增的文件会自动出现在这里（跑过扫描脚本之后）
```

**所以你可以**：随便增删改 `code\` 下的 `.c` / `.h`，建子文件夹，都行。

### 入口函数名可以改

`config.h` 里：

```c
#define SCUT_USER_INIT_FUNC      image_init      // 改成你的初始化函数名
#define SCUT_USER_PROCESS_FUNC   image_process   // 改成你的每帧处理函数名
```

环境通过这两个宏调用，所以你的函数叫 `my_vision_init` / `my_vision_run` 也没问题，
**签名保持一致即可**（初始化 `void f(void)`，处理 `int f(void)`）。

### 头文件名也可以改（v1.4.0 起）

环境需要包含你的头文件才能拿到你声明的全局变量（比如给绘图用）。

**早期版本**在这里硬编码了 `#include "camera.h"`，导致你把示例改名或删掉后
环境本体就编不过 —— 与"算法文件一个字不用动"的承诺矛盾。**现已修好**：

- 环境改为包含 `env\code_headerlist.h`（同样自动生成），里面列出 `code\` 下所有 `.h`
- 每条都用 `__has_include` 包着，所以清单过期（删了文件没重扫）也不会编不过

**为什么不能用"自动扫描目录"这种更聪明的办法**（技术限制，写在这里免得后人再试）：

| 想法 | 结果 |
|---|---|
| `#if __has_include("*.h")` | ❌ **不行**。`__has_include` 只接受一个确定的文件名，不支持通配符（GCC 实测直接报 `Invalid argument`） |
| `#include "某个目录"` | ❌ **不行**。C 标准不允许包含目录（GCC 报 `No such file or directory`） |
| 让环境去猜函数/变量名 | ❌ **做不到**。预处理器只能判断"**头文件**在不在"，无法判断"某个**变量**有没有被声明" |

**结论**：C 语言层面没有"自动包含一个目录下所有头文件"的能力。
本项目采用的办法是**让构建脚本去发现**（生成清单），这也是它能做到"改名不用动环境"的原因。
代价是：**新增/改名头文件后要重跑一次扫描**（VS 和命令行自动，Dev-C++ 需手动跑
`tools\rebuild.bat`）。

### 示例的三条调试线怎么关

`main.cpp` 的 `draw_image_info()` 默认会把示例算法导出的
`left_line` / `right_line` / `mid_line` 画成绿/蓝/红三条线。
**这三个变量是示例专有的** —— 你换成自己的算法后它们就不存在了。

`config.h` 里：

```c
#define SCUT_DRAW_SAMPLE_LINES   (1)      // 改成 0 就不再画这三条线
```

改成 `0` 之后 `main.cpp` 不会引用那些变量，你的算法只写自己的绘图代码即可。
（若你给自己的数组起了别的名字，也可以改 `SAMPLE_LINE_LEFT/RIGHT/MID` 三个宏。）

> 为什么不干脆自动判断：同上 —— 预处理器无法判断变量是否存在，
> 所以给一个**显式开关**，行为确定，不会猜错。

### 小结：改名自由度一览

| 你想改的 | 能改吗 | 怎么改 |
|---|---|---|
| 算法 `.c` / `.h` 文件名 | ✅ 能 | 直接改。改完跑一次 `tools\rebuild.bat`（VS/命令行自动） |
| 建子文件夹 | ✅ 能 | 直接建，支持 `code\algo\x.c` |
| 入口函数名 | ✅ 能 | 改 `config.h` 的 `SCUT_USER_INIT_FUNC` / `SCUT_USER_PROCESS_FUNC` |
| 删掉示例 | ✅ 能 | 删了之后把 `SCUT_DRAW_SAMPLE_LINES` 改成 0 |
| 调试线数组名 | ✅ 能 | 改 `SAMPLE_LINE_LEFT/RIGHT/MID` |
| 函数签名 | ❌ 不能 | 必须 `void f(void)` / `int f(void)` |
| 入口函数所在的编译方式 | ❌ 不能 | `code\*.c` 一律按 **C** 编译，不能写 C++ 语法 |

---

## 四、接口速查

### 输入：唯一的图像来源

```c
mt9v03x_image[y][x]     // y = 行 0~119    x = 列 0~187    值 = 灰度 0~255
```

> ⚠️ 这个数组是「摄像头」在写的。**处理前请先 `memcpy` 一份副本**，
> 不要边读边改，否则会读到半新半旧的数据。`camera.c` 里有标准写法可以照抄。

### 输出图像：在 `config.h` 里指定（不在 `code\` 里）

**输出图像不由你的算法定义**，而是由 `config.h` 配置。算法只管用 `SCUT_OutImageSet()` 写结果：

```c
/* ---- config.h 里这样配 ---- */
#define SCUT_OUT_IMAGE_ARRAY      output_image   // 要显示的数组名（可以换成你的）
#define SCUT_OUT_IMAGE_W          188            // 显示区域宽度
#define SCUT_OUT_IMAGE_H          120            // 显示区域高度
#define SCUT_OUT_IMAGE_X          0              // 显示起始列
#define SCUT_OUT_IMAGE_Y          0              // 显示起始行

/* ---- 你的算法里这样写 ---- */
SCUT_OutImageSet(x, y, 255);        // 往输出图像写一个像素，自动处理 ROI 偏移
uint8 v = SCUT_OutImageGet(x, y);   // 读回来
```

坐标是**相对显示区域**的（0 ~ `SCUT_OUT_IMAGE_W-1`），越界自动忽略。

想只看一块 ROI：把 `W/H` 调小、`X/Y` 改成起点，窗口会自动跟着调整，
**算法代码一个字都不用改**。

> 为什么输出不放 `code\` 里？因为 `code\` 要能直接拷到 CH32 车载工程。
> 把「显示到哪个数组、显示多大」这类仿真环境的事写进算法，
> 上车就要改代码。现在这些全在 `config.h`，算法保持干净。

### 绘图

```c
SCUT_DrawPoint(x, y, SCUT_COLOR_RED);                 // 画点
SCUT_DrawLine(x0, y0, x1, y1, SCUT_COLOR_GREEN);      // 画线（斜线也行）
SCUT_DrawRect(x0, y0, x1, y1, SCUT_COLOR_BLUE);       // 画矩形框
```

坐标就是图像坐标，**越界会自动忽略**，不用自己判断。

颜色用 `SCUT_COLOR_xxx`，想自己配色用 `SCUT_RGB(r, g, b)`：

```c
SCUT_DrawPoint(10, 10, SCUT_RGB(0x66, 0xCC, 0xFF));   // 自定义颜色
```

> ⚠️ **不要手写十六进制颜色**。`SCUT_COLOR_xxx` 的内部排列是 Windows 的
> `COLORREF`（`0x00BBGGRR`，蓝色在低位），不是直觉上的 `0xRRGGBB`。
> 直接写 `0xFF0000` 得到的是**蓝色**而不是红色。
> 用 `SCUT_RGB(255, 0, 0)` 或 `SCUT_COLOR_RED` 就永远是对的。

### 日志输出

```c
SCUT_Log(SCUT_LOG_LEVEL_INFO, "大津法阈值: %d", thre);
    // 自动排在「处理时间」下面，一行一行往下走

SCUT_LogAt(420, 560, SCUT_LOG_LEVEL_WARN, "长度: %d", length);
    // 自己指定窗口坐标（想在右边排一列时用）
```

**日志行数超过窗口剩余高度时，窗口会自动变高**，不会把文字挤没。
按 `R` 做复杂度测试时，第 2 次以后的日志会被自动屏蔽，不会刷屏。

### 查询

```c
SCUT_GetImageWidth();    SCUT_GetImageHeight();     // 输入图像尺寸
SCUT_GetOutputWidth();   SCUT_GetOutputHeight();    // 输出图像尺寸
```

### 耗时与刷新（细节见第六节）

```c
SCUT_EnableDrawTiming(1);       // 打开绘图计时
SCUT_GetDrawCost();             // 上次 draw_image_info() 的花费（微秒）
SCUT_GetFrameCost();            // 整帧耗时（微秒）

SCUT_Flush(500);                // 立刻刷新到屏幕并停 500ms，用来看绘制顺序
```

---

## 五、按键

| 按键 | 功能 |
|---|---|
| `+` / `-` | 下一张 / 上一张图片（到一组头尾会自动跨组） |
| `Ctrl` + `+` / `-` | 放大 / 缩小图像 |
| `R` | 图像处理重复 1000 次做复杂度测试 |
| `H` | 开关热重载（与图像编辑器的联动，见第七节） |
| `ESC` | 退出 |
| 鼠标左键 | 在窗口**右上角**显示该点的灰度值（浅蓝色） |

> 「处理时间」显示的是**平均每次**的结果，所以按 `R` 前后显示的数字可以直接对比。

下方状态栏会显示 `[热重载 开 已刷新 N 次]`，可以据此确认联动是否在工作。

---

## 六、三种耗时，别搞混（重要）

窗口上那行字可能长这样：

```
处理时间: 41.35 M cycles  (12948.4 us @3194MHz)   (绘图 3.0 us   整帧 155306.9 us)
```

这是**三个独立的量**：

| 数字 | 含义 | 怎么来的 |
|---|---|---|
| **处理时间** | 你的 `image_process()` 耗时 | 环境自动计时并显示 ★ 这是你要关心的 |
| **绘图** | `draw_image_info()` 耗时 | `SCUT_GetDrawCost()`，需先 `SCUT_EnableDrawTiming(1)` |
| **整帧** | 处理 + 绘图 + 显示刷新 | `SCUT_GetFrameCost()` |

### 「处理时间」为什么给两个数：M cycles 和 us

| 看哪个 | 含义 | 什么时候用 |
|---|---|---|
| **M cycles**（百万 CPU 周期） | 算法消耗了多少个 CPU 时钟周期 | ★ **对比性能、和同学比、和上次比** |
| **us**（微秒） | 墙上时间 | 判断**实时性够不够**（比如要跑满 100fps 就得 <10ms） |

**为什么以周期数为主**：微秒会随电脑快慢变。同一份算法，
在 3.2GHz 的机器上写 12.7ms，在 4.5GHz 的机器上可能只要 9ms ——
数字不一样，但那不是算法变好了，只是电脑更快。

**周期数则是算法自身的固有属性**：它数的是 CPU 时钟周期，
与主频解耦（现代 CPU 的 invariant TSC 特性）。
换电脑、开不开睿频、后台有没有别的程序，这个数字都基本不变。

实测同一段固定工作量：

| | 读数 |
|---|---|
| 微秒（墙上时间） | 3863.9 → 4681.0 us，**波动 21%** |
| 周期数比值 | 3192.5，**恒定** |

所以**要比较算法改进效果，请看 M cycles**。

> **单位说明**：以百万（M）为单位，`41.35 M cycles` = 4135 万周期。
> 数字不足 1 M 时会直接显示原始周期数（如 `12345 cycles`），
> 避免出现 `0.00 M` 这种看起来像没干活的显示。

> **注意**：周期数也不是物理常数。不同代、不同架构的 CPU
> （缓存大小、乱序执行、分支预测能力不同）跑同一份 C 代码，
> 周期数会**接近但不完全相同**。
> 它比微秒客观得多，但**最公平的比较还是同一台机器上前后对比**。

括号里的 `@3194MHz` 是本机自动标定出的 CPU 时钟频率，
用于把周期数换算成微秒。这个值是自动测的，不用手填。

**如果你的电脑不支持稳定周期计时**（很老的 CPU、部分虚拟机），
程序会自动回退，只显示微秒，不会给出不可信的数字。

### 「处理时间」是按不优化编译的

`code\` 下的算法固定按 `-O0`（不优化）编译，三个工具链口径一致。

为什么：如果算法被优化，编译器会做循环展开、把中间变量消除掉，
测出的周期数就**不再反映你写的代码的真实开销** ——
同一份写法在不同优化等级下能差好几倍。
而且单片机上（CH32 车载工程）通常也是不优化或低优化的，
仿真端按 `-O0` 才和上车后的表现接近。

> 想测优化后的性能？那是另一件事，请另开 Release 配置专门测，
> 不要拿这里的数字去和 `-O0` 的数字比。

### 为什么绘图不算进「处理时间」

环境只在 `image_process()` 前后取时间戳，`draw_image_info()` 在它之后执行，**不在计时区间内**。

这是**刻意的**：车载程序上没有"画点给屏幕看"这件事。如果把绘图算进去，你辛辛苦苦优化算法省下的时间，会被画一条线就吃掉的开销淹没，性能数据就失真了。

**但绘图仍然是真实的帧开销。** 实测：

| | 耗时 |
|---|---|
| 一个简单算法 | 约 2 µs |
| `SCUT_DrawPoint` × 1000 | **约 11 µs** |

画得太多会明显拖低帧率。所以给了你单独的数字去看它。

### 想观察绘制顺序？

`draw_image_info()` 里画的东西默认**攒完一帧才一次性显示**，所以最终看到的是所有元素叠在一起，分不出先后。

在两次绘制之间调用 **`SCUT_Flush(毫秒)`**，它会立刻把当前内容推上屏幕并停顿，于是能一眼看出哪些是先画的：

```c
for (y = 0; y < SCUT_IMAGE_H; y++) SCUT_DrawPoint(left_line[y], y, SCUT_COLOR_GREEN);
SCUT_Flush(500);          // ← 绿线先显示，停 0.5 秒

for (y = 0; y < SCUT_IMAGE_H; y++) SCUT_DrawPoint(right_line[y], y, SCUT_COLOR_BLUE);
SCUT_Flush(500);          // ← 蓝线再显示

SCUT_DrawRect(0, 0, 187, 119, SCUT_COLOR_RED);   // ← 红框最后显示
```

`SCUT_Flush` 的行为特性：

| 特性 | 说明 |
|---|---|
| 生效位置 | **只在 `draw_image_info()` 里有意义**；在 `image_process()` 里调没效果（那时还没进显示阶段） |
| 停顿参数 | `SCUT_Flush(0)` 只刷新不停顿，影响最小 |
| 是否影响计时 | **不影响「处理时间」**，绘图/刷新都在计时区间外 |
| 注意 | 每次调用都会立刻重画整幅图，别调太多次；正式测性能前记得去掉 |

---

## 七、配置文件 `config.h`

分两区，**你只需要关心上半部分**：

**用户配置区（可以随便改）**
- **摄像头图像尺寸**（`SCUT_IMAGE_W` / `SCUT_IMAGE_H`）
- **要显示的图像**（`SCUT_OUT_IMAGE_ARRAY` 数组名，以及 `W` / `H` / `X` / `Y`）
- 测试图片路径与后缀
- 每帧处理重复次数
- 窗口位置与缩放系数
- 日志最大行数
- 热重载开关与冷却时间

**环境底层参数（一般不用改）**
- 是否保留控制台窗口

每个配置项后面都有一句话说明，打开文件就能看懂。

### 常见改法

```c
/* ① 只看图像下半部分（ROI）—— 算法代码不用动 */
#define SCUT_OUT_IMAGE_W      188     // 宽度可以不变
#define SCUT_OUT_IMAGE_H       60     // 只看 60 行高
#define SCUT_OUT_IMAGE_X        0     // 从第 0 列开始
#define SCUT_OUT_IMAGE_Y       60     // 从第 60 行开始

/* ② 换个数组显示（比如你想看中间结果而不是最终结果） */
#define SCUT_OUT_IMAGE_ARRAY   binary_image
#define SCUT_OUT_IMAGE_ARRAY_TYPE  uint8

/* ③ 换摄像头分辨率（记得测试图也要同尺寸） */
#define SCUT_IMAGE_W          160
#define SCUT_IMAGE_H          120
```

> ⚠️ 改 `SCUT_OUT_IMAGE_*` 时注意 `X + W` 不能超过 `SCUT_IMAGE_W`，
> `Y + H` 不能超过 `SCUT_IMAGE_H`。超了会在**编译期**直接报错
> （环境放了静态断言），不会等到运行时才出问题。

---

## 八、热重载：与图像编辑器联动

调光照、调曝光这类工作，如果每次都要「编辑器里改 → 另存为 → 回环境按 `+` 翻页」会很烦。
**热重载**让你省掉后面两步。

### 怎么用

1. 打开环境，翻到你想调的那张图（比如 `pic/1/1.bmp`）
2. 打开图像编辑器，在它右侧的 **仿真联动** 区里：
   - **目标文件夹** → 指向 `pic\1`（编辑器一般能自动猜出来）
   - **目标文件名** → 填 `1`（也就是你正在看的那张）
   - 勾上 **停止操作后自动上传**，等待时间按喜好设（默认 800 毫秒）
3. 在编辑器里随便拖滑块。**停手约 800 毫秒后，环境那边会自动重新读图并重跑算法。**

不想自动的话，取消勾选，改完点 **立即上传到仿真环境** 也一样。

### 它是怎么工作的

编辑器把结果**写到正在显示的那张图本身**上，环境监测该文件的时间戳，一变就重读重跑。
所以**你翻到哪张就盯哪张**，不需要额外目录，也不需要两端直接通信。

对应实现在 `env\hot_reload.c`，对外接口在 `env\hot_reload.h`。

### 几个你可能会关心的点

| 问题 | 处理方式 |
|---|---|
| 算法很慢，会不会被刷新拖垮？ | 有**冷却时间**（默认 300ms），重载请求不会堆积。可在 `config.h` 里用 `SCUT_HOT_RELOAD_COOLDOWN_MS` 调大 |
| 图还没写完就被读了怎么办？ | 有**写完成判定**：文件大小+时间戳连续两次不变才认为写完，不会读到半张图 |
| 会不会和手动按 `+` 冲突？ | 不会。翻页时监测目标会自动切到新图，且以切换后的状态为基准 |
| 做性能测试时想排除干扰？ | 按 `H` 关掉热重载，状态栏会显示 `[热重载 关]` |
| 完全不想用？ | 把 `config.h` 里的 `SCUT_HOT_RELOAD` 改成 `0` |

> 为什么不开线程：EasyX 的 GDI 对象不是线程安全的，多线程改图像缓冲会引入难以复现的崩溃。
> 这里用**轮询文件属性**代替，开销可以忽略，且天然不存在数据竞争。

---

## 九、注意事项

### 编码问题（最容易踩的坑）

**所有源文件必须保持 `UTF-8 with BOM`。**

用记事本「另存为」时选 UTF-8，不要选 ANSI；也不要删掉文件开头的 BOM，否则中文注释会编译报错。

### 关于 C / C++ 混合编译

这个工程是**真正的混合编译**，不是把 `.c` 当 C++ 编：

| 文件 | 编译器 |
|---|---|
| `code\*.c` | **C** (gcc `-std=c11` / MSVC `/TC`) |
| `env\simu_env.c` | **C**（它负责包含 `code\` 下的所有 `.c`） |
| `main.cpp` | **C++** (g++ `-std=c++17` / MSVC `/TP`) |
| `env\disp_env.cpp` | **C++**（EasyX 只能用 C++） |

所以你在 `code\` 里写的是**真 C**：
- 变量声明要放在块的开头
- 不要用 C++ 的东西（引用、模板、`new/delete`、类）
- **不要 include easyx 之类的图形库** —— 这样算法才能直接上单片机

想验证这一点，可以跑：
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\verify_mixed.ps1
```

---

## 十、搬到 CH32 单片机（重要）

### 设计目标：`code\` 原样拷贝就能编译

整个 `code\` 目录**只依赖一份头文件** `code\scut_port.h`，
不包含仿真环境的任何其他头文件（`scut_display.h` / `simu_env.h` / `config.h` 都不碰）。

所以上车时：

```
把 code\ 整个目录拷到你的 CH32 工程里  →  直接编译  →  完成
```

**不需要改算法代码的任何一行。**

### 移植时你要做的只有一件事：提供 `scut_port.h` 里那几个实现

`scut_port.h` 声明了下面这些「环境侧」的东西，车载工程里给一份实现就行：

| 声明的符号 | 仿真环境里的实现 | 车载工程里怎么做 |
|---|---|---|
| `mt9v03x_image[120][188]` | 环境读 BMP 填进去 | 指向摄像头驱动的图像数组（逐飞库通常同名） |
| `SCUT_OutImageSet/Get` | 写显示缓冲 | 写 IPS 屏显存，或写你自己的图像数组 |
| `SCUT_DrawPoint/DrawLine/DrawRect` | EasyX 画到窗口 | 画到 IPS 屏；**不需要就直接删掉这些调用** |
| `SCUT_Log/LogAt/LogClear` | 显示在窗口下方 | 接串口打印；不需要就直接删掉 |

最省事的做法：在车载工程里新建一个 `scut_port_mcu.c`，把这几个函数写成空函数，
先把算法跑起来，之后再逐个接到真实的屏幕 / 串口上。

### 如果车载工程已经有同名类型或颜色宏

`scut_port.h` 已经预留了开关，在你的工程里提前定义即可跳过对应的定义段：

```c
#define SCUT_PORT_HAS_TYPES     // 你已经有 uint8/int16 等类型（逐飞库就有）
#define SCUT_PORT_HAS_COLORS    // 你已经有 SCUT_COLOR_xxx
#define SCUT_PORT_HAS_LOGLEVEL  // 你已经有 scut_log_level_enum
```

> 注意：单片机（IPS 屏）的颜色通常是 **RGB565**，和仿真环境的 `COLORREF` 排列不同。
> 如果你要用屏自带的颜色宏，就定义 `SCUT_PORT_HAS_COLORS`，
> 让车载工程的颜色定义生效，两边不要混用。

### 在仿真环境里它自己是怎么工作的

仿真环境会把 `code\*.c` 全部 `#include` 进 `env\simu_env.c` 这一个编译单元。
此时 `scut_port.h` 会发现环境侧的 `scut_display.h` 已经在场，
于是自动跳过自己那份定义，直接用环境的 —— 两边不会重复定义。

这个判断就是 `scut_port.h` 里的 `SCUT_PORT_STANDALONE` 宏，你不用管它。

### 验证移植是否真的没问题

仓库里可以直接验证「`code\` 单独拿去能不能编」：

```powershell
# 只拷 code\ 下的文件（不带 env\），用 gcc 以 C11 编译
gcc -std=c11 -Wall -Wextra -I code -c code\camera.c -o build\camera.o
```

能编过（且无警告）就说明算法侧是干净的。

### 图片没显示出来？

- 窗口里出现一张**渐变图** = 图片读取失败，检查 `config.h` 里的 `SCUT_PIC_DIR` / `SCUT_PIC_EXT`
- 默认是 `"pic/1"` 和 `".bmp"`
- 图片编号必须**从 1 开始连续**（环境靠文件名统计每组多少张）
- 程序启动时会自动把工作目录切到工程根目录，所以不管 exe 落在哪里都能找到 `pic\`

### 编译报错说找不到 `easyx.h`

图形库没装好：
- Dev-C++ 环境：按 `dev环境配置\环境配置指南.md` 安装 `easyx4mingw`
- VS 环境：装 EasyX 官方安装包（会自动放进 VC 目录）

---

## 十一、给 AI 的补充说明

> 这一节是给「让 AI 帮忙改代码」时用的上下文，人类读者可以跳过。

如果你正在用 AI 助手修改这个工程，请把以下信息一并提供给它：

**工程约束**
1. `code\` 下的文件是**纯 C**（C11），不得引入 C++ 语法或图形库依赖。
   环境侧用 `extern "C"` 导出接口，C 文件调用它们和调普通 C 函数一样。
2. `code\` 下的文件**只能** `#include "scut_port.h"` 这一个头文件，
   不得包含 `scut_display.h` / `simu_env.h` / `config.h` 等环境头文件 ——
   否则代码就没法直接搬到单片机。这是硬约束，改代码时务必守住。
3. `code\` 里可用的接口全部在 `code\scut_port.h` 里声明：
   `SCUT_DrawPoint/DrawLine/DrawRect`、`SCUT_Log/SCUT_LogAt/SCUT_LogClear`、
   `SCUT_Get*Width/Height`、`SCUT_OutImageSet/Get`。
   不要直接调用 EasyX 的 `putpixel` / `outtextxy` 等。
4. 输入**只有** `mt9v03x_image[SCUT_IMAGE_H][SCUT_IMAGE_W]`；
   输出用 `SCUT_OutImageSet(x, y, v)` 写，具体是哪个数组由 `config.h` 的
   `SCUT_OUT_IMAGE_ARRAY` / `W` / `H` / `X` / `Y` 决定，**算法不要硬编码尺寸**。
5. 颜色只能用 `SCUT_COLOR_xxx` 或 `SCUT_RGB(r,g,b)`，
   **不要手写十六进制**（宏内部是 `COLORREF` 的 `0x00BBGGRR` 排列，
   手写 `0xFF0000` 会得到蓝色）。
6. 新增 `.c` / `.h` 放在 `code\` 下即可（支持子文件夹），由 `env\code_filelist.h`
   自动收集进 `simu_env.c` 编译单元，**不需要修改任何工程文件**
   （`gen_filelist.ps1` 会自动同步 `.dev` 与 VS 工程的文件列表）。
   ★ 但 Dev-C++ 按 F11 不会触发这个脚本，所以**新增文件后要先跑一次
   `tools\rebuild.bat`**；VS 的 F5 与命令行脚本都会自动重扫，无需手动。

**编码约定**
- 源文件一律 `UTF-8 with BOM`。
- GCC 用 `-finput-charset=UTF-8 -fexec-charset=GBK`（输出侧 GBK），
  MSVC 用 `/utf-8`。因此 `env\disp_env.cpp` 里区分了两个转换函数：
  - `to_tchar()` —— 用于**界面文字**（按 UTF-8 → wide）
  - `path_to_tchar()` —— 用于**文件路径**（按 `CP_ACP` → wide）
    因为路径来自 `GetModuleFileNameA` 等 ANSI 接口，在中文系统上是 GBK。
  **这两者不能混用**，混用会导致中文路径找不到文件。
- 日志接口内部统一收 `const char*`，`SCUT_Log` 支持 C 的格式化占位符。

**已知设计取舍**
- `SCUT_Log` 保留了 `printf` 风格的格式化字符串（而非逐飞库那种
  `show_int` / `show_string` 分离式接口），因为仿真环境调试的数据种类多、
  变化快，格式化写法效率更高。
- 窗口在日志行数超过可用高度时会**自动加高**并重画当前帧与已有日志，
  相关逻辑在 `SCUT_Log()` 内，修改时注意保持 `s_log_buf` 的同步。
- VS 工程设置了 `<DisableFastUpToDateCheck>true</DisableFastUpToDateCheck>`，
  否则 VS 会跳过整个生成过程，导致 `DSH_GenFileList` 目标不执行、
  新建的 `code\*.c` 不被收录。**不要删掉这个设置。**

**踩过的坑（改代码前务必先读，能省很多时间）**

1. **颜色宏必须是 `COLORREF` 排列，不能再改回 `0xRRGGBB`。**
   EasyX/Windows 的缓冲是 `0x00BBGGRR`（蓝色在低位）。
   早期版本把 `SCUT_COLOR_RED` 写成 `0xFF0000`，然后在
   `disp_env.cpp` 里套一层 `BGR()` 把通道换回来 —— 等于错两次凑成对：
   画点看着是红的，但任何人把颜色宏传给 `settextcolor` / `RGB` 等
   Windows 接口，就会静默变成蓝色。
   现在宏与 `RGB(r,g,b)` **逐位等价**，`put_pixel_proc` **直接赋值、不做任何交换**。
   如果哪天发现颜色又反了，先检查是不是又有人加了一次通道交换。

2. **`SCUT_IMAGE_W/H` 由 `config.h` 定义，`scut_display.h` 里只是 `#ifndef` 兜底。**
   算法侧（`code\`）用 `scut_port.h` 的兜底默认值，所以
   **`code\` 不包含 `config.h` 也能编过**，但此时拿到的是默认 188x120。
   真正的定义只在 `config.h` 一处，别在别处再定义一遍。

3. **`code\` 里绝对不能出现仿真环境的头文件。**
   判断方法：`scut_port.h` 用 `SCUT_PORT_STANDALONE` 宏探测
   `_scut_display_h_` / `_scut_common_typedef_h_` 是否已被包含。
   在仿真环境里这两个宏必然存在（`simu_env.c` 先包含了 `simu_env.h`），
   于是 `scut_port.h` 整段跳过自己的类型/颜色/日志定义，避免重复定义；
   单独拷到单片机时它们不存在，`scut_port.h` 就成为唯一来源。
   **改动包含顺序时要特别小心**：一旦这个探测失效，症状是
   「`redefinition of 'scut_log_level_enum'`」或反过来一片
   「undefined identifier」，两种方向都可能出现。

4. **输出图像的「定义」在 `env\simu_env.c`，不在 `code\`。**
   早期版本把 `output_image` 定义在 `camera.c` 里，还带一个
   `SCUT_OUT_IMAGE_PTR` —— 那等于把显示逻辑塞进用户算法，
   拷到单片机就编不过。现在算法只用 `SCUT_OutImageSet/Get`。
   如果你要给输出加新能力（比如换元素类型），改 `config.h` 的
   `SCUT_OUT_IMAGE_ARRAY_TYPE` + `simu_env.h` 的 `scut_out_pixel_t` typedef，
   **不要**去 `code\` 里加。

5. **算法中间结果要用自己的缓冲，不要「边算边改输出图像」。**
   旧 `camera.c` 先把输入拷进 `output_image` 再原地二值化、扫线，
   导致算法逻辑依赖「输出图像」这个显示概念。现在中间结果放
   `frame_buf` / `bin_buf`，最后一步才写输出。
   这样扫描类算法才能在单片机（没有屏幕）上独立运行。

6. **源文件编码要盯住。**
   git 里存的是 UTF-16 LE，但工作区是 UTF-8(+BOM)。
   用脚本批量改文件时**千万不要**用 PowerShell 的
   `Set-Content -Encoding utf8` 去「还原」一个 UTF-8 文件 ——
   它会按 GBK 读进来再重新编码，中文全部变乱码且**不可逆**。
   改这些文件请用编辑工具直接写，或显式用
   `[System.IO.File]::WriteAllText(path, text, UTF8Encoding($true))`。

7. **画线顺序会影响可见性。**
   `draw_image_info()` 画的东西会覆盖在 `image_process()` 写出的
   二值图像之上。实测把中线（红）放在左右边界（绿/蓝）**之后**画，
   三条线才都看得见。

8. **ROI 的行步长是 `SCUT_IMAGE_W`，不是 `SCUT_OUT_IMAGE_W`。**
   输出数组是 `[SCUT_IMAGE_H][SCUT_IMAGE_W]` 的完整二维数组，
   ROI 只是它上面的一块「窗口」，**行与行之间不连续**。
   把 ROI 指针声明成 `(*)[SCUT_OUT_IMAGE_W]` 会算错行步长：
   实测 188 宽取 ROI 宽 100 时，写 `(0,1)` 会落到第 0 行第 110 列，
   而不是第 1 行第 0 列 —— 显示错位，写入越界（但可能不崩，更难查）。
   **正确做法**：用 `SCUT_OutImageRow(y)` 逐行取，或
   `base + y * SCUT_IMAGE_W + x` 手动算。`env\disp_env.cpp` 的
   `refresh_proc_image()` 就是这么做的。

9. **不要写 `sizeof(mt9v03x_image)`。**
   `scut_port.h` 明确允许移植时把 `mt9v03x_image` 换成车载工程的数组，
   那边可能声明成指针。一旦是指针，`sizeof` 变成 4/8 字节，
   `memcpy` 静默失效、算法一直算旧数据且不报错。
   请用 `SCUT_IMAGE_W * SCUT_IMAGE_H` 显式算字节数。

10. **日志采样行不要写死下标。**
    `config.h` 里 `SCUT_IMAGE_H` 可以改。历史上 `camera.c` 写死了
    `mid_line[90]`，一旦把高度改到 ≤ 90 就是每帧越界读。
    现在用 `SCUT_IMAGE_H / 2`，任何分辨率都安全。

11. **窗口高度会跟着日志行数「动态伸缩」，两处公式必须同步。**
    `SCUT_Log` 在日志变多时会自己 `initgraph` 把窗口加高；
    而 `rebuild_window()` 会按【当前实际日志行数】重算高度 ——
    所以日志变少后窗口会**缩回**正常大小（这是刻意设计，
    避免只是打了两行日志就永久占着 40 行的高度）。
    具体行为：日志多时窗口变高，日志少时回退。

    ★ 要注意的不是「只增不减」，而是**两个公式必须一致**：
          SCUT_Log 里        need_h = y + LOG_LINE_H + 60
          rebuild_window 里  win_h  = log_first_line_y() + log_rows*LOG_LINE_H + 60
    两边的行数基准都是 `s_log_lines`。若只改一处，就会出现
    「这边按 N 行算、那边按更多行撑高」→ 窗口来回跳。
    历史上 `rebuild_window` 只预留 `min(SCUT_LOG_MAX_LINES, 12)` 行，
    而 `SCUT_Log` 能涨到 40 行，差 580px，实测会明显闪 ——
    后来把两边统一到同一个基准才修好。**改任一处务必同时改另一处。**

12. **热重载的路径缓冲是 `MAX_PATH`（260）。**
    读图那边用的是 1024 字节缓冲。路径长于 260 时读图会成功，
    但监测的路径被截断 → 文件属性查询永远失败 → 每过一个冷却周期
    就报一次「需要重载」，变成 300ms 的无限重载循环。
    现已在 `SCUT_HotReloadInitEx` 里检测 `snprintf` 截断，
    截断时直接放弃监测（宁可没有联动，也不要卡死）。

**验证方式**
- 改完建议跑 `tools\verify_mixed.ps1`（完整环境应输出 8 项 PASS）。
  它用 vswhere 自动定位任意版本的 Visual Studio；**若本机没装 VS，
  MSVC 那半会被跳过，脚本会明确报告「只验证了 MinGW 部分」并以
  退出码 2 结束**，不会假装全部通过。
- 两个工具链都要能编过：
  `tools\build_dev.bat norun` 和 `tools\build_vs.bat norun`。
- 改过 `code\` 后，验证「还能不能上单片机」：
  把 `code\` 单独拷到一个临时目录，用
  `gcc -std=c11 -Wall -Wextra -I <临时目录> -c camera.c`
  编译，应该**零警告**通过（此时没有 `config.h`，走的是默认尺寸）。

---

## 十二、常见问题

| 问题 | 解决办法 |
|---|---|
| 窗口显示一张渐变图 | 图片没读到，检查 `config.h` 的 `SCUT_PIC_DIR` / `SCUT_PIC_EXT` |
| 中文显示成乱码 | 源文件必须 `UTF-8 with BOM`，不要存成 ANSI |
| 找不到 `easyx.h` | 装 EasyX（见第七节） |
| 自己加的 `.c` 没被编译 | 先看文件名是不是以 `_` 开头；否则是 Dev-C++ 没重扫 —— 跑一次 `tools\rebuild.bat` 再 F11（VS / 命令行不用手动跑） |
| **画出来颜色不对（红蓝反了）** | 别手写十六进制颜色，用 `SCUT_COLOR_xxx` 或 `SCUT_RGB(r,g,b)`。见第四节「绘图」的说明 |
| **改了输出尺寸/ROI，算法读不到图** | 输出用 `SCUT_OutImageSet/Get`，坐标相对显示区域；不要直接用 `output_image[y][x]` |
| **编译报 `scut_out_roi_check` 负数组** | `X + W` 超过 `SCUT_IMAGE_W`，或 `Y + H` 超过 `SCUT_IMAGE_H`，改 `config.h` |
| **拷到单片机编译不过** | `code\` 里只应 `#include "scut_port.h"`；若还包含了 `scut_display.h` / `config.h` 就是违规了。见第十节 |
| Dev-C++ 打开后项目列表是空的 | `.dev` 必须保持 **GBK 编码 + CRLF 换行**，别用编辑器存成 UTF-8 |
| 日志太多挡住帮助信息 | 不会，窗口会自动变高；想限制行数改 `SCUT_LOG_MAX_LINES` |
| VS 提示「VC 项目不支持通配符」 | 已经修掉了。若你自己加了通配符请改回具体文件列表 |
| VS 里按 F5 找不到 `pic` | 不应出现。环境会自动切工作目录，`.vcxproj.user` 也设了调试工作目录 |

---

*本环境用于华工智能车队 22 届视觉考核，请勿复制外传。*
