#pragma once

#include "BindingCore.h"

namespace webviewjs {
namespace detail {

/**
 * WebviewWidget 类的绑定描述。
 * 首次调用时生成类 ID 并构造方法表，之后返回同一实例。
 * 父类为 Widget，同时复用 WebView 的方法表。
 */
const ClassBinding &webviewWidgetClass();

} // namespace detail
} // namespace webviewjs