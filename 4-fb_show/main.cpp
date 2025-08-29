#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// 只需要OpenCV的核心头文件
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

int main(int argc, char *argv[]) {
    // 目标分辨率
    int width    = 720;
    int height   = 720; // 修改为720以匹配你的需求

    // --- 初始化OpenCV摄像头捕获 ---
    cv::VideoCapture cap;
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);

    // 打开ID为0的摄像头。在嵌入式设备上通常就是MIPI CSI摄像头
    cap.open(0);
    if (!cap.isOpened()) {
        printf("ERROR: Cannot open camera!\n");
        return -1;
    }

    printf("Camera opened successfully. Starting capture...\n");

    // 用于显示的Mat对象
    cv::Mat frame;

    // --- 主循环：不断捕获并显示画面 ---
    while(1)
    {
        // 从摄像头读取一帧画面
        cap >> frame;

        // 检查画面是否为空（比如摄像头断开）
        if (frame.empty()) {
            printf("ERROR: Captured empty frame!\n");
            break;
        }

        // *** 关键步骤 ***
        // 使用 "fb" 作为窗口名，opencv-mobile会将其直接渲染到 /dev/fb0
        cv::imshow("fb", frame);

        // waitKey对于imshow的刷新是必要的，即使延时很短
        // 它也提供了按键退出的机制，例如按下'q'键退出
        char key = (char)cv::waitKey(10);
        if (key == 'q' || key == 27) { // 27是ESC键
            break;
        }
    }

    // 释放摄像头资源
    cap.release();
    // 销毁所有OpenCV窗口（虽然这里只有一个隐式的framebuffer）
    // cv::destroyAllWindows();

    printf("Program finished.\n");

    return 0;
}