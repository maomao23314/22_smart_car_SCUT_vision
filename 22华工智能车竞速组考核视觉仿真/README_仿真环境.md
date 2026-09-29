# 22 届视觉组仿真环境 —— 新手上手指南

> 华南理工大学智能车队 22 届招新 · 竞速组视觉方向
> 返回 [仓库总览](../README.md) ｜ 配套工具：[图像曝光与光照编辑器](../图像曝光与光照编辑器/README_编辑器.md)

这个环境在电脑上模拟 MT9V03X 摄像头：**读一张图片 → 变成灰度数组给你 → 你写算法处理 → 环境把原图和处理结果并排显示出来，并把你想看的数据显示在下面。**

你写的算法是**纯 C** 的，可以直接搬到 CH32V307 单片机上运行。

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
| **Dev-C++** | 打开 `视觉仿真环境.dev`，按 `F11` |
| **Visual Studio** | 打开 `VS\visual_simu.sln`，选 `Debug\|x64`，按 `F5`（2017 及以上均可） |
| **命令行** | 双击 `tools\build_dev.bat`（用 MinGW）<br>双击 `tools\build_vs.bat`（用 VS，自动找任意版本） |

三种方式随便挑一种，效果完全一样。**不需要改任何项目属性。**

### 第三步：改代码

打开 `code\camera.c`，把里面的示例算法换成你自己的。**就这样，没了。**

---

## 二、项目结构

```
22th_visual_simu\
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
│   └─ camera.h               算法对外接口与全局变量声明
│
├─ env\                       环境本体（一般不用动）
│   ├─ disp_env.hpp           环境对外接口声明
│   ├─ disp_env.cpp           环境实现：窗口/图片/按键/缩放/计时/显示/日志
│   ├─ scut_display.h         ★ 给你调用的接口（画图、打日志、查询）
│   ├─ scut_common_typedef.h  公共类型与颜色定义（SCUT_COLOR_xxx）
│   ├─ simu_env.h             输入图像数组的声明
│   ├─ simu_env.c             输入图像数组的定义 + 自动收集 code\ 的挂载点
│   ├─ hot_reload.h/.c        热重载：监测当前图片是否被编辑器改写（见第七节）
│   └─ code_filelist.h        自动生成，不要手动改
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
│   └─ verify_mixed.ps1       验证 C/C++ 混合编译是否正常
│
└─ pic\                       测试图集（1~6 组，共 91 张图）
```

---

## 三、你要写的代码在哪

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

编译时会自动扫描并收录。VS 里新建的文件也会自动出现在解决方案资源管理器里。
（文件名以 `_` 开头的会被跳过，可以用来临时屏蔽某个文件。）

---

## 四、接口速查

### 输入：唯一的图像来源

```c
mt9v03x_image[y][x]     // y = 行 0~119    x = 列 0~187    值 = 灰度 0~255
```

> ⚠️ 这个数组是「摄像头」在写的。**处理前请先 `memcpy` 一份副本**，
> 不要边读边改，否则会读到半新半旧的数据。`camera.c` 里有标准写法可以照抄。

### 输出图像：在 `config.h` 里指定

```c
#define SCUT_OUT_IMAGE_ARRAY   output_image   // 要显示的数组名（可以换成你的）
#define SCUT_OUT_IMAGE_W       188            // 显示区域宽度
#define SCUT_OUT_IMAGE_H       120            // 显示区域高度
#define SCUT_OUT_IMAGE_X       0              // 显示起始列
#define SCUT_OUT_IMAGE_Y       0              // 显示起始行
```

在代码里直接写 `output_image[y][x] = ...` 即可。
只想看一块 ROI 时，把 `W/H` 调小、`X/Y` 改成起点，窗口会自动跟着调整。

### 绘图

```c
SCUT_DrawPoint(x, y, SCUT_COLOR_RED);                 // 画点
SCUT_DrawLine(x0, y0, x1, y1, SCUT_COLOR_GREEN);      // 画线（斜线也行）
SCUT_DrawRect(x0, y0, x1, y1, SCUT_COLOR_BLUE);       // 画矩形框
```

坐标就是图像坐标，**越界会自动忽略**，不用自己判断。
颜色见 `env\scut_common_typedef.h` 里的 `SCUT_COLOR_xxx`。

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

> 「处理时间」显示的是**平均每次**的微秒数，所以按 `R` 前后显示的数字可以直接对比。

下方状态栏会显示 `[热重载 开 已刷新 N 次]`，可以据此确认联动是否在工作。

---

## 六、配置文件 `config.h`

分两区，**你只需要关心上半部分**：

**用户配置区（可以随便改）**
- 要显示的图像（数组名、宽高、ROI 起点）
- 测试图片路径与后缀
- 每帧处理重复次数
- 窗口位置与缩放系数
- 日志最大行数

**环境底层参数（一般不用改）**
- 是否保留控制台窗口

每个配置项后面都有一句话说明，打开文件就能看懂。

---

## 七、热重载：与图像编辑器联动

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

## 八、注意事项

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

## 九、给 AI 的补充说明

> 这一节是给「让 AI 帮忙改代码」时用的上下文，人类读者可以跳过。

如果你正在用 AI 助手修改这个工程，请把以下信息一并提供给它：

**工程约束**
1. `code\` 下的文件是**纯 C**（C11），不得引入 C++ 语法或图形库依赖。
   环境侧用 `extern "C"` 导出接口，C 文件调用它们和调普通 C 函数一样。
2. 用户代码**只能**通过 `env\scut_display.h` 暴露的接口与外界交互：
   `SCUT_DrawPoint/DrawLine/DrawRect`、`SCUT_Log/SCUT_LogAt/SCUT_LogClear`、
   `SCUT_Get*Width/Height`。不要直接调用 EasyX 的 `putpixel` / `outtextxy` 等。
3. 输入**只有** `mt9v03x_image[120][188]`；输出图像由 `config.h` 的
   `SCUT_OUT_IMAGE_ARRAY` / `W` / `H` / `X` / `Y` 决定。
4. 新增 `.c` / `.h` 放在 `code\` 下即可，由 `env\code_filelist.h`
   自动收集进 `simu_env.c` 编译单元，**不需要修改任何工程文件**。
   若手工往 VS 工程里加文件，请重新生成一次以同步列表。

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

**验证方式**
- 改完建议跑 `tools\verify_mixed.ps1`（应输出 8 项 PASS）。
- 两个工具链都要能编过：
  `tools\build_dev.bat norun` 和 `tools\build_vs.bat norun`。

---

## 十、常见问题

| 问题 | 解决办法 |
|---|---|
| 窗口显示一张渐变图 | 图片没读到，检查 `config.h` 的 `SCUT_PIC_DIR` / `SCUT_PIC_EXT` |
| 中文显示成乱码 | 源文件必须 `UTF-8 with BOM`，不要存成 ANSI |
| 找不到 `easyx.h` | 装 EasyX（见第七节） |
| 自己加的 `.c` 没被编译 | 跑一次 `一键配置.bat`；文件名不要以 `_` 开头 |
| Dev-C++ 打开后项目列表是空的 | `.dev` 必须保持 **GBK 编码 + CRLF 换行**，别用编辑器存成 UTF-8 |
| 日志太多挡住帮助信息 | 不会，窗口会自动变高；想限制行数改 `SCUT_LOG_MAX_LINES` |
| VS 提示「VC 项目不支持通配符」 | 已经修掉了。若你自己加了通配符请改回具体文件列表 |
| VS 里按 F5 找不到 `pic` | 不应出现。环境会自动切工作目录，`.vcxproj.user` 也设了调试工作目录 |

---

*本环境用于华工智能车队 22 届视觉考核，请勿复制外传。*
