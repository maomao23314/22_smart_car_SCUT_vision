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
* 逆透视变换（俯视图）接口
*
* ★★ 本文件直接来自实车工程 main_chip_4bb7\project\code\Perspective.h，
*    只改了下面几处"环境适配"：★★
*
*      1) #include "main_0.h"  ->  #include "scut_port.h"
*         并补上 MT9V03X_H/W -> SCUT_IMAGE_H/W 的映射，
*         好让下面的 RESULT_ROW / USED_ROW 等宏原样保留、一个字都不用改。
*
*      2) uint8_t -> uint8（仿真环境的 scut_port.h 用的类型名不带 _t）
*
*    数组维度、宏的含义、ImageUsed 的用法全部保持原样。
*
* 【用法速查】
*      ImagePerspective_Init();      // 开机调一次，建映射表（不计入处理时间）
*      v = ImageUsed[i][j];          // 每帧取俯视图像素：i=行 j=列
*
* 【尺寸说明】
*      USED_ROW / USED_COL     参与变换的原图尺寸（就是摄像头图像尺寸）
*      RESULT_ROW / RESULT_COL 生成的俯视图尺寸
*      这里两者都取整幅图大小，所以俯视图和原图一样是 120 x 188。
*
* 文件名称          perspective
* 适用平台          Windows 主机仿真（EasyX） / 可移植到 CH32V307
* 原始作者          苏琦尧（2024年10月17日，实车工程）
*
* 修改记录
* 日期                                      作者                             备注
* 2024-10-17                              苏琦尧                        实车版本
* 2026-09-24                              视觉组                        移植到仿真环境
********************************************************************************************************************/

#ifndef PERSPECTIVE_H_
#define PERSPECTIVE_H_

#include "scut_port.h"

/* 环境适配：车载工程里这两个宏来自逐飞库的摄像头驱动
 * （main_0.h -> MAO_MT9V034.h），仿真环境里没有那个头文件，
 * 所以在这里补上映射。值是一致的（120 / 188），
 * 因此下面的 USED_ROW / RESULT_ROW 等宏无需任何改动。 */
#define         MT9V03X_H               SCUT_IMAGE_H
#define         MT9V03X_W               SCUT_IMAGE_W

#define         USED_ROW                MT9V03X_H                              // 用于透视图的行列
#define         USED_COL                MT9V03X_W

#define RESULT_ROW          MT9V03X_H                                           // 结果图行列
#define RESULT_COL          MT9V03X_W

#ifdef __cplusplus
extern "C" {
#endif

void ImagePerspective_Init(void);

extern uint8 *PerImg_ip[RESULT_ROW][RESULT_COL];

/* *PerImg_ip 定义使用的图像，ImageUsed 为用于巡线和识别的图像。
 *
 * 用法：ImageUsed[i][j] —— i 是行、j 是列，值就是俯视图该处的灰度。
 *
 * ★ ImageUsed 展开后是 (*PerImg_ip)。PerImg_ip 的类型是
 *   「uint8* 的二维数组」，所以 (*PerImg_ip)[i] 得到的是第 i 行那个
 *   「指针的一维数组」，再取 [j] 就是第 (i,j) 个指针，最后解引用拿到灰度。
 *   这是实车工程里调通的写法，保持原样。 */
#define ImageUsed   *PerImg_ip

#ifdef __cplusplus
}
#endif

#endif /* PERSPECTIVE_H_ */
