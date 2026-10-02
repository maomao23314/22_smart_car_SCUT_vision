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
#include "config.h"
#include <string.h>

uint8 mt9v03x_image[SCUT_IMAGE_H][SCUT_IMAGE_W];                                // 模拟摄像头图像

/*********************************************************************************************************************
* 输出图像（★ 由 config.h 配置，不写在 code\ 里）
*
* config.h 里配了四项：
*       SCUT_OUT_IMAGE_ARRAY    要显示的数组名
*       SCUT_OUT_IMAGE_W / H    显示区域宽 / 高
*       SCUT_OUT_IMAGE_X / Y    显示起始列 / 行（ROI 起点）
*
* 这里按这些配置：
*       1) 定义那个数组（尺寸取整幅图，够放下任何 ROI）
*       2) 提供 SCUT_OutImageRow() —— 取 ROI 某一行行首，显示层用它取图
*       3) 提供 SCUT_OutImageSet / Get —— 算法写读像素的入口，自动带 ROI 偏移
*
* 这样「输出到哪、显示多大」全在 config.h 里改，
* code\ 下的算法只引用 SCUT_OUT_IMAGE_W/H 这些宏，可以原样搬到单片机。
********************************************************************************************************************/

/* 编译期检查：显示区域必须是正数，且不能超出图像范围。
 *
 * 注意 X/Y 用「>= 0」判不出负数：config.h 里它们是无后缀的十进制字面量，
 * 在 C 里属于 int，所以这里确实能拦住负值；但如果谁写成 0u 之类的无符号常量，
 * 该判断会恒真。因此下面同时检查 W/H 为正、以及 X+W / Y+H 不越界 ——
 * 这几条才是真正会踩的坑。 */
typedef char scut_out_roi_check[
    (SCUT_OUT_IMAGE_W > 0 && SCUT_OUT_IMAGE_H > 0 &&
     SCUT_OUT_IMAGE_W <= SCUT_IMAGE_W && SCUT_OUT_IMAGE_H <= SCUT_IMAGE_H &&
     SCUT_OUT_IMAGE_X >= 0 && SCUT_OUT_IMAGE_Y >= 0 &&
     SCUT_OUT_IMAGE_X + SCUT_OUT_IMAGE_W <= SCUT_IMAGE_W &&
     SCUT_OUT_IMAGE_Y + SCUT_OUT_IMAGE_H <= SCUT_IMAGE_H) ? 1 : -1];

/* 像素类型由 config.h 的 SCUT_OUT_IMAGE_ARRAY_TYPE 决定（灰度图就是 uint8）。
 * 用 typedef 引进来，换元素类型时只改 config.h 一处。 */
typedef SCUT_OUT_IMAGE_ARRAY_TYPE scut_out_pixel_t;

scut_out_pixel_t SCUT_OUT_IMAGE_ARRAY[SCUT_IMAGE_H][SCUT_IMAGE_W];              // 输出图像（尺寸 = 整幅图）

/* ★ 为什么用「基址 + 行步长」而不是 `(*)[SCUT_OUT_IMAGE_W]` 二维指针？
 *
 * 数组的真实行步长是 SCUT_IMAGE_W（整幅宽度）。若把指针声明成
 *      scut_out_pixel_t (*)[SCUT_OUT_IMAGE_W]
 * 那么 p[y][x] 会按 SCUT_OUT_IMAGE_W 跨行 —— 一旦显示区域比整幅图窄
 * （也就是用了 ROI，SCUT_OUT_IMAGE_W != SCUT_IMAGE_W），
 * 从第 1 行起每一行都会算错地址，写到别的行甚至越界。
 *
 * 实测：188 宽取 ROI 宽 100 时，写 (0,1) 会落到第 0 行的第 110 列，
 *       而不是第 1 行的第 0 列。
 *
 * 所以下面统一用「行首基址 + 真实步长 SCUT_IMAGE_W」来定位，
 * ROI 偏移只在基址上体现一次，之后逐行步进永远正确。 */
static scut_out_pixel_t* const s_out_base =
    &SCUT_OUT_IMAGE_ARRAY[SCUT_OUT_IMAGE_Y][SCUT_OUT_IMAGE_X];

/* 供显示层按 ROI 逐行取图用的行指针访问器。
 * 行步长是真实的 SCUT_IMAGE_W，因此 ROI 偏移与逐行推进都正确。 */
scut_out_pixel_t* SCUT_OutImageRow(int y)
{
    if (y < 0 || y >= SCUT_OUT_IMAGE_H) { return 0; }
    return s_out_base + (size_t)y * SCUT_IMAGE_W;
}

void SCUT_OutImageSet(int x, int y, uint8 value)
{
    if (x >= 0 && x < SCUT_OUT_IMAGE_W && y >= 0 && y < SCUT_OUT_IMAGE_H)
    {
        s_out_base[(size_t)y * SCUT_IMAGE_W + x] = (scut_out_pixel_t)value;
    }
}

uint8 SCUT_OutImageGet(int x, int y)
{
    if (x >= 0 && x < SCUT_OUT_IMAGE_W && y >= 0 && y < SCUT_OUT_IMAGE_H)
    {
        return (uint8)s_out_base[(size_t)y * SCUT_IMAGE_W + x];
    }
    return 0;
}

#if defined(__has_include)
#  if __has_include("code_filelist.h")
#    include "code_filelist.h"
#  else
#    error "env\code_filelist.h 不存在：请运行一次 一键配置.bat 或 tools\gen_filelist.ps1 重新生成，然后重新编译。（没有它链接会报 undefined reference to image_process 之类）"
#  endif
#else
#  include "code_filelist.h"
#endif
