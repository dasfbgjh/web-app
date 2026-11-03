#pragma once

#include "BindingCore.h"

namespace webviewjs {
namespace detail {

/**
 * console 对象的方法表：debug / log / info / warn / error。
 * 全部转发到项目现有的 Logger，从而与原生日志统一格式、统一过滤级别、统一落盘。
 */
MethodTable buildConsoleMethods();

} // namespace detail
} // namespace webviewjs