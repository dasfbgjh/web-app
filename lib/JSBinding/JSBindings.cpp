#include "JSBindings.h"

#include "AppBinding.h"
#include "BindingCore.h"
#include "ConsoleBinding.h"
#include "TimerBinding.h"
#include "WebViewBinding.h"
#include "WebviewWidgetBinding.h"
#include "WebviewWindowBinding.h"
#include "WidgetBinding.h"

#include "common/Logger.hpp"

namespace webviewjs {

namespace {

using namespace detail;

/**
 * 全局对象与全局函数的方法表。
 * QuickJS 的属性表项采用惰性初始化，内部保存的是指向 JSCFunctionListEntry
 * 数组元素的裸指针，因此这些表必须与运行时同寿命，不能使用局部变量。
 * 原生类的方法表由各自的绑定文件持有，这里只保留非类对象的部分。
 */
struct GlobalTables {
    MethodTable app;
    MethodTable console;
    MethodTable timer;
};

const GlobalTables &globalTables() {
    static const GlobalTables tables = {
        buildAppMethods(),
        buildConsoleMethods(),
        buildTimerFunctions(),
    };
    return tables;
}

} // namespace

/**
 * 向引擎注入全部绑定：App、console、定时器，以及各原生类。
 * 需在 Engine::addStdHelpers 之后调用，否则标准 helpers 会把这里的 console 覆盖回写 stdout 的版本。
 */
void install( qjs::Engine &engine ) {
    JSContext *ctx = engine.context();
    JSRuntime *rt = engine.runtime();
    if ( !ctx || !rt )
        return;

    const GlobalTables &tables = globalTables();

    // 全局对象与全局函数先于类注册：install 可能被重复调用，
    // 类重复注册由 registerClasses 跳过，但 console 与定时器应当始终可用。
    registerObject( ctx, "App", tables.app );
    registerObject( ctx, "console", tables.console );
    registerGlobals( ctx, tables.timer );

    // 每个类的名称、ID、方法与父类都由各自的绑定文件描述，
    // 这里只负责收集；父类由 registerClasses 沿 parent 链自动先行注册。
    registerClasses( ctx, rt,
                     {
                         &widgetClass(),
                         &webViewClass(),
                         &webviewWidgetClass(),
                         &webviewWindowClass(),
                     } );
}

/**
 * 释放绑定层持有的 JS 引用并执行一次垃圾回收。
 * 原生对象与 JS 回调互相引用形成引用环，必须在引擎销毁前调用，
 * 否则运行时释放时会因对象未回收而断言失败。
 */
void shutdown( qjs::Engine &engine ) {
    JSContext *ctx = engine.context();
    JSRuntime *rt = engine.runtime();
    if ( !ctx || !rt )
        return;

    detail::releaseHandleCallbacks( ctx );
    detail::releaseTimers( ctx );
    JS_RunGC( rt );

    // 回收后该运行时已不应再有存活句柄，清空登记表避免残留悬挂指针。
    detail::g_handles.erase( rt );
}

void pumpJobs( qjs::Engine &engine ) {
    detail::pumpJobs( engine.runtime() );
}

json::value exec( std::string source, std::string scriptPath, int argc, char **argv ) {
    json::value result;
    while ( true ) {
        qjs::Engine engine;
        engine.addStdHelpers( argc, argv );
        webviewjs::install( engine );

        try {
            // 脚本自行决定是否调用 App.run() 启动事件循环，脚本执行完毕后直接退出，未启动事件循环时程序就此结束。
            auto res = engine.eval( source, scriptPath );
            result = jsToJson( engine.context(), res.raw() );
        } catch ( const qjs::Error &error ) {
            LOG_WARN << "JS 脚本执行失败: " << error.what();
            result = error.what();
        }
        webviewjs::shutdown( engine );

        if ( result.is_object() ) {
            json::object obj = result.as_object();
            json::value restart = obj["restart"];
            if ( restart.is_bool() && restart.as_bool() ) {
                LOG_INFO << "JS 引擎重启";
                continue;
            }
        }
        break;
    }
    return result;
}

} // namespace webviewjs