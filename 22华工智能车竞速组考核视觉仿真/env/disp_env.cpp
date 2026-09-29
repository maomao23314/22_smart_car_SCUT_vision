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
* 环境实现（窗口 / 图片 / 按键 / 缩放 / 计时 / 显示 / 日志）。
* 对外只暴露 disp_env.hpp 与 scut_display.h 里的接口，用户不需要碰这里。
*
* 文件名称          disp_env
* 适用平台          Windows 主机仿真（EasyX）
*
* 字符集处理：
*   源码统一 UTF-8 with BOM，编译参数为
*       GCC  : -finput-charset=UTF-8 -fexec-charset=GBK
*       MSVC : /utf-8
*   于是字符串字面量在 GCC 下是 GBK、在 MSVC 下是 UTF-8。
*   下面 to_tchar() 按编译器正确转换，两边都不用改项目属性，中文不乱码。
*
* 修改记录
* 日期                                      作者                             备注
* 2026-09-24                              视觉组                          first version
********************************************************************************************************************/

#include "disp_env.hpp"
#include "hot_reload.h"

#include <easyx.h>
#include <windows.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>

/*********************************************************************************************************************
* 字符集适配
********************************************************************************************************************/
#define TO_TCHAR_BUF                (1024)

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     把源码里的 const char* 转成 EasyX 当前字符集需要的 TCHAR
* 参数说明     text            源字符串（源码字面量）
* 参数说明     buf             输出缓冲
* 参数说明     buf_count       缓冲长度
* 返回参数     buf
* 备注信息     MSVC + Unicode 时按 UTF-8 转 wide 其它情况原样拷贝
*-----------------------------------------------------------------------------------------------------------------*/
static const TCHAR* to_tchar(const char* text, TCHAR* buf, size_t buf_count)
{
#if defined(_MSC_VER) && defined(UNICODE)
    MultiByteToWideChar(CP_UTF8, 0, text, -1, buf, (int)buf_count);
#else
    (void)buf_count;
    snprintf(buf, buf_count, "%s", text);
#endif
    return buf;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     文件名/路径专用转换
* 参数说明     text            路径字符串
* 参数说明     buf             输出缓冲
* 参数说明     buf_count       缓冲长度
* 返回参数     buf
* 备注信息     不能用 to_tchar：路径来自 ANSI 接口与相对路径拼接，
*              在中文系统上是 GBK 不是 UTF-8，按 UTF-8 转会乱码导致找不到文件
*-----------------------------------------------------------------------------------------------------------------*/
static const TCHAR* path_to_tchar(const char* text, TCHAR* buf, size_t buf_count)
{
#if defined(_MSC_VER) && defined(UNICODE)
    MultiByteToWideChar(CP_ACP, 0, text, -1, buf, (int)buf_count);
#else
    (void)buf_count;
    snprintf(buf, buf_count, "%s", text);
#endif
    return buf;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     统一文字输出 内部自动适配字符集
* 参数说明     x               横坐标
* 参数说明     y               纵坐标
* 参数说明     text            要输出的字符串
* 返回参数     void
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static void text_out(int x, int y, const char* text)
{
    TCHAR buf[TO_TCHAR_BUF];
    outtextxy(x, y, to_tchar(text, buf, TO_TCHAR_BUF));
}

/*********************************************************************************************************************
* 版面参数
********************************************************************************************************************/
#define INFO_X                      (20)                                        // 文字左边界
#define LOG_LINE_H                  (20)                                        // 日志行高
#define LOG_BUF_SIZE                (256)                                       // 单行日志缓冲长度
#define IMG_TOP                     (30)                                        // 图像顶部留白
#define HELP_LINE_H                 (20)                                        // 帮助信息行高
#define PIXEL_BOX_W                 (200)                                       // 右上角灰度值显示区宽度
#define PIXEL_BOX_H                 (24)                                        // 右上角灰度值显示区高度

/*********************************************************************************************************************
* 环境内部状态
********************************************************************************************************************/
static float  s_scale       = SCUT_WINDOW_SCALE;                                // 当前缩放系数
static int    s_running     = 1;                                                // 环境是否运行
static int    s_need_redraw = 1;                                                // 是否需要重新处理与绘制
static int    s_need_reproc = 0;                                                // 只重跑算法 不重新读图片
static int    s_frame_ready = 0;                                                // 是否已备好一帧
static int    s_now_pic     = 1;                                                // 当前图集里的图片编号
static int    s_pic_step    = 0;                                                // 本帧累积的翻页请求
static int    s_past_end    = 0;                                                // 是否已翻过最后一组
static char   s_set_root[MAX_PATH] = {0};                                       // 图集根目录
static char   s_set_dir[MAX_PATH]  = {0};                                       // 当前图集文件夹
static int    s_set_no      = 1;                                                // 当前图集编号
static int    s_set_count   = 0;                                                // 当前图集里的图片张数
static int    s_multi_set   = 0;                                                // 是否可以在 pic 下前后换组
static int    s_repeat      = SCUT_PROCESS_REPEAT;                              // 本帧重复次数
static double s_last_cost   = 0.0;                                              // 上次平均耗时（us）
static int    s_win_w = 0, s_win_h = 0;                                         // 当前窗口尺寸
static int    s_log_lines   = 0;                                                // 当前已输出日志行数
static int    s_log_enabled = 1;                                                // 是否允许输出日志
static char   s_log_buf[SCUT_LOG_MAX_LINES][LOG_BUF_SIZE];                      // 已输出日志内容（窗口重建后补画用）
static char   s_cur_file[MAX_PATH] = {0};                                       // 当前显示的图片路径（热重载监测用）
static int    s_hot_reloaded = 0;                                               // 本帧是否是热重载触发的
static double s_draw_cost    = 0.0;                                             // 上次 draw_image_info 耗时(us)
static double s_frame_cost   = 0.0;                                             // 上次整帧耗时(us)
static int    s_draw_timing  = 0;                                               // 是否测量绘图耗时
static LARGE_INTEGER s_frame_t0;                                                // 整帧起始时间戳
static LARGE_INTEGER s_draw_t0;                                                 // 绘图起始时间戳

static IMAGE  s_orig_img, s_proc_img;                                           // 原图像 / 处理后的图像

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     取两张图中较高的显示高度 作为下方文字区的基线
* 参数说明     void
* 返回参数     基准高度（像素）
* 备注信息     输出图像是 ROI 时高度可能小于原图像 版面必须按较高的算
*-----------------------------------------------------------------------------------------------------------------*/
static int disp_base_h(void)
{
    int h1 = (int)(SCUT_IMAGE_H * s_scale);
    int h2 = (int)(SCUT_OUT_IMAGE_H * s_scale);
    return (h1 > h2) ? h1 : h2;
}

static int time_line_y(void)      { return disp_base_h() + 80; }
static int log_first_line_y(void) { return disp_base_h() + 100; }

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     固定样式的文字输出
* 参数说明     x               横坐标
* 参数说明     y               纵坐标
* 参数说明     text            字符串
* 返回参数     void
* 备注信息     和「处理时间」那一行的样式保持一致
*-----------------------------------------------------------------------------------------------------------------*/
static void info_text(int x, int y, const char* text)
{
    settextstyle(16, 0, _T("宋体"));
    settextcolor(WHITE);
    setbkmode(TRANSPARENT);
    text_out(x, y, text);
}

/*********************************************************************************************************************
* 图片读取
********************************************************************************************************************/
/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     判断 base 目录下是否存在名为 name 的文件夹
* 参数说明     base            基准目录
* 参数说明     name            文件夹名
* 返回参数     1 = 存在 0 = 不存在
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static int has_dir(const char* base, const char* name)
{
    char  full[MAX_PATH];
    DWORD attr;

    if (base[0] == '\0') { snprintf(full, sizeof(full), "%s", name); }
    else                 { snprintf(full, sizeof(full), "%s\\%s", base, name); }

    attr = GetFileAttributesA(full);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     把工作目录切到工程根目录
* 参数说明     void
* 返回参数     1 = 切换成功
* 备注信息     工程根目录 = 同时含 code\ 和 env\ 的那一级。
*              从 exe 所在目录开始逐级往上找。切换之后所有图片都用相对路径读取，
*              这样不管 exe 落在 build\ 还是 VS\x64\Debug\ 都能找到 pic\。
*-----------------------------------------------------------------------------------------------------------------*/
static int chdir_to_project_root(void)
{
    char  dir[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, dir, MAX_PATH);
    char* p;

    if (n == 0 || n >= MAX_PATH) { return 0; }

    for (p = dir; *p; p++) { }
    while (p > dir && *p != '\\' && *p != '/') { p--; }
    if (p > dir) { *p = '\0'; }
    else         { return 0; }

    for (;;)
    {
        if (has_dir(dir, "code") && has_dir(dir, "env"))
        {
            return (SetCurrentDirectoryA(dir) != 0);
        }

        for (p = dir; *p; p++) { }
        while (p > dir && *p != '\\' && *p != '/') { p--; }
        if (p <= dir) { break; }
        *p = '\0';
    }
    return 0;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     判断某个图集文件夹是否存在（相对当前工作目录）
* 参数说明     rel             相对路径 例如 "pic/1"
* 返回参数     1 = 存在 0 = 不存在
* 备注信息     use_set_dir() 用它来确认该图集确实可用
*-----------------------------------------------------------------------------------------------------------------*/
static int dir_exists(const char* rel)
{
    DWORD attr = GetFileAttributesA(rel);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     从文件加载图像到 mt9v03x_image（转灰度）
* 参数说明     filename        相对路径 例如 "pic/1/1.bmp"
* 返回参数     1 = 成功 0 = 失败
* 备注信息     工作目录已经在 SCUT_EnvInit 里切到工程根目录 这里直接用相对路径
*-----------------------------------------------------------------------------------------------------------------*/
static int load_image_to_array(const char* filename)
{
    IMAGE  img;
    TCHAR  path[TO_TCHAR_BUF];
    DWORD* p_buffer;
    int    iw, ih, y, x;

    if (loadimage(&img, path_to_tchar(filename, path, TO_TCHAR_BUF)) != 0)
    {
        printf("无法加载图像: %s\n", filename);
        return 0;
    }

    p_buffer = GetImageBuffer(&img);
    iw = img.getwidth();
    ih = img.getheight();

    for (y = 0; y < SCUT_IMAGE_H; y++)
    {
        for (x = 0; x < SCUT_IMAGE_W; x++)
        {
            int r = 0, g = 0, b = 0;

            if (y < ih && x < iw)
            {
                DWORD c = p_buffer[y * iw + x];
                r = GetRValue(c);
                g = GetGValue(c);
                b = GetBValue(c);
            }
            mt9v03x_image[y][x] = (uint8)(0.299 * r + 0.587 * g + 0.114 * b);
        }
    }
    return 1;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     统计某个图集文件夹里的图片张数
* 参数说明     dir             图集文件夹（相对路径）
* 返回参数     图片张数 0 = 不存在或为空
* 备注信息     文件名是 1..N 的连续编号 最大值就是张数
*-----------------------------------------------------------------------------------------------------------------*/
static int count_images(const char* dir)
{
    char   pattern[MAX_PATH];
    int    max_no = 0;
    WIN32_FIND_DATAA fd;
    HANDLE h;

    snprintf(pattern, sizeof(pattern), "%s/*%s", dir, SCUT_PIC_EXT);

    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) { return 0; }

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { continue; }
        {
            int n = atoi(fd.cFileName);
            if (n > max_no) { max_no = n; }
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return max_no;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     切换到 set_no 号图集
* 参数说明     set_no          图集编号
* 返回参数     该组图片张数 0 = 切换失败
* 备注信息     只有该文件夹里确实有图才真正切过去
*-----------------------------------------------------------------------------------------------------------------*/
static int use_set_dir(int set_no)
{
    char dir[MAX_PATH];
    int  n;

    if (set_no < 1) { return 0; }
    if (!s_multi_set && set_no != 1) { return 0; }

    if (s_multi_set) { snprintf(dir, sizeof(dir), "%s/%d", s_set_root, set_no); }
    else             { snprintf(dir, sizeof(dir), "%s", s_set_root); }

    if (!dir_exists(dir)) { return 0; }

    n = count_images(dir);
    if (n > 0)
    {
        snprintf(s_set_dir, sizeof(s_set_dir), "%s", dir);
        s_set_no    = set_no;
        s_set_count = n;
    }
    return n;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     解析 SCUT_PIC_DIR 末级是不是数字 决定能不能前后换组
* 参数说明     void
* 返回参数     void
* 备注信息     末级是数字（pic/1）时父目录 pic 才是图集根目录
*-----------------------------------------------------------------------------------------------------------------*/
static void init_pic_sets(void)
{
    char*  p;
    char*  sep = NULL;
    size_t len;

    snprintf(s_set_root, sizeof(s_set_root), "%s", SCUT_PIC_DIR);

    len = strlen(s_set_root);
    while (len > 0 && (s_set_root[len - 1] == '/' || s_set_root[len - 1] == '\\'))
    {
        s_set_root[--len] = '\0';
    }

    for (p = s_set_root; *p; p++)
    {
        if (*p == '/' || *p == '\\') { sep = p; }
    }

    s_set_no    = 1;
    s_multi_set = 0;
    if (sep)
    {
        int n = atoi(sep + 1);
        if (n >= 1)
        {
            s_set_no    = n;
            s_multi_set = 1;
            *sep = '\0';
        }
    }

    use_set_dir(s_set_no);
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     前进一步：组内下一张 → 下一组第一张 → 末尾（渐变图）
* 参数说明     void
* 返回参数     void
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static void step_forward(void)
{
    if (s_past_end) { return; }
    if (s_now_pic < s_set_count) { s_now_pic++; return; }
    if (use_set_dir(s_set_no + 1) > 0) { s_now_pic = 1; return; }
    s_past_end = 1;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     后退一步：组内上一张 → 上一组最后一张 → 停在最开头
* 参数说明     void
* 返回参数     void
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static void step_backward(void)
{
    if (s_past_end)
    {
        s_past_end = 0;
        if (s_set_count > 0) { s_now_pic = s_set_count; }
        return;
    }
    if (s_now_pic > 1) { s_now_pic--; return; }
    if (use_set_dir(s_set_no - 1) > 0) { s_now_pic = s_set_count; return; }
    s_now_pic = 1;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     处理本帧累积的翻页请求
* 参数说明     step            正数 = 前进 负数 = 后退
* 返回参数     void
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static void advance_picture(int step)
{
    while (step > 0) { step_forward();  step--; }
    while (step < 0) { step_backward(); step++; }
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     读取当前图片到 mt9v03x_image
* 参数说明     keep_monitor    1 = 不重置热重载监测（用于文件被改写后的原地重读）
*                              0 = 重置（正常翻页/初始化）
* 返回参数     void
* 备注信息     读不到（编号不存在/已翻过最后一组）就生成渐变图 环境不会崩
*-----------------------------------------------------------------------------------------------------------------*/
static void load_image_ex(int keep_monitor)
{
    char filename[MAX_PATH] = {0};
    int  y, x;

    advance_picture(s_pic_step);
    s_pic_step = 0;

    if (!s_past_end && s_set_dir[0] != '\0')
    {
        snprintf(filename, sizeof(filename), "%s/%d%s", s_set_dir, s_now_pic, SCUT_PIC_EXT);
    }

    if (filename[0] != '\0' && load_image_to_array(filename))
    {
        /* 记住当前文件 并把热重载监测切到它身上。
         * 原地重读时（keep_monitor=1）不重置基准，
         * 否则刚重载完就会被当成「又变了」，陷入无限重载。 */
        if (strcmp(s_cur_file, filename) != 0 || !keep_monitor)
        {
            snprintf(s_cur_file, sizeof(s_cur_file), "%s", filename);
            SCUT_HotReloadInitEx(s_cur_file, SCUT_HOT_RELOAD_COOLDOWN_MS);
        }
        return;
    }

    printf("生成渐变图（图片读取失败，请检查 config.h 的 SCUT_PIC_DIR / SCUT_PIC_EXT）\n");
    s_cur_file[0] = '\0';
    SCUT_HotReloadInit(NULL);
    for (y = 0; y < SCUT_IMAGE_H; y++)
    {
        for (x = 0; x < SCUT_IMAGE_W; x++)
        {
            mt9v03x_image[y][x] = (uint8)((x + y) % 255);
        }
    }
}

static void load_image(void)
{
    load_image_ex(0);
}

/*********************************************************************************************************************
* 图像缓冲
********************************************************************************************************************/
/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     从灰度数组生成 EasyX 图像
* 参数说明     img_array       灰度数组（行优先）
* 参数说明     src_w           源宽度
* 参数说明     src_h           源高度
* 参数说明     img             输出图像
* 返回参数     void
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static void create_image_from_array(const uint8* img_array, int src_w, int src_h, IMAGE& img)
{
    int    y, x;
    DWORD* p_buffer;

    img.Resize(src_w, src_h);
    p_buffer = GetImageBuffer(&img);

    for (y = 0; y < src_h; y++)
    {
        for (x = 0; x < src_w; x++)
        {
            uint8 gray = img_array[(size_t)y * src_w + x];
            p_buffer[(size_t)y * src_w + x] = RGB(gray, gray, gray);
        }
    }
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     刷新原图像缓冲（直接用 mt9v03x_image）
* 参数说明     void
* 返回参数     void
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static void refresh_orig_image(void)
{
    create_image_from_array(&mt9v03x_image[0][0], SCUT_IMAGE_W, SCUT_IMAGE_H, s_orig_img);
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     刷新处理后图像缓冲（按 config.h 的 ROI 取一块）
* 参数说明     void
* 返回参数     void
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static void refresh_proc_image(void)
{
    create_image_from_array(&SCUT_OUT_IMAGE_PTR[0][0], SCUT_OUT_IMAGE_W, SCUT_OUT_IMAGE_H, s_proc_img);
}

/*********************************************************************************************************************
* 显示
********************************************************************************************************************/
/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     显示图像（带标题和边框 按当前缩放系数放大）
* 参数说明     img             要显示的图像
* 参数说明     start_x         左上角横坐标
* 参数说明     start_y         左上角纵坐标
* 参数说明     title           标题
* 返回参数     void
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static void display_image(const IMAGE& img, int start_x, int start_y, const char* title)
{
    int   iw = img.getwidth();
    int   ih = img.getheight();
    int   sw = (int)(iw * s_scale);
    int   sh = (int)(ih * s_scale);
    IMAGE scaled;
    HDC   hdc;

    settextstyle(16, 0, _T("宋体"));
    settextcolor(WHITE);
    setbkmode(TRANSPARENT);
    text_out(start_x, start_y - 25, title);

    if (sw < 1) { sw = 1; }
    if (sh < 1) { sh = 1; }

    scaled.Resize(sw, sh);
    hdc = GetImageHDC(&scaled);

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(GetImageHDC(&scaled), 0, 0, sw, sh,
               GetImageHDC(&img),    0, 0, iw, ih, SRCCOPY);

    putimage(start_x, start_y, &scaled, SRCCOPY);
    rectangle(start_x - 1, start_y - 1, start_x + sw, start_y + sh);
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     显示帮助信息与当前图片位置
* 参数说明     void
* 返回参数     void
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static void display_help(void)
{
    char str[256];
    int  help_y = disp_base_h() + 40;

    settextstyle(14, 0, _T("宋体"));
    settextcolor(LIGHTGRAY);
    setbkmode(TRANSPARENT);
    text_out(20, help_y, "+/-:切换图片  Ctrl+/-:缩放  ESC:退出  R:复杂度测试  H:热重载开关  左键:取灰度值");

    if (s_past_end)           { snprintf(str, sizeof(str), "当前图片: 已翻过最后一组（显示渐变图）"); }
    else if (s_set_count <= 0){ snprintf(str, sizeof(str), "当前图片: 读不到图片（检查 config.h 的 SCUT_PIC_DIR / SCUT_PIC_EXT）"); }
    else if (s_multi_set)     { snprintf(str, sizeof(str), "当前图片: 第%d组  %d/%d", s_set_no, s_now_pic, s_set_count); }
    else                      { snprintf(str, sizeof(str), "当前图片: %d/%d", s_now_pic, s_set_count); }

    /* 热重载状态直接显示出来 方便确认与编辑器的联动确实在工作 */
    if (SCUT_HotReloadIsEnabled())
    {
        snprintf(str + strlen(str), sizeof(str) - strlen(str),
                 "    [热重载 开  已刷新 %d 次]", SCUT_HotReloadCount());
    }
    else
    {
        snprintf(str + strlen(str), sizeof(str) - strlen(str), "    [热重载 关]");
    }

    /* 刚触发重载时在行尾加个提示 让用户知道图被换掉了。
     * 注意：不能再往下多占一行 —— 那一行是 SCUT_EnvPresent 写「处理时间」的位置，
     * 会和这里的提示叠在一起（实测确认过）。 */
    if (s_hot_reloaded)
    {
        settextcolor(RGB(0x66, 0xCC, 0xFF));
        snprintf(str + strlen(str), sizeof(str) - strlen(str),
                 "   ← 图片已被外部修改，已自动重新加载");
        settextcolor(LIGHTGRAY);
    }
    text_out(20, help_y + HELP_LINE_H, str);
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     建立/重建窗口：清屏、按缩放系数定尺寸、摆到 config.h 指定的位置
* 参数说明     void
* 返回参数     void
* 备注信息     窗口尺寸没变就不重复 initgraph 避免闪烁
*-----------------------------------------------------------------------------------------------------------------*/
static void rebuild_window(void)
{
    int  img_w  = (int)(SCUT_IMAGE_W * s_scale);
    int  img_w2 = (int)(SCUT_OUT_IMAGE_W * s_scale);
    int  win_w  = img_w + img_w2 + 40;
    int  win_h  = disp_base_h() + 100 + (SCUT_LOG_MAX_LINES > 12 ? 12 : SCUT_LOG_MAX_LINES) * LOG_LINE_H + 60;
    HWND hwnd;

    if (win_w < 820) { win_w = 820; }
    if (win_h < 620) { win_h = 620; }

    if (win_w != s_win_w || win_h != s_win_h)
    {
        initgraph(win_w, win_h, SCUT_USE_CONSOLE ? 1 : 0);
        s_win_w = win_w;
        s_win_h = win_h;
    }

    setbkcolor(BLACK);
    cleardevice();

    hwnd = GetHWnd();
    SetWindowPos(hwnd, NULL, SCUT_WINDOW_X, SCUT_WINDOW_Y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

/*********************************************************************************************************************
* 环境接口实现
********************************************************************************************************************/
void SCUT_EnvInit(void)
{
    chdir_to_project_root();
    init_pic_sets();

    s_log_lines = 0;
    rebuild_window();
    load_image();

    refresh_orig_image();
    refresh_proc_image();

    s_running     = 1;
    s_need_redraw = 1;
    s_need_reproc = 0;
    s_frame_ready = 0;
    s_last_cost   = 0.0;
}

void SCUT_EnvClose(void)
{
    s_running = 0;
    closegraph();
}

int SCUT_EnvUpdate(void)
{
    ExMessage msg;

    if (!s_running) { return 0; }

    while (peekmessage(&msg))
    {
        if (msg.message == WM_KEYDOWN)
        {
            int ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

            if (ctrl)
            {
                switch (msg.vkcode)
                {
                    case VK_ADD:
                    case 0xBB:
                        s_scale += 0.1f;
                        s_need_redraw = 1;
                        break;
                    case VK_SUBTRACT:
                    case 0xBD:
                        if (s_scale > 0.3f) { s_scale -= 0.1f; s_need_redraw = 1; }
                        break;
                    default: break;
                }
            }
            else if (msg.vkcode == VK_ESCAPE)
            {
                s_running = 0;
            }
        }
        else if (msg.message == WM_CHAR)
        {
            switch (msg.ch)
            {
                case '+':
                    s_pic_step += 1;
                    s_need_reproc = 0;
                    s_need_redraw = 1;
                    break;
                case '-':
                    s_pic_step -= 1;
                    s_need_reproc = 0;
                    s_need_redraw = 1;
                    break;
                case 'r':
                case 'R':
                    s_repeat      = 1000;
                    s_need_reproc = 1;
                    s_need_redraw = 1;
                    break;
                case 'h':
                case 'H':
                    /* 热重载开关：方便做性能测试时排除文件监测的干扰 */
                    SCUT_HotReloadEnable(!SCUT_HotReloadIsEnabled());
                    printf("热重载: %s\n", SCUT_HotReloadIsEnabled() ? "开" : "关");
                    s_need_reproc = 0;
                    s_need_redraw = 1;
                    break;
                default: break;
            }
        }
        else if (msg.message == WM_LBUTTONDOWN)
        {
            COLORREF c = getpixel(msg.x, msg.y);
            SCUT_EnvShowPixel(msg.x, msg.y, GetRValue(c));
        }
    }

    if (!s_running) { return 0; }

    /* ---- 热重载检查 ----
     * 每帧轮询一次当前图片是否被外部改写（编辑器保存后触发）。
     * 放在这里而不是开线程，是因为 EasyX 的 GDI 对象不是线程安全的，
     * 多线程改 mt9v03x_image / 图像缓冲会引入难以复现的崩溃。
     * SCUT_HotReloadPoll 自带节流与冷却，开销可以忽略。 */
    s_hot_reloaded = 0;
#if SCUT_HOT_RELOAD
    if (SCUT_HotReloadPoll())
    {
        s_hot_reloaded = 1;
        s_need_reproc  = 0;        // 重新读图（而不是只重跑算法）
        s_need_redraw  = 1;
    }
#endif

    if (s_need_redraw)
    {
        rebuild_window();

        /* 热重载时图片路径没变，用 keep_monitor 避免把监测基准重置掉 */
        if (!s_need_reproc) { load_image_ex(s_hot_reloaded); }
        s_need_reproc = 0;

        s_log_lines   = 0;
        s_frame_ready = 1;
    }
    else
    {
        s_frame_ready = 0;
        Sleep(10);
    }

    return 1;
}

int SCUT_EnvNeedRender(void)
{
    return s_frame_ready;
}

void SCUT_EnvProcessImage(void)
{
    LARGE_INTEGER freq, t0, t1;
    double        total_us;
    int           i;

    QueryPerformanceFrequency(&freq);

    /* 整帧计时从这里开始：处理 → 绘图 → 显示 → 刷新缓冲 */
    QueryPerformanceCounter(&s_frame_t0);

    s_log_enabled = 1;
    QueryPerformanceCounter(&t0);
    image_process();
    QueryPerformanceCounter(&t1);

    if (s_repeat > 1)
    {
        LARGE_INTEGER t2, t3;

        s_log_enabled = 0;
        QueryPerformanceCounter(&t2);
        for (i = 1; i < s_repeat; i++) { image_process(); }
        QueryPerformanceCounter(&t3);
        s_log_enabled = 1;

        total_us = (double)((t1.QuadPart - t0.QuadPart) +
                            (t3.QuadPart - t2.QuadPart)) * 1000000.0 / (double)freq.QuadPart;
    }
    else
    {
        total_us = (double)(t1.QuadPart - t0.QuadPart) * 1000000.0 / (double)freq.QuadPart;
    }

    s_last_cost = total_us / (double)s_repeat;
    s_repeat    = SCUT_PROCESS_REPEAT;

    refresh_orig_image();
    refresh_proc_image();
}

void SCUT_EnvPresent(void)
{
    char str[128];
    LARGE_INTEGER f, t1;

    if (!s_frame_ready) { return; }

    /* 先结算整帧耗时，再画那行文字。
     * 顺序不能反：文字里要显示 s_frame_cost，若放在画完之后算，
     * 显示的就永远是上一帧的值（实测会一直是 0.0）。
     * 这里统计的是「处理 + 绘图」到显示之前的部分；
     * 真正的 putimage 没法算进自己这一帧，那是显示器的固有延迟。 */
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t1);
    s_frame_cost = (double)(t1.QuadPart - s_frame_t0.QuadPart)
                   * 1000000.0 / (double)f.QuadPart;

    /* 「处理时间」是你的算法耗时；后面括号里的绘图/整帧耗时是另外两个量。
     * 分开显示是因为绘图再慢也不该算进算法性能，但它确实影响帧率。 */
    if (s_draw_timing)
    {
        snprintf(str, sizeof(str), "处理时间: %.1f us   (绘图 %.1f us   整帧 %.1f us)",
                 s_last_cost, s_draw_cost, s_frame_cost);
    }
    else
    {
        snprintf(str, sizeof(str), "处理时间: %.1f us   (整帧 %.1f us)",
                 s_last_cost, s_frame_cost);
    }
    info_text(INFO_X, time_line_y(), str);

    display_image(s_orig_img, 5, IMG_TOP, "原图像");
    display_image(s_proc_img, 15 + (int)(SCUT_IMAGE_W * s_scale), IMG_TOP, "处理后的图像");
    display_help();

    s_need_redraw = 0;
    s_frame_ready = 0;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     绘图计时入口（由 draw_image_info 前后调用）
* 参数说明     t0/t1           起止时间戳  t0 == NULL 时表示开始
* 返回参数     void
* 备注信息     只在 SCUT_EnableDrawTiming(1) 后才真正测量
*-----------------------------------------------------------------------------------------------------------------*/
void SCUT_EnvDrawTimingBegin(void)
{
    if (!s_draw_timing) { return; }
    QueryPerformanceCounter(&s_draw_t0);
}

void SCUT_EnvDrawTimingEnd(void)
{
    LARGE_INTEGER f, t1;

    if (!s_draw_timing) { return; }

    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t1);
    s_draw_cost = (double)(t1.QuadPart - s_draw_t0.QuadPart)
                  * 1000000.0 / (double)f.QuadPart;
}

void SCUT_EnvShowPixel(int x, int y, int gray)
{
    char str[64];

    /* 显示在窗口右上角 用浅蓝色 避开日志区与帮助信息 */
    snprintf(str, sizeof(str), "(%d,%d) = %d", x, y, gray);

    setfillcolor(BLACK);
    solidrectangle(s_win_w - PIXEL_BOX_W, 2, s_win_w - 4, 2 + PIXEL_BOX_H);

    settextstyle(16, 0, _T("宋体"));
    settextcolor(RGB(0x66, 0xCC, 0xFF));
    setbkmode(TRANSPARENT);
    text_out(s_win_w - PIXEL_BOX_W + 6, 5, str);
}

/*********************************************************************************************************************
* 用户接口实现（scut_display.h）
********************************************************************************************************************/
extern "C" {

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     在输出图像上写一个像素
* 参数说明     x               横坐标
* 参数说明     y               纵坐标
* 参数说明     color           颜色
* 返回参数     void
* 备注信息     EasyX 缓冲是 0xAARRGGBB 而颜色宏是 0xRRGGBB 所以用 BGR() 换通道
*-----------------------------------------------------------------------------------------------------------------*/
static void put_pixel_proc(int x, int y, unsigned int color)
{
    DWORD* p_buf = GetImageBuffer(&s_proc_img);

    if (x >= 0 && x < SCUT_OUT_IMAGE_W && y >= 0 && y < SCUT_OUT_IMAGE_H)
    {
        p_buf[y * SCUT_OUT_IMAGE_W + x] = BGR((COLORREF)color);
    }
}

void SCUT_DrawPoint(int x, int y, unsigned int color)
{
    put_pixel_proc(x, y, color);
}

void SCUT_DrawLine(int x_start, int y_start, int x_end, int y_end, unsigned int color)
{
    int dx, dy, sx, sy, err;

    if ((x_start < 0 && x_end < 0) || (x_start >= SCUT_OUT_IMAGE_W && x_end >= SCUT_OUT_IMAGE_W) ||
        (y_start < 0 && y_end < 0) || (y_start >= SCUT_OUT_IMAGE_H && y_end >= SCUT_OUT_IMAGE_H))
    {
        return;
    }

    dx = (x_end > x_start) ? (x_end - x_start) : (x_start - x_end);
    dy = (y_end > y_start) ? (y_end - y_start) : (y_start - y_end);
    sx = (x_start < x_end) ? 1 : -1;
    sy = (y_start < y_end) ? 1 : -1;
    err = dx - dy;

    for (;;)
    {
        put_pixel_proc(x_start, y_start, color);
        if (x_start == x_end && y_start == y_end) { break; }

        {
            int e2 = err * 2;
            if (e2 > -dy) { err -= dy; x_start += sx; }
            if (e2 <  dx) { err += dx; y_start += sy; }
        }
    }
}

void SCUT_DrawRect(int x_start, int y_start, int x_end, int y_end, unsigned int color)
{
    SCUT_DrawLine(x_start, y_start, x_end, y_start, color);
    SCUT_DrawLine(x_end, y_start, x_end, y_end, color);
    SCUT_DrawLine(x_end, y_end, x_start, y_end, color);
    SCUT_DrawLine(x_start, y_end, x_start, y_start, color);
}

void SCUT_Log(scut_log_level_enum level, const char* fmt, ...)
{
    char    buf[LOG_BUF_SIZE];
    va_list args;
    int     y;
    int     need_h;

    (void)level;
    if (!s_log_enabled) { return; }

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (s_log_lines >= SCUT_LOG_MAX_LINES) { s_log_lines = 0; }

    y = log_first_line_y() + s_log_lines * LOG_LINE_H;

    /* 日志行数超出窗口剩余空间时 自动把窗口加高 */
    need_h = y + LOG_LINE_H + 60;
    if (need_h > s_win_h)
    {
        initgraph(s_win_w, need_h, SCUT_USE_CONSOLE ? 1 : 0);
        s_win_h = need_h;
        setbkcolor(BLACK);
        cleardevice();

        /* 窗口重建会清掉画面 这里把当前帧和已输出的日志补画一遍 */
        {
            char str[96];
            int  k;

            snprintf(str, sizeof(str), "处理时间: %.1f us", s_last_cost);
            info_text(INFO_X, time_line_y(), str);
            display_image(s_orig_img, 5, IMG_TOP, "原图像");
            display_image(s_proc_img, 15 + (int)(SCUT_IMAGE_W * s_scale), IMG_TOP, "处理后的图像");
            display_help();

            for (k = 0; k < s_log_lines; k++)
            {
                if (s_log_buf[k][0] != '\0')
                {
                    text_out(INFO_X, log_first_line_y() + k * LOG_LINE_H, s_log_buf[k]);
                }
            }
        }
    }

    settextstyle(16, 0, _T("宋体"));
    settextcolor(WHITE);
    setbkmode(TRANSPARENT);
    text_out(INFO_X, y, buf);

    snprintf(s_log_buf[s_log_lines], LOG_BUF_SIZE, "%s", buf);
    s_log_lines++;
}

void SCUT_LogAt(int x, int y, scut_log_level_enum level, const char* fmt, ...)
{
    char    buf[LOG_BUF_SIZE];
    va_list args;

    (void)level;
    if (!s_log_enabled) { return; }

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    settextstyle(16, 0, _T("宋体"));
    settextcolor(WHITE);
    setbkmode(TRANSPARENT);
    text_out(x, y, buf);
}

void SCUT_LogClear(void)
{
    s_log_lines = 0;
}

int SCUT_GetImageWidth(void)    { return SCUT_IMAGE_W; }
int SCUT_GetImageHeight(void)   { return SCUT_IMAGE_H; }
int SCUT_GetOutputWidth(void)   { return SCUT_OUT_IMAGE_W; }
int SCUT_GetOutputHeight(void)  { return SCUT_OUT_IMAGE_H; }

double SCUT_GetDrawCost(void)   { return s_draw_cost; }
double SCUT_GetFrameCost(void)  { return s_frame_cost; }

void SCUT_EnableDrawTiming(int enable)
{
    s_draw_timing = (enable != 0);
    if (!s_draw_timing) { s_draw_cost = 0.0; }
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     立即把输出图像刷到屏幕（强制刷新）
* 参数说明     delay_ms        刷新后停顿毫秒数
* 返回参数     void
* 备注信息     实现要点：
*               1) 绝对【不能】调用 refresh_proc_image()。
*                  那个函数会把 SCUT_OUT_IMAGE_PTR 指向的原始数组
*                  重新拷进 s_proc_img，正好把 SCUT_DrawPoint 刚画上去的
*                  像素全部覆盖掉 —— 实测会导致画面上什么都看不到。
*                  SCUT_DrawPoint 本来就是直接改 s_proc_img 缓冲的，
*                  所以这里只要把 s_proc_img 显示出来即可。
*               2) 不调 SCUT_EnvPresent()：那会清掉 s_frame_ready，
*                  让本帧后面的正常显示被跳过。
*               3) UpdateWindow 让 Windows 立刻提交 GDI 绘制。
*-----------------------------------------------------------------------------------------------------------------*/
void SCUT_Flush(int delay_ms)
{
    if (!s_running) { return; }

    /* 直接显示当前缓冲（里面已含 SCUT_DrawPoint 画的内容） */
    display_image(s_proc_img, 15 + (int)(SCUT_IMAGE_W * s_scale), IMG_TOP,
                  "处理后的图像");

    /* 让 Windows 真正把 GDI 绘制提交出去 */
    UpdateWindow(GetHWnd());

    if (delay_ms > 0) { Sleep(delay_ms); }
}

} /* extern "C" */
