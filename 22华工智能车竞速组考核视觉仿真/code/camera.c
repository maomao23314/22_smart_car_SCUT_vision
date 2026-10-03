/*===================================================================================================================
* 此文档为AI编写，用于测试仿真环境，请替换为实际工程代码
* 此文档为AI编写，用于测试仿真环境，请替换为实际工程代码
* 此文档为AI编写，用于测试仿真环境，请替换为实际工程代码
* 此文档为AI编写，用于测试仿真环境，请替换为实际工程代码
* 此文档为AI编写，用于测试仿真环境，请替换为实际工程代码
* 
* 
* 
* 
* 修改记录
* 日期                                      作者                             备注
* 2026-09-24                              视觉组                          first version
* 2026-10-1                               大肥鱼                           增加逆透视相关测试与兼容
********************************************************************************************************************/

#include "camera.h"
#include <string.h>

/*-------------------------------------------------------------------------------------------------------------------
* 逆透视（俯视图）接入说明
*
* 本示例把逆透视做成【可开关】的，用来演示怎么和仿真环境配合：
*       CAMERA_USE_PERSPECTIVE = 1  用俯视图做处理（算法读 ImageUsed）
*       CAMERA_USE_PERSPECTIVE = 0  直接用原图做处理（不启用时的写法）
* 注意：具体的参数需要自己去测试、设置
*
* ★ perspective.h 用 __has_include 条件包含，不是必需的：
*   perspective.c/.h 只是一个【可选示例】。把它们删掉之后，
*   这里会自动退化成"用原图处理"，本文件照样能编译 ——
*   不会因为少了一个可选示例就整个工程编不过。
*   若你删掉了 perspective 又没把 CAMERA_USE_PERSPECTIVE 改成 0，
*   下面的 #error 会明确告诉你该改哪里，而不是报一堆看不懂的
*   "ImageUsed undeclared"。
*-----------------------------------------------------------------------------------------------------------------*/
#if defined(__has_include)
#  if __has_include("perspective.h")
#    include "perspective.h"
#    define CAMERA_HAVE_PERSPECTIVE 1
#  endif
#endif
#ifndef CAMERA_HAVE_PERSPECTIVE
#  define CAMERA_HAVE_PERSPECTIVE 0
#endif

#define CAMERA_USE_PERSPECTIVE      (1)                                         // 1=用逆透视俯视图 0=用原图

#if CAMERA_USE_PERSPECTIVE && !CAMERA_HAVE_PERSPECTIVE
/* 用 ASCII 写错误信息：中文经过 -fexec-charset=GBK 转换后，
 * #error 的报错文本可能出现乱码/转义告警，反而不易读。
 * 中文说明放在上面注释里（注释不参与转码）。 */
#  error "CAMERA_USE_PERSPECTIVE=1 but perspective.h was not found. Either set CAMERA_USE_PERSPECTIVE to 0, or restore code/perspective.c and code/perspective.h."
#endif

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

#if CAMERA_USE_PERSPECTIVE
    /* ★ 逆透视建表放在这里 —— 这是"所有帧开始之前"的初始化环节。
     *
     * 为什么必须在这里、而不是每帧：
     *   表里存的是【指针】，指向 mt9v03x_image 内部的固定位置。
     *   图像内容每帧都在变，但数组地址不变，所以表建一次就能一直用。
     *   每帧重建既浪费（120*188 次浮点运算）又毫无意义。
     *
     * 为什么这里不计入"处理时间"：
     *   环境只在每帧 image_process() 前后取时间戳，而本函数在
     *   主循环之外、第一帧之前调用一次，所以建表再慢也不影响成绩。
     *
     * ★ 这里重复调用是安全的：如果 config.h 里 SCUT_ENABLE_PERSPECTIVE_INIT
     *   也开了，环境会先调一次、这里再调一次，结果是同一张表（幂等）。
     *   宁可多建一次，也不能漏建 —— 漏建的话 PerImg_ip 全是空指针，
     *   一旦访问就是段错误，而且报错位置在解引用处，很难联想到是初始化漏了。 */
    ImagePerspective_Init();
#endif
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

    /* 2. 大津法求阈值
     *
     * ★ 阈值统一在【原图副本 frame_buf】上求，不在俯视图上求。
     *   原因：俯视图里有一部分区域超出了原图范围，被填成固定的黑值
     *   （见 perspective.c 里的 BlackColor），这些像素不是真实画面。
     *   让它们参与直方图统计会把阈值往黑的方向拉偏。
     *   而且相机亮度这件事本来就只跟原图有关，和做什么变换无关。 */
    thre = otsu_get_threshold(&frame_buf[0][0], SCUT_IMAGE_W, SCUT_IMAGE_H);
    otsu_threshold = thre;

    /* 3. 二值化
     *    取图来源由 CAMERA_USE_PERSPECTIVE 决定：
     *        开了逆透视 -> ImageUsed[y][x]（俯视图，原始工程就是这种用法）
     *        没开       -> frame_buf[y][x]（原图副本）
     *    两种写法都是直接二维下标，不需要额外的取图函数。 */
    for (y = 0; y < SCUT_IMAGE_H; y++)
    {
        for (x = 0; x < SCUT_IMAGE_W; x++)
        {
#if CAMERA_USE_PERSPECTIVE
            uint8 v = ImageUsed[y][x];
#else
            uint8 v = frame_buf[y][x];
#endif
            bin_buf[y][x] = (v < thre) ? 0 : 255;
        }
    }

    /* 4. 扫边线（用中间结果算，与显示无关） */
    find_line();
    black_px = count_black();

    /* 5. 把最终结果写进输出图像
     *    输出是哪个数组、多大、哪块 ROI 由 config.h 决定，
     *    算法这边只调 SCUT_OutImageSet，不关心细节 */
    for (y = 0; y < SCUT_OUT_IMAGE_H; y++)
    {
        for (x = 0; x < SCUT_OUT_IMAGE_W; x++)
        {
            SCUT_OutImageSet(x, y, bin_buf[y][x]);
        }
    }

    /* 6. 输出日志（显示在窗口下方「处理时间」下面）。
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

        /* 把"现在用的是哪种图"打出来，避免调参时看错画面还不知道。
         * 显示出来的二值图是俯视图还是原图，全靠这个开关，很容易搞混。 */
#if CAMERA_USE_PERSPECTIVE
        SCUT_Log(SCUT_LOG_LEVEL_INFO, "图像来源: 逆透视俯视图");
#else
        SCUT_Log(SCUT_LOG_LEVEL_INFO, "图像来源: 原始图像");
#endif
    }

    return 1;
}
