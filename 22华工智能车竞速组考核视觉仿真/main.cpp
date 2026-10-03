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
* 程序入口 + 两个「接入函数」。
*
* 【你要写的东西只有两处】
*   1) 图像处理  →  code\camera.c
*          void image_init()       开机初始化（不需要就留空）
*          int  image_process()    每帧的图像处理主函数
*      在 code\ 下可以随便加 .c/.h 文件，构建脚本会自动收集，不用改工程。
*   2) 本文件下面的两个接入函数
*          void draw_image_info()  ① 画点画线 把中间结果显示在图上
*          void show_image_data()  ② SCUT_Log 输出数据
*
* 其余环境细节（窗口、消息、按键、切图片、缩放、计时、显示、日志）
* 全部封装在 env\disp_env.cpp 里。
*
* 可调参数见 config.h，详细说明见 readme.txt。
*
* 文件名称          main
* 适用平台          Windows 主机仿真（EasyX）
*
* 修改记录
* 日期                                      作者                             备注
* 2026-09-24                              视觉组                          first version
********************************************************************************************************************/

#include "disp_env.hpp"

/* 逆透视访问（俯视图映射表）。
 * ★ main.cpp 属于仿真环境本体（不是要搬去单片机的算法），
 *   所以这里可以自由包含 code\ 下的头文件来做可视化。
 *   反过来 code\ 下的算法【不能】包含 env\ 的任何头文件 —— 这个方向是单向的。 */
#include "perspective.h"

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     draw_image_info 绘制图像信息接入
* 参数说明     void
* 返回参数     void
* 使用示例     画点：SCUT_DrawPoint(60, 100, SCUT_COLOR_RED);
*              画线：SCUT_DrawLine(0, 0, 187, 119, SCUT_COLOR_GREEN);
*              画框：SCUT_DrawRect(10, 10, 100, 60, SCUT_COLOR_BLUE);
* 备注信息     坐标就是图像坐标 越界会自动忽略 不用自己判断
*              颜色见 scut_common_typedef.h 里的 SCUT_COLOR_xxx
*              （⚠ 不要手写十六进制颜色，宏内部是 COLORREF 排列；
*                自定义颜色请用 SCUT_RGB(r, g, b)）
*
*              本示例画三条线，颜色分工：
*                  绿 = 左边界 left_line
*                  蓝 = 右边界 right_line
*                  红 = 中线   mid_line
*
*              ★ 这里画的东西【不计入】「处理时间」那个数字。
*                环境只在 image_process() 前后取时间戳，绘图在它之后，
*                不在计时区间内 —— 这是刻意的，车载程序上没有
*                「画点给屏幕看」这件事，算进去会让性能数据失真。
*
*                但绘图仍然是真实的帧开销（实测画 1000 个点约 11us）。
*                想看这个数字：SCUT_EnableDrawTiming(1) 打开，
*                然后读 SCUT_GetDrawCost()，或直接看窗口上显示的
*                「(绘图 x us)」。整帧耗时用 SCUT_GetFrameCost()。
*
*              ★ 想观察绘制顺序（哪些先画、哪些后画）：
*                在两次绘制之间调用 SCUT_Flush(毫秒)，它会立刻把当前
*                内容推上屏幕并停顿，于是能一眼看出先后。
*                例：先画左边界 → SCUT_Flush(500) → 再画右边界
*                只用于调试观察，正式测性能前记得去掉。
*-----------------------------------------------------------------------------------------------------------------*/
void draw_image_info(void)
{
    int y;

    /* 三条线的颜色分工：
     *     左边界 left_line  -> 绿色
     *     右边界 right_line -> 蓝色
     *     中线   mid_line   -> 红色
     *
     * 注意：这里画的东西【不计入】「处理时间」，只算进「绘图」那项。
     *       mid_line 是算法算出来的结果（camera.c 里计算），
     *       但「把它画出来」属于显示，所以放在本函数而不是算法里
     *       —— 这样 code\ 才能原样搬到没有屏幕的单片机上。
     *
     * ★★ 坐标系换算（配置了 ROI 时最容易踩的坑）★★
     *
     *   两个坐标系要分清：
     *
     *     【图像坐标系】 left_line[y] / mid_line[y] 里的行列值。
     *                    y 走 0 ~ SCUT_IMAGE_H-1，列值 0 ~ SCUT_IMAGE_W-1。
     *                    这是算法算出来的坐标，跟显示配置无关。
     *
     *     【显示坐标系】 SCUT_DrawPoint 的 (x, y)。
     *                    它画在【输出图像】上，范围是
     *                    x: 0 ~ SCUT_OUT_IMAGE_W-1
     *                    y: 0 ~ SCUT_OUT_IMAGE_H-1
     *                    （越界会被自动忽略，不会报错、也不会画出来）
     *
     *   默认配置下（SCUT_OUT_IMAGE_W/H == SCUT_IMAGE_W/H，X/Y == 0）
     *   两个坐标系重合，直接画就对。
     *
     *   但 config.h 允许把输出区域改成整幅图的一块子区域（ROI），例如
     *       #define SCUT_OUT_IMAGE_H  60
     *       #define SCUT_OUT_IMAGE_Y  60    // 只显示第 60~119 行
     *   这时两个坐标系就差了一个偏移：
     *       - 图中第 60 行对应显示的 y = 0（要【减】去 SCUT_OUT_IMAGE_Y）
     *       - 第 0~59 行根本不在显示范围内，画了也看不到
     *   所以下面必须：
     *       1) 循环范围用 SCUT_OUT_IMAGE_H（只画显示得到的那部分）
     *       2) 源头下标加上 SCUT_OUT_IMAGE_Y（读对图像坐标系的行）
     *       3) 目标坐标减去 SCUT_OUT_IMAGE_X/Y（换算到显示坐标系）
     *
     *   不做这个换算的话，症状是「线条莫名其妙少了一半、位置也偏了」，
     *   而且不报任何错，很难联想到是 ROI 配置引起的。
     *
     * ★★ 用了逆透视时要额外注意 ★★
     *
     *   上面说的是「算法在原图上算」的情况。如果算法改成读俯视图
     *   （ImageUsed），那 left_line[] 的列值就变成俯视图坐标系的，
     *   和右侧显示的原图坐标系不再对应，需要先把输出图像本身换成
     *   俯视图（在 image_process 里逐像素用 SCUT_OutImageSet 写
     *   ImageUsed 的值），或者把坐标反向映射回原图再画。
     *
     *   本示例没有启用逆透视，所以按原图坐标系处理。 */
#if SCUT_DRAW_SAMPLE_LINES
    for (y = 0; y < SCUT_OUT_IMAGE_H; y++)
    {
        /* 图像坐标系里的行号：显示区域的第 y 行 = 整图的第 y+Y 行 */
        int src_y = y + SCUT_OUT_IMAGE_Y;

        /* 超出算法实际算过的范围就跳过。
         * 正常情况下 SCUT_OUT_IMAGE_Y+SCUT_OUT_IMAGE_H <= SCUT_IMAGE_H
         * 由 simu_env.c 的静态断言保证，这里只是双保险，防止
         * 有人改了断言之外的东西后越界读数组。 */
        if (src_y < 0 || src_y >= SCUT_IMAGE_H) { continue; }

        /* 列方向同理：算法给的列是图像坐标，减去 ROI 起点才是显示坐标。
         * 数组名由 config.h 的 SAMPLE_LINE_* 宏决定，环境不写死。 */
        SCUT_DrawPoint(SAMPLE_LINE_LEFT[src_y]  - SCUT_OUT_IMAGE_X, y, SCUT_COLOR_GREEN);
        SCUT_DrawPoint(SAMPLE_LINE_RIGHT[src_y] - SCUT_OUT_IMAGE_X, y, SCUT_COLOR_BLUE);
        SCUT_DrawPoint(SAMPLE_LINE_MID[src_y]   - SCUT_OUT_IMAGE_X, y, SCUT_COLOR_RED);
    }
#endif
    /* SCUT_DRAW_SAMPLE_LINES == 0 时这里什么都不画。
     * 那是"用户换了自己的算法"的正常情况 —— 不画调试线不该导致编不过，
     * 你自己的可视化代码写在这个函数里即可。 */
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     show_image_data 显示图像数据接入
* 参数说明     void
* 返回参数     void
* 使用示例     SCUT_Log(SCUT_LOG_LEVEL_INFO, "阈值: %d", thre);
*              自动排在「处理时间」下面 一行一行往下走
*              SCUT_LogAt(420, 560, SCUT_LOG_LEVEL_INFO, "长度: %d", len);
*              自己指定窗口坐标
* 备注信息     只支持 C 的格式化字符串（%d %u %f %s %x ...）
*              按 R 做复杂度测试时 第 2 次以后的日志会被自动屏蔽 不会刷屏
*-----------------------------------------------------------------------------------------------------------------*/
void show_image_data(void)
{
    SCUT_Log(SCUT_LOG_LEVEL_INFO, "图像尺寸: %d x %d", SCUT_IMAGE_W, SCUT_IMAGE_H);
    SCUT_Log(SCUT_LOG_LEVEL_INFO, "输出区域: %dx%d @(%d,%d)",
             SCUT_OUT_IMAGE_W, SCUT_OUT_IMAGE_H, SCUT_OUT_IMAGE_X, SCUT_OUT_IMAGE_Y);
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     main 程序入口
* 参数说明     void
* 返回参数     0 = 正常退出
* 使用示例
* 备注信息     标准流程：环境初始化 → 主循环 → 关闭
*-----------------------------------------------------------------------------------------------------------------*/
int main(void)
{
    SCUT_EnvInit();

    /* 你的算法初始化（函数名由 config.h 的 SCUT_USER_INIT_FUNC 决定）。
     *
     * ★ 这是"所有帧开始之前"的初始化环节，且【不计入处理时间】——
     *   环境只在每帧的 image_process() 前后取时间戳，这里在循环之外。
     *   所以建表、标定、清缓冲这类一次性工作放这里最合适，
     *   典型的例子就是逆透视的坐标映射表（只需建一次，之后每帧直接用）。 */
    SCUT_USER_INIT_FUNC();

#if SCUT_ENABLE_PERSPECTIVE_INIT
    /* 逆透视建表（可选，默认关闭）。
     *
     * 为什么需要这个开关：逆透视表只要建一次，之后每帧复用，
     * 所以它属于"初始化"而不是"每帧处理"，放这里不会被计时。
     *
     * 为什么默认关闭：别人的算法是多样化的 —— 不用逆透视的人
     * 代码里根本没有这个函数。若默认就调用，会直接链接失败。
     * 所以默认 0，用了逆透视的人在 config.h 里改成 1 即可。
     *
     * 注意顺序：必须在本帧开始处理之前建好，且要在摄像头图像数组
     * 已就绪之后。这里两帧都还没跑，地址已经确定，是安全的。 */
    SCUT_PERSPECTIVE_INIT_FUNC();
#endif

    /* 想看到「绘图 xx us」就打开这行。默认关闭是因为计时本身有微小开销，
     * 做精细性能测试时不需要它。 */
    SCUT_EnableDrawTiming(1);

    while (SCUT_EnvUpdate())
    {
        if (!SCUT_EnvNeedRender()) { continue; }

        SCUT_EnvProcessImage();

        /* 绘图计时包一层，这样 SCUT_GetDrawCost() 才能读到耗时。
         * 注意：绘图【不计入】「处理时间」那个数字 —— 环境只在
         * image_process() 前后取时间戳。两者是两个独立的量：
         *     处理时间 = 你的算法耗时         （环境自动显示）
         *     绘图耗时 = draw_image_info 的耗时（用 SCUT_GetDrawCost() 读）
         * 想省掉这层计时的开销，可调用 SCUT_EnableDrawTiming(0) 关掉。 */
        SCUT_EnvDrawTimingBegin();
        draw_image_info();
        SCUT_EnvDrawTimingEnd();

        show_image_data();
        SCUT_EnvPresent();
    }

    SCUT_EnvClose();
    return 0;
}
