#include "BindingCore.h"

#include "common/Logger.hpp"

#include <algorithm>
#include <functional>
#include <unordered_set>

namespace webviewjs {
namespace detail {

namespace {

/**
 * 本模块已注册的类 ID 集合。
 * 子类原型链会复用父类方法，方法内只能拿到 opaque 句柄与 class_id，
 * 因此需要一个集合来判定该 ID 是否属于本模块，而不是逐个比较固定的几个 ID。
 */
std::unordered_set<JSClassID> g_classIds;

void registerClassDef( JSRuntime *rt, JSClassID classId, const char *name ) {
    JSClassDef def = {};
    def.class_name = name;
    def.finalizer = handleFinalizer;
    JS_NewClass( rt, classId, &def );
    g_classIds.insert( classId );
}

void registerClass( JSContext *ctx, JSClassID classId, const char *name, JSCFunction *ctorFn,
                    const MethodTable &methods, JSValue parentProto, const MethodTable *statics ) {
    JSValue proto = JS_NewObject( ctx );
    JS_SetPropertyFunctionList( ctx, proto, methods.data(), methods.size() );
    if ( !JS_IsUndefined( parentProto ) )
        JS_SetPrototype( ctx, proto, parentProto );

    JSValue ctor = JS_NewCFunction2( ctx, ctorFn, name, 1, JS_CFUNC_constructor, 0 );
    if ( statics )
        JS_SetPropertyFunctionList( ctx, ctor, statics->data(), statics->size() );
    JS_SetConstructor( ctx, ctor, proto );

    JS_SetClassProto( ctx, classId, proto );

    JSValue global = JS_GetGlobalObject( ctx );
    JS_SetPropertyStr( ctx, global, name, ctor );
    JS_FreeValue( ctx, global );
}

} // namespace

std::unordered_map<JSRuntime *, std::vector<Handle *>> g_handles;

bool isOurClass( JSClassID id ) {
    return g_classIds.count( id ) != 0;
}

void registerClasses( JSContext *ctx, JSRuntime *rt, const std::vector<const ClassBinding *> &classes ) {

    std::function<void( const ClassBinding * )> registerOne = [&]( const ClassBinding *binding ) {
        if ( !binding || binding->id == JS_INVALID_CLASS_ID )
            return;
        if ( JS_IsRegisteredClass( rt, binding->id ) )
            return;

        registerOne( binding->parent );

        registerClassDef( rt, binding->id, binding->name );

        JSValue parentProto = JS_UNDEFINED;
        if ( binding->parent )
            parentProto = JS_GetClassProto( ctx, binding->parent->id );

        registerClass( ctx, binding->id, binding->name, binding->ctor, *binding->methods, parentProto,
                       binding->statics );

        if ( binding->parent )
            JS_FreeValue( ctx, parentProto );
    };

    for ( const ClassBinding *binding : classes )
        registerOne( binding );
}

Handle *anyHandle( JSContext *ctx, JSValueConst thisVal ) {
    JSClassID id = JS_INVALID_CLASS_ID;
    Handle *h = static_cast<Handle *>( JS_GetAnyOpaque( thisVal, &id ) );
    if ( !h || !isOurClass( id ) ) {
        JS_ThrowTypeError( ctx, "非法的接收者对象" );
        return nullptr;
    }
    return h;
}

Widget *widgetOf( Handle *h ) {
    if ( !h || !h->binding || !h->binding->asWidget )
        return nullptr;
    return h->binding->asWidget( h->object );
}

WebView *webviewOf( Handle *h ) {
    if ( !h || !h->binding || !h->binding->asWebView )
        return nullptr;
    return h->binding->asWebView( h->object );
}

WebviewWidget *webviewWidgetOf( Handle *h ) {
    if ( !h || !h->binding || !h->binding->asWebviewWidget )
        return nullptr;
    return h->binding->asWebviewWidget( h->object );
}

WebviewWindow *webviewWindowOf( Handle *h ) {
    if ( !h || !h->binding || !h->binding->asWebviewWindow )
        return nullptr;
    return h->binding->asWebviewWindow( h->object );
}

Widget *widgetOf( JSContext *ctx, JSValueConst thisVal ) {
    Widget *w = widgetOf( anyHandle( ctx, thisVal ) );
    if ( !w ) {
        JS_ThrowTypeError( ctx, "该对象不支持窗口操作" );
        return nullptr;
    }
    return w;
}

WebView *webviewOf( JSContext *ctx, JSValueConst thisVal ) {
    WebView *w = webviewOf( anyHandle( ctx, thisVal ) );
    if ( !w ) {
        JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );
        return nullptr;
    }
    return w;
}

std::string argString( JSContext *ctx, JSValueConst val, const std::string &def ) {
    size_t len = 0;
    const char *text = JS_ToCStringLen( ctx, &len, val );
    if ( !text )
        return def;
    std::string out( text, len );
    JS_FreeCString( ctx, text );
    return out;
}

int32_t argInt32( JSContext *ctx, JSValueConst val, int32_t def ) {
    int32_t out = def;
    if ( JS_ToInt32( ctx, &out, val ) ) {
        JS_FreeValue( ctx, JS_GetException( ctx ) );
        return def;
    }
    return out;
}

int64_t argInt64( JSContext *ctx, JSValueConst val, int64_t def ) {
    int64_t out = def;
    if ( JS_ToInt64( ctx, &out, val ) ) {
        JS_FreeValue( ctx, JS_GetException( ctx ) );
        return def;
    }
    return out;
}

double argDouble( JSContext *ctx, JSValueConst val, double def ) {
    double out = def;
    if ( JS_ToFloat64( ctx, &out, val ) ) {
        JS_FreeValue( ctx, JS_GetException( ctx ) );
        return def;
    }
    return out;
}

bool argBool( JSContext *ctx, JSValueConst val, bool def ) {
    int out = JS_ToBool( ctx, val );
    if ( out < 0 ) {
        JS_FreeValue( ctx, JS_GetException( ctx ) );
        return def;
    }
    return out != 0;
}

json::value jsToJson( JSContext *ctx, JSValueConst val ) {
    JSValue text = JS_JSONStringify( ctx, val, JS_UNDEFINED, JS_UNDEFINED );
    if ( JS_IsException( text ) ) {
        JS_FreeValue( ctx, JS_GetException( ctx ) );
        return json::value();
    }

    size_t len = 0;
    const char *str = JS_ToCStringLen( ctx, &len, text );
    json::value result;
    if ( str ) {
        try {
            result = json::parse( std::string( str, len ) );
        } catch ( ... ) {
            result = json::value();
        }
        JS_FreeCString( ctx, str );
    }
    JS_FreeValue( ctx, text );
    return result;
}

JSValue jsonToJS( JSContext *ctx, const json::value &val ) {
    std::string text = json::serialize( val );
    JSValue out = JS_ParseJSON( ctx, text.c_str(), text.size(), "<json>" );
    if ( JS_IsException( out ) ) {
        JS_FreeValue( ctx, JS_GetException( ctx ) );
        return JS_UNDEFINED;
    }
    return out;
}

JSValue newString( JSContext *ctx, const std::string &str ) {
    return JS_NewStringLen( ctx, str.c_str(), str.size() );
}

JSValue newStringArray( JSContext *ctx, const std::vector<std::string> &items ) {
    JSValue arr = JS_NewArray( ctx );
    for ( size_t i = 0; i < items.size(); ++i )
        JS_SetPropertyUint32( ctx, arr, static_cast<uint32_t>( i ), newString( ctx, items[i] ) );
    return arr;
}

json::object mapToJson( const std::map<std::string, std::string> &map ) {
    json::object obj;
    for ( const auto &kv : map )
        obj[kv.first] = kv.second;
    return obj;
}

void setCallback( JSContext *ctx, const CallbackStorePtr &store, const std::string &key, JSValueConst fn ) {
    if ( !store )
        return;
    auto it = store->map.find( key );
    if ( it != store->map.end() ) {
        JS_FreeValue( ctx, it->second );
        store->map.erase( it );
    }
    if ( JS_IsFunction( ctx, fn ) )
        store->map.emplace( key, JS_DupValue( ctx, fn ) );
}

void removeCallback( JSContext *ctx, const CallbackStorePtr &store, const std::string &key ) {
    if ( !store )
        return;
    auto it = store->map.find( key );
    if ( it == store->map.end() )
        return;
    JS_FreeValue( ctx, it->second );
    store->map.erase( it );
}

void invokeCallback( JSContext *ctx, const CallbackStorePtr &store, const std::string &key, int argc,
                     JSValueConst *argv ) {
    if ( !store )
        return;
    auto it = store->map.find( key );
    if ( it == store->map.end() )
        return;

    JSValue ret = JS_Call( ctx, it->second, JS_UNDEFINED, argc, argv );
    if ( JS_IsException( ret ) ) {
        JSValue ex = JS_GetException( ctx );
        LOG_WARN << "JS 回调异常: " << qjs::exceptionToString( ctx, ex );
        JS_FreeValue( ctx, ex );
    }
    JS_FreeValue( ctx, ret );
}

Handle *newHandle( const ClassBinding &binding, void *object, bool owned ) {
    Handle *h = new Handle();
    h->binding = &binding;
    h->object = object;
    h->owned = owned;
    return h;
}

JSValue wrapHandle( JSContext *ctx, const ClassBinding &binding, Handle *h ) {
    JSValue obj = JS_NewObjectClass( ctx, static_cast<int>( binding.id ) );
    if ( JS_IsException( obj ) ) {
        if ( h->owned && h->binding && h->binding->destroy )
            h->binding->destroy( h->object );
        delete h;
        return obj;
    }
    JS_SetOpaque( obj, h );
    g_handles[JS_GetRuntime( ctx )].push_back( h );
    return obj;
}

JSValue wrapBorrowed( JSContext *ctx, const ClassBinding &binding, void *ptr ) {
    return wrapHandle( ctx, binding, newHandle( binding, ptr, false ) );
}

void handleFinalizer( JSRuntime *rt, JSValue val ) {
    JSClassID id = JS_INVALID_CLASS_ID;
    Handle *h = static_cast<Handle *>( JS_GetAnyOpaque( val, &id ) );
    if ( !h || !isOurClass( id ) )
        return;

    if ( h->owned && h->binding && h->binding->destroy )
        h->binding->destroy( h->object );

    if ( h->callbacks ) {
        for ( auto &kv : h->callbacks->map )
            JS_FreeValueRT( rt, kv.second );
        h->callbacks->map.clear();
    }

    auto git = g_handles.find( rt );
    if ( git != g_handles.end() ) {
        auto &list = git->second;
        auto it = std::find( list.begin(), list.end(), h );
        if ( it != list.end() )
            list.erase( it );
    }

    delete h;
}

void forwardCreated( JSContext *ctx, const CallbackStorePtr &store, bool success ) {
    JSValue arg = JS_NewBool( ctx, success );
    invokeCallback( ctx, store, "created", 1, &arg );
    JS_FreeValue( ctx, arg );
}

void registerObject( JSContext *ctx, const char *name, const MethodTable &methods ) {
    JSValue obj = JS_NewObject( ctx );
    JS_SetPropertyFunctionList( ctx, obj, methods.data(), methods.size() );

    JSValue global = JS_GetGlobalObject( ctx );
    JS_SetPropertyStr( ctx, global, name, obj );
    JS_FreeValue( ctx, global );
}

void registerGlobals( JSContext *ctx, const MethodTable &functions ) {
    JSValue global = JS_GetGlobalObject( ctx );
    JS_SetPropertyFunctionList( ctx, global, functions.data(), functions.size() );
    JS_FreeValue( ctx, global );
}

void pumpJobs( JSRuntime *rt ) {
    if ( !rt )
        return;

    while ( JS_IsJobPending( rt ) ) {
        JSContext *jobCtx = nullptr;
        int ret = JS_ExecutePendingJob( rt, &jobCtx );
        if ( ret < 0 ) {
            if ( jobCtx ) {
                JSValue ex = JS_GetException( jobCtx );
                LOG_WARN << "JS 异步任务异常: " << qjs::exceptionToString( jobCtx, ex );
                JS_FreeValue( jobCtx, ex );
            }
        } else if ( ret == 0 ) {
            break;
        }
    }
}

void releaseHandleCallbacks( JSContext *ctx ) {
    auto git = g_handles.find( JS_GetRuntime( ctx ) );
    if ( git == g_handles.end() )
        return;

    std::vector<JSValue> pending;
    for ( Handle *h : git->second ) {
        if ( h->released || !h->callbacks )
            continue;
        h->released = true;
        for ( auto &kv : h->callbacks->map )
            pending.push_back( kv.second );
        h->callbacks->map.clear();
    }

    for ( JSValue v : pending )
        JS_FreeValue( ctx, v );
}

} // namespace detail
} // namespace webviewjs