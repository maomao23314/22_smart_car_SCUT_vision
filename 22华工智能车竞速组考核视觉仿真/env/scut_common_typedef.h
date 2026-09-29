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
* 公共类型与常用颜色定义。
* 命名风格参考逐飞 CH32V307VCT6 开源库（zf_common_typedef / zf_common_font）。
*
* 文件名称          scut_common_typedef
* 适用平台          Windows 主机仿真（EasyX）
*
* 修改记录
* 日期                                      作者                             备注
* 2026-09-24                              视觉组                          first version
********************************************************************************************************************/

#ifndef _scut_common_typedef_h_
#define _scut_common_typedef_h_

#include <stdint.h>

typedef unsigned char           uint8;
typedef signed char             int8;
typedef unsigned short          uint16;
typedef signed short            int16;
typedef unsigned int            uint32;
typedef signed int              int32;

/*********************************************************************************************************************
* 常用颜色（0xRRGGBB 格式）
* 说明：直接用 SCUT_COLOR_xxx 即可，不需要自己算 RGB 值
********************************************************************************************************************/
#define SCUT_COLOR_WHITE            (0xFFFFFF)                                  // 白色
#define SCUT_COLOR_BLACK            (0x000000)                                  // 黑色
#define SCUT_COLOR_RED              (0xFF0000)                                  // 红色
#define SCUT_COLOR_GREEN            (0x00FF00)                                  // 绿色
#define SCUT_COLOR_BLUE             (0x0000FF)                                  // 蓝色
#define SCUT_COLOR_YELLOW           (0xFFFF00)                                  // 黄色
#define SCUT_COLOR_CYAN             (0x00FFFF)                                  // 青色
#define SCUT_COLOR_PURPLE           (0xFF00FF)                                  // 紫色
#define SCUT_COLOR_MAGENTA          (0xFF00FF)                                  // 品红
#define SCUT_COLOR_ORANGE           (0xFF8000)                                  // 橙色
#define SCUT_COLOR_BROWN            (0xBC8040)                                  // 棕色
#define SCUT_COLOR_GRAY             (0x808080)                                  // 灰色
#define SCUT_COLOR_LIGHTGRAY        (0xC0C0C0)                                  // 浅灰
#define SCUT_COLOR_LIGHTBLUE        (0x66CCFF)                                  // 浅蓝
#define SCUT_COLOR_PINK             (0xFE19FE)                                  // 粉色
#define SCUT_COLOR_39C5BB           (0x39C5BB)                                  // 初音绿

#endif
