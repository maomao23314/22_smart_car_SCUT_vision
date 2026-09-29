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
* 热重载实现。接口说明见 hot_reload.h。
*
* 本文件用 C 编译器编译（与 simu_env.c 一致），只依赖 Windows API，
* 不接触 EasyX，因此可以安全地被 simu_env.c 的编译单元包含。
*
* 核心状态机：
*
*       IDLE ──(发现文件变化)──> SETTLING ──(连续 N 次签名不变)──> 触发重载 ──> COOLDOWN ──> IDLE
*
*   IDLE      文件与上次记录一致，什么都不做
*   SETTLING  检测到变化，但还不能确定写完，继续观察
*   触发      签名稳定，认为写入结束，返回 1
*   COOLDOWN  刚重载过，屏蔽后续变化直到冷却结束
*
* 文件名称          hot_reload
* 适用平台          Windows 主机仿真（EasyX）
*
* 修改记录
* 日期                                      作者                             备注
* 2026-09-24                              视觉组                          first version
*********************************************************************************************************************/

#include "hot_reload.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

/*********************************************************************************************************************
* 参数
*********************************************************************************************************************/

/* 签名连续多少次不变就认为写完了。
 * 编辑器保存一张 188x120 的 BMP 是瞬间的事，2 次（约 2 个主循环）
 * 已经足够稳，又不至于让用户感到延迟。 */
#define HR_STABLE_TICKS             (2)

/* 轮询节流：两次真正去查文件的最短间隔（毫秒）。
 * 主循环可能一秒跑上千次，没必要每次都碰磁盘。 */
#define HR_POLL_INTERVAL_MS         (80)

/* 冷却时间默认值，config.h 里可以覆盖 */
#ifndef SCUT_HOT_RELOAD_COOLDOWN_MS
#define SCUT_HOT_RELOAD_COOLDOWN_MS (300)
#endif

/*********************************************************************************************************************
* 内部状态
*********************************************************************************************************************/

static char           s_path[MAX_PATH]      = {0};      // 当前监测的文件
static int            s_enabled             = 1;        // 是否开启
static int            s_has_baseline        = 0;        // 是否已记录基准签名
static int            s_cooldown_ms         = SCUT_HOT_RELOAD_COOLDOWN_MS;

/* 文件签名：大小 + 最后写入时间 */
typedef struct
{
    unsigned long long size;
    unsigned long long mtime;
    int                valid;      // 0 = 文件当前不存在
}hr_signature_t;

static hr_signature_t s_baseline;                       // 已确认的（上一次重载时的）签名
static hr_signature_t s_last_seen;                      // 上一次轮询看到的签名
static int            s_stable_ticks        = 0;        // 签名连续不变的次数
static int            s_pending             = 0;        // 是否处于 SETTLING
static unsigned long  s_last_poll_tick      = 0;        // 上次轮询时刻
static unsigned long  s_cooldown_until_tick = 0;        // 冷却结束时刻
static int            s_reload_count        = 0;        // 累计重载次数

/*********************************************************************************************************************
* 工具
*********************************************************************************************************************/

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     取当前 tick（毫秒）
* 参数说明     void
* 返回参数     毫秒计数 溢出回绕也没问题（无符号差值总成立）
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static unsigned long hr_now_ms(void)
{
    return (unsigned long)GetTickCount();
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     取文件签名
* 参数说明     sig             输出
* 返回参数     void
* 备注信息     GetFileAttributesExA 比 fopen 轻得多 也不会锁住文件
*              这样编辑器正在写的时候我们依然能读到属性
*-----------------------------------------------------------------------------------------------------------------*/
static void hr_stat(const char* path, hr_signature_t* sig)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;

    sig->size  = 0;
    sig->mtime = 0;
    sig->valid = 0;

    if (path == NULL || path[0] == '\0') { return; }
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &fad) == 0) { return; }
    if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { return; }

    sig->size  = ((unsigned long long)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    sig->mtime = ((unsigned long long)fad.ftLastWriteTime.dwHighDateTime << 32) |
                  (unsigned long long)fad.ftLastWriteTime.dwLowDateTime;
    sig->valid = 1;
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     两个签名是否相同
* 参数说明     a b             待比较的签名
* 返回参数     1 = 相同 0 = 不同
* 备注信息
*-----------------------------------------------------------------------------------------------------------------*/
static int hr_same(const hr_signature_t* a, const hr_signature_t* b)
{
    return (a->valid == b->valid) && (a->valid == 0 ||
           (a->size == b->size && a->mtime == b->mtime));
}

/*********************************************************************************************************************
* 接口实现
*********************************************************************************************************************/

void SCUT_HotReloadInitEx(const char* file_path, int cooldown_ms)
{
    if (cooldown_ms <= 0) { cooldown_ms = SCUT_HOT_RELOAD_COOLDOWN_MS; }
    s_cooldown_ms = cooldown_ms;

    if (file_path == NULL) { s_path[0] = '\0'; }
    else
    {
        snprintf(s_path, sizeof(s_path), "%s", file_path);
    }

    /* 切图时把基准设成「当前状态」，避免刚切过去就被判定为变化而重复重载 */
    hr_stat(s_path, &s_baseline);
    s_last_seen    = s_baseline;
    s_has_baseline = 1;
    s_stable_ticks = 0;
    s_pending      = 0;
    s_last_poll_tick = hr_now_ms();
}

void SCUT_HotReloadInit(const char* file_path)
{
    SCUT_HotReloadInitEx(file_path, 0);
}

/*-------------------------------------------------------------------------------------------------------------------
* 函数简介     轮询一次 判断是否需要重载
* 参数说明     void
* 返回参数     1 = 应重载 0 = 不需要
* 备注信息
*   节流：HR_POLL_INTERVAL_MS 内只真正查一次磁盘
*   冷却：刚重载过的一段时间内不响应新变化 防止算法慢时请求堆积
*   稳定性：签名连续 HR_STABLE_TICKS 次不变才认为写完
*-----------------------------------------------------------------------------------------------------------------*/
int SCUT_HotReloadPoll(void)
{
    unsigned long now;
    hr_signature_t cur;

    if (!s_enabled || !s_has_baseline || s_path[0] == '\0') { return 0; }

    now = hr_now_ms();

    /* 节流：没到间隔就先不查 */
    if ((unsigned long)(now - s_last_poll_tick) < HR_POLL_INTERVAL_MS) { return 0; }
    s_last_poll_tick = now;

    /* 冷却：刚重载过，等一等 */
    if ((long)(now - s_cooldown_until_tick) < 0) { return 0; }

    hr_stat(s_path, &cur);

    /* 与基准一致 → 没变化，清掉挂起状态 */
    if (hr_same(&cur, &s_baseline))
    {
        s_pending      = 0;
        s_stable_ticks = 0;
        s_last_seen    = cur;
        return 0;
    }

    /* 签名与上次看到的一样 → 可能已经写完了，累计稳定次数 */
    if (s_pending && hr_same(&cur, &s_last_seen))
    {
        s_stable_ticks++;
    }
    else
    {
        /* 签名又变了（还在写，或者刚发现变化）→ 重新计数 */
        s_stable_ticks = 0;
        s_pending      = 1;
    }
    s_last_seen = cur;

    if (s_stable_ticks < HR_STABLE_TICKS) { return 0; }

    /* 写入完成 接受这次变化 */
    s_baseline           = cur;
    s_pending            = 0;
    s_stable_ticks       = 0;
    s_cooldown_until_tick = now + (unsigned long)s_cooldown_ms;
    s_reload_count++;

    return 1;
}

void SCUT_HotReloadEnable(int enable)
{
    s_enabled = (enable != 0);

    if (s_enabled)
    {
        /* 重新开启时以当前状态为基准 避免把关闭期间的变化当成新变化 */
        hr_stat(s_path, &s_baseline);
        s_last_seen      = s_baseline;
        s_pending        = 0;
        s_stable_ticks   = 0;
        s_last_poll_tick = hr_now_ms();
    }
}

int SCUT_HotReloadIsEnabled(void) { return s_enabled; }
int SCUT_HotReloadCount(void)     { return s_reload_count; }
