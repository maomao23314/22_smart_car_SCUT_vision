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
* 用户算法示例：大津法（Otsu）二值化 + 左右扫线。
*
* 【这是纯 C 文件】用 C 编译器编译（不是 C++），所以：
*       变量的声明要放在块的开头
*       不要用 C++ 的东西（引用、模板、new/delete、类）
*       不要 include 图形库  这样算法能直接搬到单片机上
*
* 在这里可以随便加文件：在 code\ 目录下新建 .c / .h 就行，
* 构建脚本会通过 code_filelist.h 自动收集，不用改工程。
*
* 文件名称          camera
* 适用平台          Windows 主机仿真（EasyX） / 可移植到 CH32V307
*
* 修改记录
* 日期                                      作者                             备注
* 2026-09-24                              视觉组                          first version
********************************************************************************************************************/

#include "camera.h"
#include <string.h>

/*********************************************************************************************************************
* 对外变量
********************************************************************************************************************/
uint8 output_image[SCUT_IMAGE_H][SCUT_IMAGE_W];                                 // 要显示的图像

uint8 (*const SCUT_OUT_IMAGE_PTR)[SCUT_OUT_IMAGE_W] =
    (uint8 (*)[SCUT_OUT_IMAGE_W])(&output_image[SCUT_OUT_IMAGE_Y][SCUT_OUT_IMAGE_X]);

typedef char scut_out_size_check[
    (SCUT_OUT_IMAGE_X + SCUT_OUT_IMAGE_W <= SCUT_IMAGE_W &&
     SCUT_OUT_IMAGE_Y + SCUT_OUT_IMAGE_H <= SCUT_IMAGE_H) ? 1 : -1];           // 编译期检查显示区域不越界

int otsu_threshold = 0;                                                         // 大津法阈值
int left_line[SCUT_IMAGE_H];                                                    // 左边界
int right_line[SCUT_IMAGE_H];                                                   // 右边界
int mid_line[SCUT_IMAGE_H];                                                     // 中线
int edge_count = 0;                                                             // 本帧跳变点数

static uint8 frame_buf[SCUT_IMAGE_H][SCUT_IMAGE_W];                             // 输入图像副本（防撕裂）
static int   hist[256];                                                         // 灰度直方图

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     大津法求二值化阈值
* 参数说明     img             灰度图像（行优先）
* 参数说明     w               图像宽度
* 参数说明     h               图像高度
* 返回参数     最佳分割阈值 0 ~ 255
* 使用示例     thre = otsu_get_threshold(&frame_buf[0][0], SCUT_IMAGE_W, SCUT_IMAGE_H);
* 备注信息     遍历 256 个阈值取类间方差最大的那个 光照变化时比固定阈值稳
*-----------------------------------------------------------------------------------------------------------------*/
int otsu_get_threshold(const uint8* img, int w, int h)
{
    int    i, x, y;
    int    total, sum_all = 0, sum_bg = 0, w_bg = 0;
    int    best_thre = 0;
    double var_max = 0.0;
    double mean_bg, mean_fg, var_between;

    for (i = 0; i < 256; i++) { hist[i] = 0; }

    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++) { hist[img[y * w + x]]++; }
    }

    total = w * h;
    for (i = 0; i < 256; i++) { sum_all += i * hist[i]; }

    for (i = 0; i < 256; i++)
    {
        w_bg += hist[i];
        if (w_bg == 0)     { continue; }
        if (w_bg == total) { break; }

        sum_bg += i * hist[i];

        mean_bg = (double)sum_bg / (double)w_bg;
        mean_fg = (double)(sum_all - sum_bg) / (double)(total - w_bg);

        var_between = (double)w_bg * (double)(total - w_bg)
                      * (mean_bg - mean_fg) * (mean_bg - mean_fg);

        if (var_between > var_max)
        {
            var_max   = var_between;
            best_thre = i;
        }
    }

    return best_thre;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     二值化 小于阈值写 0（黑） 否则写 255（白）
* 参数说明     thre            二值化阈值
* 返回参数     void
* 使用示例     binary_img(otsu_threshold);
* 备注信息     结果直接写进 output_image
*-----------------------------------------------------------------------------------------------------------------*/
static void binary_img(int thre)
{
    int x, y;

    for (y = 0; y < SCUT_IMAGE_H; y++)
    {
        for (x = 0; x < SCUT_IMAGE_W; x++)
        {
            output_image[y][x] = (output_image[y][x] < thre) ? 0 : 255;
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     左右扫线（跳变法）
* 参数说明     void
* 返回参数     void
* 使用示例     find_line();
* 备注信息     从左往右找「黑→白」作为左边界 从右往左找作为右边界 中线取平均
*-----------------------------------------------------------------------------------------------------------------*/
static void find_line(void)
{
    int x, y;

    edge_count = 0;

    for (y = 0; y < SCUT_IMAGE_H; y++)
    {
        left_line[y]  = 0;
        right_line[y] = SCUT_IMAGE_W - 1;

        for (x = 0; x < SCUT_IMAGE_W - 1; x++)
        {
            if (output_image[y][x] == 0 && output_image[y][x + 1] == 255)
            {
                left_line[y] = x;
                edge_count++;
                break;
            }
        }

        for (x = SCUT_IMAGE_W - 1; x > left_line[y] + 1; x--)
        {
            if (output_image[y][x] == 0 && output_image[y][x - 1] == 255)
            {
                right_line[y] = x;
                edge_count++;
                break;
            }
        }

        mid_line[y] = (left_line[y] + right_line[y]) / 2;
    }
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     统计黑色像素个数
* 参数说明     void
* 返回参数     黑色像素数量
* 使用示例     n = count_black();
* 备注信息     仅作示例 说明可以按自己的需要扩展
*-----------------------------------------------------------------------------------------------------------------*/
static int count_black(void)
{
    int x, y, n = 0;

    for (y = 0; y < SCUT_IMAGE_H; y++)
    {
        for (x = 0; x < SCUT_IMAGE_W; x++)
        {
            if (output_image[y][x] == 0) { n++; }
        }
    }
    return n;
}

/*********************************************************************************************************************
* 环境调用的两个入口
********************************************************************************************************************/
void image_init(void)
{
    memset(output_image, 0, sizeof(output_image));
    memset(left_line,    0, sizeof(left_line));
    memset(right_line,   0, sizeof(right_line));
    memset(mid_line,     0, sizeof(mid_line));
    otsu_threshold = 0;
    edge_count     = 0;
}

int image_process(void)
{
    int thre;
    int black_px;
    int y;

    /* 1. 拷一份输入（mt9v03x_image 是「摄像头」在写的） */
    memcpy(frame_buf, mt9v03x_image, sizeof(mt9v03x_image));
    memcpy(output_image, frame_buf, sizeof(frame_buf));

    /* 2. 大津法求阈值并二值化 */
    thre = otsu_get_threshold(&frame_buf[0][0], SCUT_IMAGE_W, SCUT_IMAGE_H);
    otsu_threshold = thre;
    binary_img(thre);

    /* 3. 扫边线 */
    find_line();
    black_px = count_black();

    /* 4. 输出日志（显示在窗口下方「处理时间」下面） */
    SCUT_Log(SCUT_LOG_LEVEL_INFO, "大津法阈值: %d", otsu_threshold);
    SCUT_Log(SCUT_LOG_LEVEL_INFO, "跳变点数: %d", edge_count);
    SCUT_Log(SCUT_LOG_LEVEL_INFO, "黑色像素: %d", black_px);
    SCUT_Log(SCUT_LOG_LEVEL_INFO, "中线(第90行): %d", mid_line[90]);
    SCUT_Log(SCUT_LOG_LEVEL_WARN, "左线(第90行): %d   右线: %d", left_line[90], right_line[90]);

    /* 5. 在图像上画出中线 */
    for (y = 0; y < SCUT_IMAGE_H; y += 2)
    {
        SCUT_DrawPoint(mid_line[y], y, SCUT_COLOR_RED);
    }

    return 1;
}
