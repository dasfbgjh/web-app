#pragma once

#include "cpp/QuickJS.hpp"
#include "common/Utils.h"

namespace webviewjs {

/**
 * 运行 JS 脚本。
 * 脚本自行决定是否调用 App.run() 启动事件循环，脚本执行完毕后直接退出，未启动事件循环时程序就此结束。
 * @param source 要执行的 JS 代码。
 * @param scriptPath 脚本文件路径。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 脚本执行结果。
 */
json::value exec( std::string source, std::string scriptPath, int argc = 0, char **argv = nullptr );

} // namespace webviewjs