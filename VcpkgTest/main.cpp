#include "logger.h"
#include <iostream>

int main(int argc, char *argv[]) {
    // 1. 初始化日志（全局一次）
    Logger::Init("VcpkgTest");


    // 2. 开始使用日志
    LOG(INFO) << "Application started";
    LOG(WARNING) << "This is a warning message";
    LOG(ERROR) << "This is an error message";

    // 条件日志示例
    int count = 5;
    LOG_IF(INFO, count > 10) << "Count is too large: " << count;
    LOG(ERROR) << "Configuration file not found:" << count;

    // // 每 N 次输出一次
    // for (int i = 0; i < 100; i++) {
    //     LOG_EVERY_N(INFO, 10) << "Processed " << i << " items";
    // }

    // 第一次输出
    LOG_FIRST_N(INFO, 1) << "This message appears only once";

    // 3. 程序结束前关闭
    Logger::Shutdown();

    return 0;
}
