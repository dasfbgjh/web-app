#pragma once

#include "BindingCore.h"

namespace webviewjs {
namespace detail {

/**
 * setTimeout / setInterval / clearTimeout / clearInterval 的函数表。
 * 这些是全局函数而非对象方法，由 registerGlobals 直接挂到全局对象上。
 */
MethodTable buildTimerFunctions();

/**
 * 释放该运行时上所有尚未触发的定时器，并停止对应的系统定时器。
 * 定时器持有 JS 回调引用，必须在 JS_FreeRuntime 之前断开。
 */
void releaseTimers( JSContext *ctx );

} // namespace detail
} // namespace webviewjs