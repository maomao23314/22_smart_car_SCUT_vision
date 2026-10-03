/*===================================================================================================================
 *  华南理工大学  智能车队                                       SCUT_Smart_car
 *  ---------------------------------------------------------------------------------------------------------------
 *  22 届招新 · 竞速组视觉方向
 *
 *  22 届视觉组仿真环境（SCUT Vision Simulation Environment）
 *  在 PC 上用 EasyX 模拟 MT9V03X 灰度摄像头 + IPS 屏，供视觉算法离线调试。
 *  算法代码（code\*.c）不依赖图形库，可直接移植到 CH32V307 车载程序。
 *
 *  SCUT-IVT-22-BANNER
 *  作者：华南理工大学智能车队 22 届视觉组
 *  许可：GPL-3.0（见仓库根目录 LICENSE）
 *=================================================================================================================*/

/*********************************************************************************************************************
* 22 届视觉组仿真环境（SCUT Vision Simulation Environment）
*
* ★★ 单片机移植兼容层 ★★
*
* 这个头文件存在的唯一目的：让 code\ 下的算法代码【原样拷贝】到
* CH32V307 车载工程（逐飞开源库等）就能编译，不需要改任何一行。
*
* 它做的事：
*   1) 提供算法需要的全部类型（uint8/int16/...）与图像尺寸常量
*   2) 声明输入图像 mt9v03x_image（摄像头写的那个数组）
*   3) 声明输出图像，具体是哪个数组 / 多大 / 取哪块 ROI —— 由 config.h 决定
*   4) 提供绘图与日志接口（SCUT_DrawPoint / SCUT_Log 等）
*
* 移植到单片机时怎么办：
*   算法文件（如 camera.c）只 #include 本文件，不直接碰仿真环境的头文件。
*   上车时把本文件替换成你车载工程里的一份同名头文件即可 —— 常见做法是：
*       - mt9v03x_image      -> 指向摄像头驱动的图像数组
*       - SCUT_DrawPoint 等  -> 留空 / 或指向 IPS 屏驱动
*       - SCUT_Log 等        -> 留空 / 或 printf 到串口
*   算法代码本身一个字都不用改。
*
* 注意：本文件【不包含】windows.h / easyx.h 等任何平台相关头文件，
*       所以在单片机工程里也能直接编译。
*
* 文件名称          scut_port
* 适用平台          Windows 主机仿真（EasyX） / 可移植到 CH32V307
*
* 修改记录
* 日期                                      作者                             备注
* 2026-09-24                              视觉组                          first version
********************************************************************************************************************/

#ifndef _scut_port_h_
#define _scut_port_h_

/*********************************************************************************************************************
* 常量 ------------------------------------------------------------------------
*
* ★ 与仿真环境共存的机制（重要）
*
*   仿真环境里，env\simu_env.c 会把 code\*.c 全部 #include 进同一个编译单元，
*   所以本文件和 env\scut_display.h 会同时出现在一个 .c 里。
*   两边都定义 SCUT_IMAGE_W / 颜色宏 / 日志接口的话就会重复定义。
*
*   解决办法：本文件里凡是仿真环境已经提供的东西，都用 #ifndef 包起来；
*   而仿真环境的头文件（scut_display.h / scut_common_typedef.h）在包含本文件
*   之前就已经定义好了这些宏，于是这里会整段跳过，不会冲突。
*
*   而把 code\ 单独拷到单片机工程时，没有仿真环境的头文件，
*   这些 #ifndef 就会生效，本文件成为唯一来源 —— 两边都能编过。
*
* ★★ 包含顺序无关性（重要，2026 修订）★★
*
*   老写法是「看环境头文件的 include guard 有没有被定义过」来判断自己在哪：
*       #if defined(_scut_display_h_) || defined(_scut_common_typedef_h_)
*   这个判断的答案【取决于谁先被包含】，是靠约定而不是靠机制成立的：
*   它只有在「环境头文件已经先包含进来了」时才为真。
*
*   一旦有人改了包含顺序（例如某个 code\*.c 自己先碰了本文件、
*   或 env\simu_env.c 里调整了 #include 的先后），判断就会翻转成
*   「独立模式」，于是本文件会自己再定义一遍 uint8 / 颜色宏 /
*   scut_log_level_enum，而环境头文件随后再定义一遍 ——
*   报错是 redefinition of 'scut_log_level_enum'，
*   或者反方向的一片 undefined identifier，
*   报错位置和真正的原因（包含顺序）毫无关系，极难排查。
*
*   现在的做法：本文件主动去【探测环境头文件在不在】，在的话就地包含它。
*   这样无论谁先谁后、无论 code\ 下有几十个文件怎么互相包含，
*   答案都一致 —— 判断从「碰巧成立」变成「机制保证」。
*
*   探测用 __has_include（C11 起 GCC/Clang/MSVC 都支持），
*   并用 __has_include 自身是否存在做兜底（很老的编译器）。
*   单片机工程里根本没有 scut_display.h，探测自然失败，
*   于是走独立模式 —— 行为与原来完全一致。
********************************************************************************************************************/

/*-------------------------------------------------------------------------------------------------------------------
* 探测「仿真环境的头文件是否可达」
* 说明：可达 → 就地包含，让环境侧当唯一来源（同时 SCUT_PORT_STANDALONE = 0）
*       不可达 → 独立模式，本文件成为唯一来源（SCUT_PORT_STANDALONE = 1）
* 备注：用户若在自己的工程里提供了同名头文件却被误判，可提前定义
*       SCUT_PORT_FORCE_STANDALONE 强制走独立模式。
*-----------------------------------------------------------------------------------------------------------------*/
#if !defined(SCUT_PORT_STANDALONE)
#if !defined(SCUT_PORT_FORCE_STANDALONE) && defined(__has_include)
#  if __has_include("scut_display.h") && __has_include("scut_common_typedef.h")
#    define SCUT_PORT_HAS_ENV_HEADERS   (1)
#  else
#    define SCUT_PORT_HAS_ENV_HEADERS   (0)
#  endif
#else
#  define SCUT_PORT_HAS_ENV_HEADERS       (0)
#endif

#if SCUT_PORT_HAS_ENV_HEADERS
/* 主动包含，而不是被动等别人先包含 —— 这一句就是「顺序无关」的关键。
 * 头文件自带 include guard，重复包含无副作用。 */
#include "scut_common_typedef.h"
#include "scut_display.h"
#define SCUT_PORT_STANDALONE            (0)                                     // 0 = 跑在仿真环境里
#else
#define SCUT_PORT_STANDALONE            (1)                                     // 1 = 独立（单片机/裸机）
#endif
#endif /* !SCUT_PORT_STANDALONE */

/*********************************************************************************************************************
* 基础类型
* 说明：单片机工程里通常已经有 uint8 / int16 这类定义（逐飞库在 zf_common_typedef.h）。
*       若那边已经定义过，在这里定义 SCUT_PORT_HAS_TYPES 即可避免重复定义。
********************************************************************************************************************/
#ifndef SCUT_PORT_HAS_TYPES
#if SCUT_PORT_STANDALONE
#include <stdint.h>

typedef unsigned char           uint8;
typedef signed char             int8;
typedef unsigned short          uint16;
typedef signed short            int16;
typedef unsigned int            uint32;
typedef signed int              int32;
#define SCUT_PORT_HAS_TYPES             (1)
#else
/* 仿真环境已经给了 uint8 等类型，直接用，不重复定义 */
#define SCUT_PORT_HAS_TYPES             (1)
#endif
#endif /* SCUT_PORT_HAS_TYPES */

/*********************************************************************************************************************
* 摄像头图像尺寸
* 说明：MT9V03X 的常见分辨率就是 188x120。
*       算法里请统一用 SCUT_IMAGE_W / SCUT_IMAGE_H，不要写死 188 / 120。
*       换摄像头（例如 160x120）时只改这里两行。
********************************************************************************************************************/
#ifndef SCUT_IMAGE_W
#define SCUT_IMAGE_W                (188)                                       // 摄像头图像宽度
#endif
#ifndef SCUT_IMAGE_H
#define SCUT_IMAGE_H                (120)                                       // 摄像头图像高度
#endif

/*********************************************************************************************************************
* 输入图像
* 说明：mt9v03x_image[y][x]  y=行 0~119  x=列 0~187  值=灰度 0~255
*       这是「摄像头」在写的数组，也是算法唯一的输入。
*
* 移植时：把下面这行换成你车载工程里摄像头数组的声明即可
*         （逐飞库通常叫 mt9v03x_image 或 mt9v03x_csi_image）。
********************************************************************************************************************/
extern uint8 mt9v03x_image[SCUT_IMAGE_H][SCUT_IMAGE_W];

/*********************************************************************************************************************
* 常用颜色
* 说明：与仿真环境 env\scut_common_typedef.h 保持一致。
*       排列是 Windows 的 COLORREF（0x00BBGGRR），但算法只要用宏名就行，
*       不用关心排列；想自己配颜色请用 SCUT_RGB(r, g, b)。
*
* 移植时：单片机（IPS 屏）的颜色排列常常是 RGB565，与这里不同。
*         如果车载工程已经定义了同名宏，用 SCUT_PORT_HAS_COLORS 屏蔽本节。
*
* ★ 为什么加了 SCUT_PORT_STANDALONE 判断：
*   仿真环境里 env\scut_common_typedef.h 已经定义过这些颜色宏，
*   而 env\simu_env.c 会把 code\*.c（连带本文件）包含进同一个编译单元，
*   于是这里会再定义一遍。两份定义目前字面量完全相同，
*   按 C 标准属于「相同重定义」不报错 —— 但这很脆：
*   哪天有人只改了其中一份，轻则报重定义错误，
*   重则两份悄悄不一致（比如环境里是红、单车上变成蓝）。
*   所以让环境在场时整段跳过，颜色永远只有一份定义来源。
********************************************************************************************************************/
/* ★ 这里的条件用「颜色宏是否已存在」而不是只看 SCUT_PORT_STANDALONE：
 *   SCUT_PORT_STANDALONE 现在由 __has_include 探测得出，二者已经一致；
 *   但只要用户提前定义了 SCUT_PORT_HAS_COLORS（车载工程常见），
 *   或环境侧已经给过 SCUT_COLOR_RED，本节就必须整段跳过。
 *   多这一重 SCUT_COLOR_RED 判断，可以兜住「头文件可达但被条件编译挡住」
 *   之类的边角情况，避免重定义。 */
#if SCUT_PORT_STANDALONE && !defined(SCUT_PORT_HAS_COLORS) && !defined(SCUT_COLOR_RED)
#define SCUT_RGB(r, g, b)           ((unsigned int)(((unsigned char)(r))        \
                                     | (((unsigned int)(unsigned char)(g)) << 8) \
                                     | (((unsigned int)(unsigned char)(b)) << 16)))

#define SCUT_COLOR_WHITE            SCUT_RGB(0xFF, 0xFF, 0xFF)                  // 白色
#define SCUT_COLOR_BLACK            SCUT_RGB(0x00, 0x00, 0x00)                  // 黑色
#define SCUT_COLOR_RED              SCUT_RGB(0xFF, 0x00, 0x00)                  // 红色
#define SCUT_COLOR_GREEN            SCUT_RGB(0x00, 0xFF, 0x00)                  // 绿色
#define SCUT_COLOR_BLUE             SCUT_RGB(0x00, 0x00, 0xFF)                  // 蓝色
#define SCUT_COLOR_YELLOW           SCUT_RGB(0xFF, 0xFF, 0x00)                  // 黄色
#define SCUT_COLOR_CYAN             SCUT_RGB(0x00, 0xFF, 0xFF)                  // 青色
#define SCUT_COLOR_PURPLE           SCUT_RGB(0xFF, 0x00, 0xFF)                  // 紫色
#define SCUT_COLOR_MAGENTA          SCUT_RGB(0xFF, 0x00, 0xFF)                  // 品红
#define SCUT_COLOR_ORANGE           SCUT_RGB(0xFF, 0x80, 0x00)                  // 橙色
#define SCUT_COLOR_BROWN            SCUT_RGB(0xBC, 0x80, 0x40)                  // 棕色
#define SCUT_COLOR_GRAY             SCUT_RGB(0x80, 0x80, 0x80)                  // 灰色
#define SCUT_COLOR_LIGHTGRAY        SCUT_RGB(0xC0, 0xC0, 0xC0)                  // 浅灰
#define SCUT_COLOR_LIGHTBLUE        SCUT_RGB(0x66, 0xCC, 0xFF)                  // 浅蓝
#define SCUT_COLOR_PINK             SCUT_RGB(0xFE, 0x19, 0xFE)                  // 粉色
#define SCUT_COLOR_39C5BB           SCUT_RGB(0x39, 0xC5, 0xBB)                  // 初音绿
#endif /* SCUT_PORT_STANDALONE && !SCUT_PORT_HAS_COLORS && !SCUT_COLOR_RED */

/*********************************************************************************************************************
* 输出图像尺寸（★ 由 config.h 决定）
*
* 说明：算法要显示的图有多大、取哪一块，全部在 config.h 里配置，
*       算法代码只引用下面这些宏，不写死任何数字。
*
*       SCUT_OUT_IMAGE_W / H    输出图像宽 / 高
*       SCUT_OUT_IMAGE_X / Y    输出图像在整幅图里的起点（做 ROI 显示时用）
*
* 在仿真环境里：这些宏由 config.h 定义，同时决定窗口右侧显示的区域。
* 移植到单片机时：算法只需要一个「画在哪」的目标缓冲，
*                 把这几个宏按你的屏幕尺寸改一下就行，
*                 并在你的工程里提供 SCUT_OutImageSet / SCUT_DrawPoint 的实现。
*
* ★★ 这里为什么【不能】只靠 #ifndef，也不能整节跳过（重要）★★
*
*   本节要同时满足两个要求，两者都踩过坑：
*
*   要求一：code\\*.c 先于 config.h 被包含时，这几个宏也必须存在。
*     有人调整包含顺序（或 env\\simu_env.c 里改了 #include 的先后）后，
*     SCUT_OUT_IMAGE_W 会变成未定义，报错是
*         error: 'SCUT_OUT_IMAGE_W' undeclared
*     而报错位置在用户的 camera.c 里，看着像是算法写错了，
*     实际是包含顺序问题 —— 极难排查。所以兜底定义必须一直在。
*
*   要求二：不能和 config.h 的定义打架。
*     config.h 里写的是   #define SCUT_OUT_IMAGE_X   0
*     而早期兜底写的是     #define SCUT_OUT_IMAGE_X   (0)
*     两者字面量【不同】（有无括号），#ifndef 挡不住，于是报
*         warning: "SCUT_OUT_IMAGE_X" redefined
*     替换后的值其实一样，属于纯噪声警告，但会淹没真正的告警。
*
*   ★ 解决办法：兜底定义与 config.h 采用【完全相同的字面量写法】，
*     于是无论谁先谁后，两份定义都逐字相同 ——
*     按 C 标准，相同重定义是合法的，不报错也不警告；
*     而宏本身又永远存在，不会出现 undeclared。
*     两个要求同时满足。
*
*   注意：改这里的任何一个值，都必须同步改 config.h 里对应那一行，
*         否则「相同重定义」的前提被破坏，warning 会立刻回来。
********************************************************************************************************************/
#ifndef SCUT_OUT_IMAGE_W
#define SCUT_OUT_IMAGE_W            SCUT_IMAGE_W                                // 输出宽度（默认整幅）
#endif
#ifndef SCUT_OUT_IMAGE_H
#define SCUT_OUT_IMAGE_H            SCUT_IMAGE_H                                // 输出高度（默认整幅）
#endif
#ifndef SCUT_OUT_IMAGE_X
#define SCUT_OUT_IMAGE_X            0                                           // 输出起点列
#endif
#ifndef SCUT_OUT_IMAGE_Y
#define SCUT_OUT_IMAGE_Y            0                                           // 输出起点行
#endif

/*********************************************************************************************************************
* 日志等级
* 说明：仿真环境里由 scut_display.h 提供，这里跳过避免重复定义。
********************************************************************************************************************/
#if SCUT_PORT_STANDALONE
#ifndef SCUT_PORT_HAS_LOGLEVEL
typedef enum
{
    SCUT_LOG_LEVEL_INFO = 0,                                                    // 普通信息
    SCUT_LOG_LEVEL_WARN,                                                        // 警告
    SCUT_LOG_LEVEL_ERROR,                                                       // 错误
}scut_log_level_enum;
#define SCUT_PORT_HAS_LOGLEVEL          (1)
#endif
#endif /* SCUT_PORT_STANDALONE */

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************************************
* 绘图接口
*
* 说明：坐标就是图像坐标，越界会自动忽略，不用自己判断。
*       颜色用 SCUT_COLOR_xxx 或 SCUT_RGB(r,g,b)。
*
* 仿真环境里这几个函数由 env\disp_env.cpp 实现，这里不重复声明。
*
* 移植到单片机时：如果你不需要画图，在车载工程里给一份空实现即可，
*                 算法里的调用不会因为它们没实现而编译失败。
********************************************************************************************************************/
#if SCUT_PORT_STANDALONE
void        SCUT_DrawPoint              (int x, int y, unsigned int color);     // 画点
void        SCUT_DrawLine               (int x0, int y0, int x1, int y1,
                                         unsigned int color);                   // 画线（斜线也行）
void        SCUT_DrawRect               (int x0, int y0, int x1, int y1,
                                         unsigned int color);                   // 画矩形边框

/*********************************************************************************************************************
* 日志接口
*
* 说明：用法同 printf，会显示在仿真环境窗口下方。
*       移植到单片机时把它接到串口 / IPS 屏，或留空。
********************************************************************************************************************/
void        SCUT_Log                    (scut_log_level_enum level, const char* fmt, ...);
void        SCUT_LogAt                  (int x, int y, scut_log_level_enum level, const char* fmt, ...);
void        SCUT_LogClear               (void);

/*********************************************************************************************************************
* 查询接口
********************************************************************************************************************/
int         SCUT_GetImageWidth          (void);                                 // 输入图像宽度
int         SCUT_GetImageHeight         (void);                                 // 输入图像高度
int         SCUT_GetOutputWidth         (void);                                 // 输出图像宽度
int         SCUT_GetOutputHeight        (void);                                 // 输出图像高度
#endif /* SCUT_PORT_STANDALONE */

#ifdef __cplusplus
}
#endif

#endif /* _scut_port_h_ */
