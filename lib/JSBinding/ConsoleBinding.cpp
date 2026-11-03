#include "ConsoleBinding.h"

#include "common/Logger.hpp"

namespace webviewjs {
namespace detail {

namespace {

/** JS_PrintValue 的输出回调，把格式化结果追加到字符串 */
void writeToString( void *opaque, const char *buf, size_t len ) {
    static_cast<std::string *>( opaque )->append( buf, len );
}

/**
 * 把 console 的多个参数拼成一行，行为对齐标准 js_print：
 * 字符串按原文输出，其余值交给 JS_PrintValue 做结构化打印（对象、数组、Error 等）。
 */
std::string formatArgs( JSContext *ctx, int argc, JSValueConst *argv ) {
    std::string text;
    for ( int i = 0; i < argc; ++i ) {
        if ( i > 0 )
            text += " ";

        if ( JS_IsString( argv[i] ) ) {
            const char *str = JS_ToCString( ctx, argv[i] );
            if ( str ) {
                text += str;
                JS_FreeCString( ctx, str );
            }
            continue;
        }
        JS_PrintValue( ctx, writeToString, &text, argv[i], nullptr );
    }
    return text;
}

JSValue consoleDebug( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    LOG_DEBUG << formatArgs( ctx, argc, argv );
    return JS_UNDEFINED;
}

JSValue consoleLog( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    LOG_LOG << formatArgs( ctx, argc, argv );
    return JS_UNDEFINED;
}

JSValue consoleInfo( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    LOG_INFO << formatArgs( ctx, argc, argv );
    return JS_UNDEFINED;
}

JSValue consoleWarn( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    LOG_WARN << formatArgs( ctx, argc, argv );
    return JS_UNDEFINED;
}

JSValue consoleError( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    LOG_ERROR << formatArgs( ctx, argc, argv );
    return JS_UNDEFINED;
}

} // namespace

MethodTable buildConsoleMethods() {
    MethodTable t;
    t.add( "debug", 1, consoleDebug )
        .add( "log", 1, consoleLog )
        .add( "info", 1, consoleInfo )
        .add( "warn", 1, consoleWarn )
        .add( "error", 1, consoleError )
        .add( "print", 1, consoleLog );
    return t;
}

} // namespace detail
} // namespace webviewjs