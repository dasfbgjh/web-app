#include "WebviewWindowBinding.h"

#include "WebviewWidgetBinding.h"
#include "WidgetBinding.h"
#include "common/Logger.hpp"

namespace webviewjs {
namespace detail {

namespace {

void startWebviewWindowCreate( JSContext *ctx, WebviewWindow *win, const CallbackStorePtr &store, int argc,
                               JSValueConst *argv ) {
    if ( argc > 0 && JS_IsObject( argv[0] ) ) {
        WebviewWindow::Config cfg = WebviewWindow::configFromJson( jsToJson( ctx, argv[0] ) );
        if ( argc > 1 )
            setCallback( ctx, store, "created", argv[1] );
        win->create( cfg, [ctx, store]( bool success, WebviewWindow * ) {
            forwardCreated( ctx, store, success );
        } );
    }
}

JSValue webviewWindowCreate( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebviewWindow *win = webviewWindowOf( h );
    if ( !win )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWindow" );
    startWebviewWindowCreate( ctx, win, h->callbacks, argc, argv );
    return JS_NewBool( ctx, true );
}

JSValue webviewWindowClose( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebviewWindow *win = webviewWindowOf( anyHandle( ctx, thisVal ) );
    if ( !win )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWindow" );
    win->close();
    return JS_UNDEFINED;
}

JSValue webviewWindowGetMainWebview( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebviewWindow *win = webviewWindowOf( anyHandle( ctx, thisVal ) );
    if ( !win )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWindow" );
    WebviewWidget *main = win->getMainWebview();
    if ( !main )
        return JS_NULL;
    return wrapBorrowed( ctx, webviewWidgetClass(), main );
}

JSValue webviewWindowGetSubWebview( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebviewWindow *win = webviewWindowOf( anyHandle( ctx, thisVal ) );
    if ( !win )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWindow" );
    WebviewWidget *sub = win->getSubWebview( argc > 0 ? argString( ctx, argv[0] ) : std::string() );
    if ( !sub )
        return JS_NULL;
    return wrapBorrowed( ctx, webviewWidgetClass(), sub );
}

JSValue webviewWindowAddSubWebview( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebviewWindow *win = webviewWindowOf( anyHandle( ctx, thisVal ) );
    if ( !win )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWindow" );
    if ( argc < 1 )
        return JS_ThrowTypeError( ctx, "addSubWebview 需要名称" );

    std::string name = argString( ctx, argv[0] );
    WebviewWidget::Config cfg = argc > 1 ? WebviewWidget::configFromJson( jsToJson( ctx, argv[1] ) )
                                         : WebviewWidget::Config{};
    return JS_NewBool( ctx, win->addSubWebview( name, cfg ) );
}

JSValue webviewWindowRemoveSubWebview( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebviewWindow *win = webviewWindowOf( anyHandle( ctx, thisVal ) );
    if ( !win )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWindow" );
    return JS_NewBool( ctx, win->removeSubWebview( argc > 0 ? argString( ctx, argv[0] ) : std::string() ) );
}

/**
 * JS 回调签名：(config, subWebview) => 修改后的 config | 无返回值
 * config 是可修改的普通对象；返回对象时作为最终配置，但窗口句柄等宿主强制字段不被覆盖。
 */
JSValue webviewWindowSetSubWebviewCreateCallback( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebviewWindow *win = webviewWindowOf( h );
    if ( !win )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWindow" );

    const CallbackStorePtr store = h->callbacks;
    if ( argc > 0 && JS_IsFunction( ctx, argv[0] ) ) {
        setCallback( ctx, store, "subWebviewCreate", argv[0] );
        win->setSubWebviewCreateCallback( [ctx, store]( WebviewWidget::Config &config, WebviewWidget *sub ) {
            auto it = store->map.find( "subWebviewCreate" );
            if ( it == store->map.end() )
                return;

            JSValue args[2] = { jsonToJS( ctx, WebviewWidget::configToJson( config ) ),
                                wrapBorrowed( ctx, webviewWidgetClass(), sub ) };
            JSValue ret = JS_Call( ctx, it->second, JS_UNDEFINED, 2, args );
            JS_FreeValue( ctx, args[0] );
            JS_FreeValue( ctx, args[1] );

            if ( JS_IsException( ret ) ) {
                JSValue ex = JS_GetException( ctx );
                LOG_WARN << "子 WebView 创建回调异常: " << qjs::exceptionToString( ctx, ex );
                JS_FreeValue( ctx, ex );
                return;
            }
            if ( JS_IsObject( ret ) ) {
                WebviewWidget::Config applied = WebviewWidget::configFromJson( jsToJson( ctx, ret ) );
                applied.window.parent = config.window.parent;
                applied.window.noBorder = config.window.noBorder;
                applied.targetWindow = config.targetWindow;
                config = applied;
            }
            JS_FreeValue( ctx, ret );
        } );
    } else {
        removeCallback( ctx, store, "subWebviewCreate" );
        win->setSubWebviewCreateCallback( nullptr );
    }
    return JS_UNDEFINED;
}

/**
 * JS 回调签名：(success, subWebview) => void
 */
JSValue webviewWindowSetSubWebviewCreateCompleteCallback( JSContext *ctx, JSValueConst thisVal, int argc,
                                                         JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebviewWindow *win = webviewWindowOf( h );
    if ( !win )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWindow" );

    const CallbackStorePtr store = h->callbacks;
    if ( argc > 0 && JS_IsFunction( ctx, argv[0] ) ) {
        setCallback( ctx, store, "subWebviewCreateComplete", argv[0] );
        win->setSubWebviewCreateCompleteCallback( [ctx, store]( bool success, WebviewWidget *sub ) {
            JSValue args[2] = { JS_NewBool( ctx, success ), wrapBorrowed( ctx, webviewWidgetClass(), sub ) };
            invokeCallback( ctx, store, "subWebviewCreateComplete", 2, args );
            JS_FreeValue( ctx, args[0] );
            JS_FreeValue( ctx, args[1] );
        } );
    } else {
        removeCallback( ctx, store, "subWebviewCreateComplete" );
        win->setSubWebviewCreateCompleteCallback( nullptr );
    }
    return JS_UNDEFINED;
}

} // namespace

JSValue webviewWindowConstructor( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    WebviewWindow *win = new WebviewWindow();
    Handle *h = newHandle( webviewWindowClass(), win );
    JSValue obj = wrapHandle( ctx, webviewWindowClass(), h );
    if ( JS_IsException( obj ) )
        return obj;
    startWebviewWindowCreate( ctx, win, h->callbacks, argc, argv );
    return obj;
}

MethodTable buildWebviewWindowMethods() {
    MethodTable t;
    t.add( "create", 2, webviewWindowCreate )
        .add( "close", 0, webviewWindowClose )
        .add( "getMainWebview", 0, webviewWindowGetMainWebview )
        .add( "getSubWebview", 1, webviewWindowGetSubWebview )
        .add( "addSubWebview", 2, webviewWindowAddSubWebview )
        .add( "removeSubWebview", 1, webviewWindowRemoveSubWebview )
        .add( "setSubWebviewCreateCallback", 1, webviewWindowSetSubWebviewCreateCallback )
        .add( "setSubWebviewCreateCompleteCallback", 1, webviewWindowSetSubWebviewCreateCompleteCallback );
    return t;
}

namespace {

/**
 * 方法表必须与运行时同寿命：QuickJS 的属性表项惰性初始化时会缓存数组元素地址，
 * 因此这里用静态局部变量持有，不能返回临时对象。
 */
MethodTable &webviewWindowMethods() {
    static MethodTable table = buildWebviewWindowMethods();
    return table;
}

} // namespace

const ClassBinding &webviewWindowClass() {
    static const ClassBinding binding = [] {
        ClassBinding b;
        JS_NewClassID( &b.id );
        b.name = "WebviewWindow";
        b.ctor = webviewWindowConstructor;
        b.methods = &webviewWindowMethods();
        b.parent = &widgetClass();
        b.asWidget = []( void *p ) { return static_cast<Widget *>( static_cast<WebviewWindow *>( p ) ); };
        b.asWebviewWindow = []( void *p ) { return static_cast<WebviewWindow *>( p ); };
        b.destroy = []( void *p ) { delete static_cast<WebviewWindow *>( p ); };
        return b;
    }();
    return binding;
}

} // namespace detail
} // namespace webviewjs