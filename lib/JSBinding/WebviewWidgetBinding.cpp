#include "WebviewWidgetBinding.h"

#include "WebViewBinding.h"
#include "WidgetBinding.h"

namespace webviewjs {
namespace detail {

namespace {

void startWebviewWidgetCreate( JSContext *ctx, WebviewWidget *ww, const CallbackStorePtr &store, int argc,
                               JSValueConst *argv ) {
    if ( argc > 0 && JS_IsObject( argv[0] ) ) {
        WebviewWidget::Config cfg = WebviewWidget::configFromJson( jsToJson( ctx, argv[0] ) );
        if ( argc > 1 )
            setCallback( ctx, store, "created", argv[1] );
        ww->create( cfg, [ctx, store]( bool success, WebviewWidget * ) {
            forwardCreated( ctx, store, success );
        } );
    }
}

JSValue webviewWidgetCreate( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebviewWidget *ww = webviewWidgetOf( h );
    if ( !ww )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWidget" );
    startWebviewWidgetCreate( ctx, ww, h->callbacks, argc, argv );
    return JS_NewBool( ctx, true );
}

JSValue webviewWidgetClose( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebviewWidget *ww = webviewWidgetOf( anyHandle( ctx, thisVal ) );
    if ( !ww )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWidget" );
    ww->close();
    return JS_UNDEFINED;
}

JSValue webviewWidgetSetAutoWebviewTransparent( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebviewWidget *ww = webviewWidgetOf( anyHandle( ctx, thisVal ) );
    if ( !ww )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWidget" );
    ww->setAutoWebviewTransparent( argc > 0 ? argBool( ctx, argv[0], true ) : true );
    return JS_UNDEFINED;
}

JSValue webviewWidgetSetWebViewMargin( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebviewWidget *ww = webviewWidgetOf( anyHandle( ctx, thisVal ) );
    if ( !ww )
        return JS_ThrowTypeError( ctx, "该对象不是 WebviewWidget" );
    ww->setWebViewMargin( argc > 0 ? argInt32( ctx, argv[0] ) : 0,
                          argc > 1 ? argInt32( ctx, argv[1] ) : 0,
                          argc > 2 ? argInt32( ctx, argv[2] ) : 0,
                          argc > 3 ? argInt32( ctx, argv[3] ) : 0 );
    return JS_UNDEFINED;
}

} // namespace

JSValue webviewWidgetConstructor( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    WebviewWidget *ww = new WebviewWidget();
    Handle *h = newHandle( webviewWidgetClass(), ww );
    JSValue obj = wrapHandle( ctx, webviewWidgetClass(), h );
    if ( JS_IsException( obj ) )
        return obj;
    startWebviewWidgetCreate( ctx, ww, h->callbacks, argc, argv );
    return obj;
}

MethodTable buildWebviewWidgetMethods() {
    MethodTable t = *webViewClass().methods;
    t.add( "create", 2, webviewWidgetCreate )
        .add( "close", 0, webviewWidgetClose )
        .add( "setAutoWebviewTransparent", 1, webviewWidgetSetAutoWebviewTransparent )
        .add( "setWebViewMargin", 4, webviewWidgetSetWebViewMargin );
    return t;
}

namespace {

/**
 * 方法表必须与运行时同寿命：QuickJS 的属性表项惰性初始化时会缓存数组元素地址，
 * 因此这里用静态局部变量持有，不能返回临时对象。
 */
MethodTable &webviewWidgetMethods() {
    static MethodTable table = buildWebviewWidgetMethods();
    return table;
}

} // namespace

const ClassBinding &webviewWidgetClass() {
    static const ClassBinding binding = [] {
        ClassBinding b;
        JS_NewClassID( &b.id );
        b.name = "WebviewWidget";
        b.ctor = webviewWidgetConstructor;
        b.methods = &webviewWidgetMethods();
        b.parent = &widgetClass();
        b.asWidget = []( void *p ) { return static_cast<Widget *>( static_cast<WebviewWidget *>( p ) ); };
        b.asWebView = []( void *p ) { return static_cast<WebView *>( static_cast<WebviewWidget *>( p ) ); };
        b.asWebviewWidget = []( void *p ) { return static_cast<WebviewWidget *>( p ); };
        b.destroy = []( void *p ) { delete static_cast<WebviewWidget *>( p ); };
        return b;
    }();
    return binding;
}

} // namespace detail
} // namespace webviewjs