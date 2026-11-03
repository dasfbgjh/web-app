#pragma once

#include "BindingCore.h"

namespace webviewjs {
namespace detail {

/**
 * WebView 类的绑定描述。
 * 首次调用时生成类 ID 并构造方法表，之后返回同一实例。
 */
const ClassBinding &webViewClass();

} // namespace detail
} // namespace webviewjs