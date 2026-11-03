#include "AppBinding.h"

#include "webview/EventLoop.h"

namespace webviewjs {
namespace detail {

namespace {

/** JS 作业轮询间隔（毫秒），决定 Promise 回调的响应延迟 */
constexpr UINT kJobPumpInterval = 10;

/**
 * 启动事件循环并阻塞，直到 App.quit() 或最后一个窗口关闭。
 * 循环期间通过定时器持续执行 JS 待处理作业，否则 await 之后的代码不会继续。
 */
JSValue appRun( JSContext *ctx, JSValueConst, int, JSValueConst * ) {
    JSRuntime *rt = JS_GetRuntime( ctx );

    UINT_PTR timer = EventLoop::startTimer( kJobPumpInterval, [rt]( UINT_PTR ) {
        pumpJobs( rt );
        return true;
    } );

    EventLoop loop;
    int exitCode = loop.exec();

    EventLoop::stopTimer( timer );
    return JS_NewInt32( ctx, exitCode );
}

/** 退出所有事件循环，可在任意 JS 回调中调用 */
JSValue appQuit( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    EventLoop::quit( argc > 0 ? argInt32( ctx, argv[0] ) : 0 );
    return JS_UNDEFINED;
}

} // namespace

MethodTable buildAppMethods() {
    MethodTable t;
    t.add( "run", 0, appRun )
        .add( "quit", 1, appQuit );
    return t;
}

} // namespace detail
} // namespace webviewjs