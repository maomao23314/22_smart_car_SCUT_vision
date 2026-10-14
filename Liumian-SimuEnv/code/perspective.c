//仅供参考、测试仿真环境，实际使用请更换参数/算法
//参考链接https://blog.csdn.net/wu58430/article/details/126317900
//仅供参考、测试仿真环境，实际使用请更换参数/算法
//参考链接https://blog.csdn.net/wu58430/article/details/126317900
//仅供参考、测试仿真环境，实际使用请更换参数/算法
//参考链接https://blog.csdn.net/wu58430/article/details/126317900
//仅供参考、测试仿真环境，实际使用请更换参数/算法
//参考链接https://blog.csdn.net/wu58430/article/details/126317900




#include "perspective.h"

#define PER_IMG     mt9v03x_image                                               // 用于透视变换的图像

uint8* PerImg_ip[RESULT_ROW][RESULT_COL];

static uint8 BlackColor = 0;                                                    // 无内容部分像素值

/* 去畸变参数 */
double cameraMatrix[3][3] = { {148.404692,0.000000,96.738556},
                              {0.000000,151.874202,50.592531},
                              {0.000000,0.000000,1.000000} };
double distCoeffs[5] = { -1.101773,1.298787,-0.006304,0.004262,-0.684578 };
int move_xy[2] = { 0,-9 };

/* 逆透视参数
 *
 * ★ 这一行是实车标定的结果，对应实车相机（视场角、安装高度、俯仰角）。
 *   仿真环境的测试图集是另一套合成相机拍的，所以直接拿它跑仿真图时，
 *   画面不会是一张标准俯视图 —— 这属于"参数需要按你的相机重新标定"，
 *   不是代码问题。想重标就改这一行（改完重新编译即可）。
 *   下面被注释掉的另外两组是实车调试时试过的其它值，留作参考。 */
//double change_un_Mat[3][3] = { {0.782976,-0.496506,21.700241},{0.000000,0.374399,-0.980273},{0.000000,-0.005282,1.013830} };
//double change_un_Mat[3][3] = { {0.528455,-0.465701,30.891514},{0.000000,0.214685,6.192835},{0.000000,-0.004954,0.857088} };//69/48/62/66/126/66/74/25/114/25
double change_un_Mat[3][3] = { {0.333333,-0.380787,34.597222},{0.000000,0.121528,8.958333},{0.000000,-0.004051,0.701389} };//66/60/62/66/126/66/76/24/112/24

/*******************************************************/
void find_xy(int x, int y, int local[2]) {
    double fx = cameraMatrix[0][0]
        , fy = cameraMatrix[1][1]
        , ux = cameraMatrix[0][2]
        , uy = cameraMatrix[1][2]
        , k1 = distCoeffs[0]
        , k2 = distCoeffs[1]
        , k3 = distCoeffs[4]
        , p1 = distCoeffs[2]
        , p2 = distCoeffs[3];
    double xCorrected = (x - ux) / fx;
    double yCorrected = (y - uy) / fy;
    double xDistortion, yDistortion;
    double r2 = xCorrected * xCorrected + yCorrected * yCorrected;
    double deltaRa = 1. + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2;
    double deltaRb = 1 / (1.);
    double deltaTx = 2. * p1 * xCorrected * yCorrected + p2 * (r2 + 2. * xCorrected * xCorrected);
    double deltaTy = p1 * (r2 + 2. * yCorrected * yCorrected) + 2. * p2 * xCorrected * yCorrected;
    xDistortion = xCorrected * deltaRa * deltaRb + deltaTx;
    yDistortion = yCorrected * deltaRa * deltaRb + deltaTy;
    xDistortion = xDistortion * fx + ux;
    yDistortion = yDistortion * fy + uy;
    if (yDistortion >= 0 && yDistortion < USED_ROW && xDistortion >= 0 && xDistortion < USED_COL) {
        local[0] = (int)yDistortion;
        local[1] = (int)xDistortion;
    }
    else {
        local[0] = -1;
        local[1] = -1;
    }
}

void find_xy1(int x, int y, int local[2]) {
    int local_x = (int)((change_un_Mat[0][0] * x
        + change_un_Mat[0][1] * y + change_un_Mat[0][2])
        / (change_un_Mat[2][0] * x + change_un_Mat[2][1] * y
            + change_un_Mat[2][2]));
    int local_y = (int)((change_un_Mat[1][0] * x
        + change_un_Mat[1][1] * y + change_un_Mat[1][2])
        / (change_un_Mat[2][0] * x + change_un_Mat[2][1] * y
            + change_un_Mat[2][2]));
    if (local_x
        >= 0 && local_y >= 0 && local_x < RESULT_COL && local_y < RESULT_ROW) {
        local[0] = local_y;
        local[1] = local_x;
    }
    else {
        local[0] = -1;
        local[1] = -1;
    }
}

void ImagePerspective_Init() {
    for (int i = 0; i < RESULT_ROW; i++) {
        for (int j = 0; j < RESULT_COL; j++) {
            int local_xy[2] = { -1 };
            find_xy1(j, i, local_xy);
            if (local_xy[0] != -1 && local_xy[0] != -1) {
                int local_xy1[2] = { -1 };
                find_xy(local_xy[1] - move_xy[0], local_xy[0] - move_xy[1], local_xy1);
                if (local_xy1[0] != -1 && local_xy1[1] != -1) {
                    PerImg_ip[i][j] = &PER_IMG[local_xy1[0]][local_xy1[1]];
                }
                else PerImg_ip[i][j] = &BlackColor;
            }
            else PerImg_ip[i][j] = &BlackColor;
        }
    }
}
