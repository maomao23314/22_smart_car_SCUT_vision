/*===================================================================================================================
 *  华工智能车队
 *  ---------------------------------------------------------------------------------------------------------------
 *  流眄（Liumian）· 视觉算法仿真环境
 *
 *  在 PC 上用 EasyX 模拟 MT9V03X 灰度摄像头 + IPS 屏，供视觉算法离线调试。
 *  算法代码（code\*.c）不依赖图形库，可直接移植到 CH32V307 车载程序。
 *
 *  作者：华工智能车队视觉组
 *  许可：GPL-3.0（见仓库根目录 LICENSE）
 *=================================================================================================================*/

/*********************************************************************************************************************
* 流眄（Liumian）· 视觉算法仿真环境
*
* 公共类型与常用颜色定义。
* 命名风格参考逐飞 CH32V307VCT6 开源库（zf_common_typedef / zf_common_font）。
*
* 文件名称          LM_common_typedef
* 适用平台          Windows 主机仿真（EasyX）
*
* 修改记录
* 日期                                      作者                             备注
* 2026-09-24                              视觉组                          first version
********************************************************************************************************************/

#ifndef _LM_common_typedef_h_
#define _LM_common_typedef_h_

#include <stdint.h>

typedef unsigned char           uint8;
typedef signed char             int8;
typedef unsigned short          uint16;
typedef signed short            int16;
typedef unsigned int            uint32;
typedef signed int              int32;

/*********************************************************************************************************************
* 常用颜色
*
* ★ 格式说明（很重要，改错会红蓝互换）
*
*   下面所有 LM_COLOR_xxx 都是【0x00BBGGRR】排列，也就是 Windows 的 COLORREF。
*   写法与 Windows 的 RGB(r, g, b) 宏完全等价：
*
*       LM_COLOR_RED  ==  RGB(0xFF, 0x00, 0x00)  ==  0x0000FF
*
*   为什么不用更符合直觉的 0xFF0000（0xRRGGBB）？
*       仿真环境的显示层是 EasyX，而 EasyX 画点/取色的底层就是 Windows GDI。
*       如果宏写成 0xRRGGBB，那么：
*         - 内部画点时得额外做一次通道交换，等于错两次凑成对，很难排查；
*         - 用户一旦把 LM_COLOR_xxx 直接传给任何 Windows/EasyX 接口
*           （settextcolor / setlinecolor / RGB 等），颜色就会静默红蓝互换。
*       统一成 COLORREF 之后，宏可以直接喂给任意 Windows 接口，不会再错。
*
*   用户只要用宏名（LM_COLOR_RED 等）就永远不用关心排列顺序。
*   想自己算颜色请统一用 LM_RGB(r, g, b)，不要手写十六进制。
********************************************************************************************************************/

/* 与 Windows RGB(r,g,b) 等价的构造宏（0x00BBGGRR 排列）。
 * 这里不用 Windows 的 RGB 宏，是为了让本文件在任何平台上都能编译
 * —— 算法代码要能直接搬到 CH32 单片机上，不能依赖 windows.h。 */
#define LM_RGB(r, g, b)           ((unsigned int)(((unsigned char)(r))        \
                                     | (((unsigned int)(unsigned char)(g)) << 8) \
                                     | (((unsigned int)(unsigned char)(b)) << 16)))

#define LM_COLOR_WHITE            LM_RGB(0xFF, 0xFF, 0xFF)                  // 白色
#define LM_COLOR_BLACK            LM_RGB(0x00, 0x00, 0x00)                  // 黑色
#define LM_COLOR_RED              LM_RGB(0xFF, 0x00, 0x00)                  // 红色
#define LM_COLOR_GREEN            LM_RGB(0x00, 0xFF, 0x00)                  // 绿色
#define LM_COLOR_BLUE             LM_RGB(0x00, 0x00, 0xFF)                  // 蓝色
#define LM_COLOR_YELLOW           LM_RGB(0xFF, 0xFF, 0x00)                  // 黄色
#define LM_COLOR_CYAN             LM_RGB(0x00, 0xFF, 0xFF)                  // 青色
#define LM_COLOR_PURPLE           LM_RGB(0xFF, 0x00, 0xFF)                  // 紫色
#define LM_COLOR_MAGENTA          LM_RGB(0xFF, 0x00, 0xFF)                  // 品红
#define LM_COLOR_ORANGE           LM_RGB(0xFF, 0x80, 0x00)                  // 橙色
#define LM_COLOR_BROWN            LM_RGB(0xBC, 0x80, 0x40)                  // 棕色
#define LM_COLOR_GRAY             LM_RGB(0x80, 0x80, 0x80)                  // 灰色
#define LM_COLOR_LIGHTGRAY        LM_RGB(0xC0, 0xC0, 0xC0)                  // 浅灰
#define LM_COLOR_LIGHTBLUE        LM_RGB(0x66, 0xCC, 0xFF)                  // 浅蓝
#define LM_COLOR_PINK             LM_RGB(0xFE, 0x19, 0xFE)                  // 粉色
#define LM_COLOR_39C5BB           LM_RGB(0x39, 0xC5, 0xBB)                  // 初音绿

#endif
