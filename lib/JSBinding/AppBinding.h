#pragma once

#include "BindingCore.h"

namespace webviewjs {
namespace detail {

/**
 * App 方法表。
 * App 是全局单例对象，不提供构造函数，因此只暴露方法表供 JSBindings 统一注册。
 */
MethodTable buildAppMethods();

} // namespace detail
} // namespace webviewjs