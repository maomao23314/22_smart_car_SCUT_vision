/*===================================================================================================================
 *  华南理工大学  智能车队                                       SCUT Intelligent Vehicle Team
 *  ---------------------------------------------------------------------------------------------------------------
 *  22 届招新 · 竞速组视觉方向                         22nd Recruitment - Vision Group, Racing Division
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
* 模拟摄像头图像数组的定义，以及「自动收集 code\ 目录」的挂载点。
*
* 本文件用 C 编译器编译（gcc / MSVC /TC），是整个自动包含机制的入口：
*       code_filelist.h 由 tools\gen_filelist.ps1 自动生成，
*       里面把 code\ 下所有 .c 文件 #include 进来。
* 构建脚本会先跑 gen_filelist.ps1 再编译本文件；
* VS 工程通过 PreBuildEvent 调用同一个脚本。
*
* ★ 这个文件【跟着仓库走】，不在 .gitignore 里！
*   它缺失时 simu_env.o 会缺少用户算法的全部符号，
*   链接时报一堆「undefined reference to image_process / left_line ...」，
*   和真实原因（清单文件没生成）毫无关系，极难排查。
*   所以正常情况下它应该在仓库里；万一被人删了，
*   下面的 #error 会在编译期直接报人话错误，而不是等到链接期。
*
* 文件名称          simu_env
* 适用平台          Windows 主机仿真（EasyX）
*
* 修改记录
* 日期                                      作者                             备注
* 2026-09-24                              视觉组                          first version
********************************************************************************************************************/

#include "simu_env.h"

uint8 mt9v03x_image[SCUT_IMAGE_H][SCUT_IMAGE_W];                                // 模拟摄像头图像

#if defined(__has_include)
#  if __has_include("code_filelist.h")
#    include "code_filelist.h"
#  else
#    error "env\code_filelist.h 不存在：请运行一次 一键配置.bat 或 tools\gen_filelist.ps1 重新生成，然后重新编译。（没有它链接会报 undefined reference to image_process 之类）"
#  endif
#else
#  include "code_filelist.h"
#endif
