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

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     draw_image_info 绘制图像信息接入
* 参数说明     void
* 返回参数     void
* 使用示例     画点：SCUT_DrawPoint(60, 100, SCUT_COLOR_RED);
*              画线：SCUT_DrawLine(0, 0, 187, 119, SCUT_COLOR_GREEN);
*              画框：SCUT_DrawRect(10, 10, 100, 60, SCUT_COLOR_BLUE);
* 备注信息     坐标就是图像坐标 越界会自动忽略 不用自己判断
*              颜色见 scut_common_typedef.h 里的 SCUT_COLOR_xxx
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

    for (y = 0; y < SCUT_IMAGE_H; y++)
    {
        SCUT_DrawPoint(left_line[y],  y, SCUT_COLOR_GREEN);
        SCUT_DrawPoint(right_line[y], y, SCUT_COLOR_BLUE);
    }
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
    image_init();

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
