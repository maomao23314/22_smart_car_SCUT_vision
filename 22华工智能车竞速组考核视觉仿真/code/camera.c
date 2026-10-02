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
* 用户算法示例：大津法（Otsu）二值化 + 左右扫线。
*
* ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
* ★ 重要：本文件【只是参考示例】，不是能直接用的比赛代码。                ★
* ★                                                                        ★
* ★ 这里的大津法 + 跳变扫线写得非常简单，目的只有一个：把环境的接口       ★
* ★ （输入图像、输出图像、绘图、日志）演示一遍，让你知道代码该写在哪儿、   ★
* ★ 怎么编译、怎么看结果。                                                ★
* ★                                                                        ★
* ★ 它和真正能上赛道的视觉代码【差得很远】，例如：                        ★
* ★   - 没有处理赛道上的十字、环岛、坡道、断路、虚线等特殊元素             ★
* ★   - 没有边线滤波 / 补线 / 丢线保护，跳变点一多中线就会乱跳             ★
* ★   - 扫线只取第一个跳变点，遇到噪点、反光会直接跑偏                     ★
* ★   - 没有做透视变换，也没有按行加权，远端一点点误差会被放大             ★
* ★   - 固定按整幅图处理，没有分区、没有动态阈值、没有置信度判断           ★
* ★                                                                        ★
* ★ 一句话：把它当成「Hello World」，不是「参考答案」。                   ★
* ★ 你需要自己设计算法，或者参考往届开源方案（逐飞、各路校赛开源库等）。   ★
* ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
*
* 【这是纯 C 文件】用 C 编译器编译（不是 C++），所以：
*       变量的声明要放在块的开头
*       不要用 C++ 的东西（引用、模板、new/delete、类）
*       不要 include 图形库  这样算法能直接搬到单片机上
*
* ★★ 移植说明（重要）★★
*   本文件只依赖 code\scut_port.h，不碰仿真环境的任何头文件。
*   把整个 code\ 目录拷到 CH32 车载工程里即可编译，不需要改代码。
*   上车时要替换的只有 scut_port.h 里那几项（图像数组、绘图、日志），
*   算法本身一个字都不用动。
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
int otsu_threshold = 0;                                                         // 大津法阈值
int left_line[SCUT_IMAGE_H];                                                    // 左边界
int right_line[SCUT_IMAGE_H];                                                   // 右边界
int mid_line[SCUT_IMAGE_H];                                                     // 中线
int edge_count = 0;                                                             // 本帧跳变点数

static uint8 frame_buf[SCUT_IMAGE_H][SCUT_IMAGE_W];                             // 输入图像副本（防撕裂）
static uint8 bin_buf[SCUT_IMAGE_H][SCUT_IMAGE_W];                               // 二值结果（算法内部用）
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
* 函数简介     左右扫线（跳变法）
* 参数说明     void
* 返回参数     void
* 使用示例     find_line();
* 备注信息     从左往右找「黑→白」作为左边界 从右往左找作为右边界 中线取平均
*              扫的是本地二值结果 bin_buf，不依赖输出图像
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
            if (bin_buf[y][x] == 0 && bin_buf[y][x + 1] == 255)
            {
                left_line[y] = x;
                edge_count++;
                break;
            }
        }

        for (x = SCUT_IMAGE_W - 1; x > left_line[y] + 1; x--)
        {
            if (bin_buf[y][x] == 0 && bin_buf[y][x - 1] == 255)
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
            if (bin_buf[y][x] == 0) { n++; }
        }
    }
    return n;
}

/*********************************************************************************************************************
* 环境调用的两个入口
********************************************************************************************************************/
void image_init(void)
{
    memset(bin_buf,    0, sizeof(bin_buf));
    memset(left_line,  0, sizeof(left_line));
    memset(right_line, 0, sizeof(right_line));
    memset(mid_line,   0, sizeof(mid_line));
    otsu_threshold = 0;
    edge_count     = 0;
}

int image_process(void)
{
    int thre;
    int black_px;
    int x, y;

    /* 1. 拷一份输入（mt9v03x_image 是「摄像头」在写的）
     *
     * ★ 不要写 sizeof(mt9v03x_image)：
     *   scut_port.h 的移植说明里允许把 mt9v03x_image 换成车载工程自己的数组，
     *   而那边有可能声明成指针（extern uint8* mt9v03x_image;）。
     *   一旦是指针，sizeof 会变成 4 或 8 字节 —— 拷贝静默失效，
     *   算法就一直在算旧数据，还不报错，极难排查。
     *   所以这里显式用「宽 × 高」算字节数，与声明形式无关。 */
    memcpy(frame_buf, mt9v03x_image, (size_t)SCUT_IMAGE_W * SCUT_IMAGE_H);

    /* 2. 大津法求阈值，并生成二值结果
     *    中间结果存在算法自己的 bin_buf 里，不依赖输出图像 */
    thre = otsu_get_threshold(&frame_buf[0][0], SCUT_IMAGE_W, SCUT_IMAGE_H);
    otsu_threshold = thre;

    for (y = 0; y < SCUT_IMAGE_H; y++)
    {
        for (x = 0; x < SCUT_IMAGE_W; x++)
        {
            bin_buf[y][x] = (frame_buf[y][x] < thre) ? 0 : 255;
        }
    }

    /* 3. 扫边线（用中间结果算，与显示无关） */
    find_line();
    black_px = count_black();

    /* 4. 把最终结果写进输出图像
     *    输出是哪个数组、多大、哪块 ROI 由 config.h 决定，
     *    算法这边只调 SCUT_OutImageSet，不关心细节 */
    for (y = 0; y < SCUT_OUT_IMAGE_H; y++)
    {
        for (x = 0; x < SCUT_OUT_IMAGE_W; x++)
        {
            SCUT_OutImageSet(x, y, bin_buf[y][x]);
        }
    }

    /* 5. 输出日志（显示在窗口下方「处理时间」下面）。
     *    搬到单片机时把 SCUT_Log 换成串口打印、或者直接删掉都行。
     *
     * ★ 采样行不要写死 90：config.h 里 SCUT_IMAGE_H 是可以改的，
     *   一旦改到 ≤ 90，写死的下标就会越界读（数组外），行为不可预期。
     *   这里用高度的一半，任何分辨率下都合法。 */
    {
        int probe = SCUT_IMAGE_H / 2;

        SCUT_Log(SCUT_LOG_LEVEL_INFO, "大津法阈值: %d", otsu_threshold);
        SCUT_Log(SCUT_LOG_LEVEL_INFO, "跳变点数: %d", edge_count);
        SCUT_Log(SCUT_LOG_LEVEL_INFO, "黑色像素: %d", black_px);
        SCUT_Log(SCUT_LOG_LEVEL_INFO, "中线(第%d行): %d", probe, mid_line[probe]);
        SCUT_Log(SCUT_LOG_LEVEL_WARN, "左线(第%d行): %d   右线: %d",
                 probe, left_line[probe], right_line[probe]);
    }

    return 1;
}
