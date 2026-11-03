#include "TimerBinding.h"

#include "common/Logger.hpp"
#include "webview/EventLoop.h"

#include <mutex>
#include <unordered_map>
#include <vector>

namespace webviewjs {
namespace detail {

namespace {

/**
 * 一个 JS 定时器。
 * ctx/fn/args 都持有 JS 引用，条目被移除时必须显式释放。
 */
struct TimerEntry {
    JSContext *ctx = nullptr;
    JSValue fn = JS_UNDEFINED;
    std::vector<JSValue> args;
    bool repeat = false;
};

/** 按运行时分组的定时器登记表，键为 EventLoop 返回的系统定时器 ID */
std::unordered_map<JSRuntime *, std::unordered_map<UINT_PTR, TimerEntry>> g_timers;
std::mutex g_timersMutex;

void freeEntry( JSContext *ctx, TimerEntry &entry ) {
    JS_FreeValue( ctx, entry.fn );
    for ( JSValue &arg : entry.args )
        JS_FreeValue( ctx, arg );
    entry.fn = JS_UNDEFINED;
    entry.args.clear();
}

/**
 * 系统定时器回调，返回 true 表示继续（setInterval），false 表示停止（setTimeout）。
 *
 * 取出待执行的回调后立刻释放锁再调用 JS：JS 回调里可能再次调用 clearInterval，
 * 若持锁调用会死锁。一次性定时器在调用前就摘除，回调异常也不会导致重复触发。
 */
bool fireTimer( JSRuntime *rt, UINT_PTR id ) {
    TimerEntry pending;
    bool repeat = false;
    {
        std::lock_guard<std::mutex> lock( g_timersMutex );
        auto rtIt = g_timers.find( rt );
        if ( rtIt == g_timers.end() )
            return false;

        auto it = rtIt->second.find( id );
        if ( it == rtIt->second.end() )
            return false;

        repeat = it->second.repeat;
        pending.ctx = it->second.ctx;
        pending.fn = JS_DupValue( pending.ctx, it->second.fn );
        pending.args.reserve( it->second.args.size() );
        for ( JSValue arg : it->second.args )
            pending.args.push_back( JS_DupValue( pending.ctx, arg ) );

        if ( !repeat ) {
            freeEntry( pending.ctx, it->second );
            rtIt->second.erase( it );
        }
    }

    int argc = static_cast<int>( pending.args.size() );
    JSValue ret = JS_Call( pending.ctx, pending.fn, JS_UNDEFINED, argc, pending.args.data() );
    if ( JS_IsException( ret ) ) {
        JSValue ex = JS_GetException( pending.ctx );
        LOG_WARN << "JS 定时器回调异常: " << qjs::exceptionToString( pending.ctx, ex );
        JS_FreeValue( pending.ctx, ex );
    }
    JS_FreeValue( pending.ctx, ret );
    freeEntry( pending.ctx, pending );

    return repeat;
}

JSValue createTimer( JSContext *ctx, int argc, JSValueConst *argv, bool repeat ) {
    if ( argc < 1 || !JS_IsFunction( ctx, argv[0] ) )
        return JS_ThrowTypeError( ctx, "定时器回调必须是函数" );

    // WM_TIMER 精度约为 10ms，延迟按毫秒传入，0 交给系统最小间隔
    int delay = argc > 1 ? argInt32( ctx, argv[1], 0 ) : 0;
    UINT interval = delay > 0 ? static_cast<UINT>( delay ) : 1;

    TimerEntry entry;
    entry.ctx = ctx;
    entry.repeat = repeat;
    entry.fn = JS_DupValue( ctx, argv[0] );
    for ( int i = 2; i < argc; ++i )
        entry.args.push_back( JS_DupValue( ctx, argv[i] ) );

    JSRuntime *rt = JS_GetRuntime( ctx );
    UINT_PTR id = EventLoop::startTimer( interval, [rt]( UINT_PTR timerId ) {
        return fireTimer( rt, timerId );
    } );
    if ( !id ) {
        freeEntry( ctx, entry );
        return JS_ThrowInternalError( ctx, "创建系统定时器失败" );
    }

    {
        // 登记必须在 startTimer 之后：回调需要 ID 才能反查条目，
        // 而本线程正在执行 JS，消息循环此刻不会派发 WM_TIMER，不存在竞态。
        std::lock_guard<std::mutex> lock( g_timersMutex );
        g_timers[rt][id] = std::move( entry );
    }

    return JS_NewInt32( ctx, static_cast<int32_t>( id ) );
}

JSValue jsSetTimeout( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    return createTimer( ctx, argc, argv, false );
}

JSValue jsSetInterval( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    return createTimer( ctx, argc, argv, true );
}

JSValue jsClearTimer( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    if ( argc < 1 )
        return JS_UNDEFINED;

    UINT_PTR id = static_cast<UINT_PTR>( argInt32( ctx, argv[0], 0 ) );
    if ( !id )
        return JS_UNDEFINED;

    TimerEntry removed;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock( g_timersMutex );
        auto rtIt = g_timers.find( JS_GetRuntime( ctx ) );
        if ( rtIt != g_timers.end() ) {
            auto it = rtIt->second.find( id );
            if ( it != rtIt->second.end() ) {
                removed = std::move( it->second );
                rtIt->second.erase( it );
                found = true;
            }
        }
    }

    if ( found ) {
        EventLoop::stopTimer( id );
        freeEntry( ctx, removed );
    }
    return JS_UNDEFINED;
}

} // namespace

MethodTable buildTimerFunctions() {
    MethodTable t;
    t.add( "setTimeout", 2, jsSetTimeout )
        .add( "setInterval", 2, jsSetInterval )
        .add( "clearTimeout", 1, jsClearTimer )
        .add( "clearInterval", 1, jsClearTimer );
    return t;
}

void releaseTimers( JSContext *ctx ) {
    std::unordered_map<UINT_PTR, TimerEntry> timers;
    {
        std::lock_guard<std::mutex> lock( g_timersMutex );
        auto rtIt = g_timers.find( JS_GetRuntime( ctx ) );
        if ( rtIt == g_timers.end() )
            return;
        timers = std::move( rtIt->second );
        g_timers.erase( rtIt );
    }

    for ( auto &item : timers ) {
        EventLoop::stopTimer( item.first );
        freeEntry( ctx, item.second );
    }
}

} // namespace detail
} // namespace webviewjs