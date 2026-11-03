#include "WidgetBinding.h"

namespace webviewjs {
namespace detail {

namespace {

JSValue widgetCreate( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    Widget::Config cfg = Widget::configFromJson( argc > 0 ? jsToJson( ctx, argv[0] ) : json::value( json::object{} ) );
    return JS_NewBool( ctx, w->create( cfg ) );
}

JSValue widgetClose( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->close();
    return JS_UNDEFINED;
}

JSValue widgetBringToTop( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->bringToTop();
    return JS_UNDEFINED;
}

JSValue widgetBringToBottom( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->bringToBottom();
    return JS_UNDEFINED;
}

JSValue widgetSetClickThrough( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setClickThrough( argc > 0 ? argBool( ctx, argv[0] ) : true );
    return JS_UNDEFINED;
}

JSValue widgetIsClickThrough( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->isClickThrough() );
}

JSValue widgetGetHwnd( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewInt64( ctx, static_cast<int64_t>( reinterpret_cast<intptr_t>( w->getHwnd() ) ) );
}

JSValue widgetIsChild( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->isChild() );
}

JSValue widgetSetTitle( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setTitle( argc > 0 ? argString( ctx, argv[0] ) : std::string() );
    return JS_UNDEFINED;
}

JSValue widgetGetTitle( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return newString( ctx, w->getTitle() );
}

JSValue widgetSetSize( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    int width = argc > 0 ? argInt32( ctx, argv[0] ) : 0;
    int height = argc > 1 ? argInt32( ctx, argv[1] ) : 0;
    w->setSize( width, height, argc > 2 ? argBool( ctx, argv[2], true ) : true );
    return JS_UNDEFINED;
}

JSValue widgetGetSize( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    int width = 0, height = 0;
    w->getSize( width, height );
    JSValue obj = JS_NewObject( ctx );
    JS_SetPropertyStr( ctx, obj, "width", JS_NewInt32( ctx, width ) );
    JS_SetPropertyStr( ctx, obj, "height", JS_NewInt32( ctx, height ) );
    return obj;
}

JSValue widgetGetClientSize( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    int width = 0, height = 0;
    w->getClientSize( width, height );
    JSValue obj = JS_NewObject( ctx );
    JS_SetPropertyStr( ctx, obj, "width", JS_NewInt32( ctx, width ) );
    JS_SetPropertyStr( ctx, obj, "height", JS_NewInt32( ctx, height ) );
    return obj;
}

JSValue widgetSetPosition( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setPosition( argc > 0 ? argInt32( ctx, argv[0] ) : 0,
                    argc > 1 ? argInt32( ctx, argv[1] ) : 0 );
    return JS_UNDEFINED;
}

JSValue widgetGetPosition( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    int x = 0, y = 0;
    w->getPosition( x, y );
    JSValue obj = JS_NewObject( ctx );
    JS_SetPropertyStr( ctx, obj, "x", JS_NewInt32( ctx, x ) );
    JS_SetPropertyStr( ctx, obj, "y", JS_NewInt32( ctx, y ) );
    return obj;
}

JSValue widgetSetRect( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setRect( argc > 0 ? argInt32( ctx, argv[0] ) : 0,
                argc > 1 ? argInt32( ctx, argv[1] ) : 0,
                argc > 2 ? argInt32( ctx, argv[2] ) : 0,
                argc > 3 ? argInt32( ctx, argv[3] ) : 0 );
    return JS_UNDEFINED;
}

JSValue widgetCenter( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->center();
    return JS_UNDEFINED;
}

JSValue widgetSetTopMost( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setTopMost( argc > 0 ? argBool( ctx, argv[0] ) : true );
    return JS_UNDEFINED;
}

JSValue widgetSetFullscreen( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setFullscreen( argc > 0 ? argBool( ctx, argv[0] ) : true );
    return JS_UNDEFINED;
}

JSValue widgetIsFullscreen( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->isFullscreen() );
}

JSValue widgetMaximize( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->maximize();
    return JS_UNDEFINED;
}

JSValue widgetMinimize( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->minimize();
    return JS_UNDEFINED;
}

JSValue widgetRestore( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->restore();
    return JS_UNDEFINED;
}

JSValue widgetSetVisible( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setVisible( argc > 0 ? argBool( ctx, argv[0], true ) : true );
    return JS_UNDEFINED;
}

JSValue widgetIsVisible( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->isVisible() );
}

JSValue widgetIsMaximized( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->isMaximized() );
}

JSValue widgetIsMinimized( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->isMinimized() );
}

JSValue widgetSetMinSize( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setMinSize( argc > 0 ? argInt32( ctx, argv[0] ) : 0, argc > 1 ? argInt32( ctx, argv[1] ) : 0 );
    return JS_UNDEFINED;
}

JSValue widgetSetMaxSize( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setMaxSize( argc > 0 ? argInt32( ctx, argv[0] ) : 0, argc > 1 ? argInt32( ctx, argv[1] ) : 0 );
    return JS_UNDEFINED;
}

JSValue widgetSetOpacity( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    int value = argc > 0 ? argInt32( ctx, argv[0], 255 ) : 255;
    if ( value < 0 )
        value = 0;
    if ( value > 255 )
        value = 255;
    w->setOpacity( static_cast<BYTE>( value ) );
    return JS_UNDEFINED;
}

JSValue widgetGetOpacity( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    Widget *w = widgetOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewInt32( ctx, static_cast<int32_t>( w->getOpacity() ) );
}

} // namespace

JSValue widgetConstructor( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    Widget *w = new Widget();
    Handle *h = newHandle( widgetClass(), w );
    JSValue obj = wrapHandle( ctx, widgetClass(), h );
    if ( JS_IsException( obj ) )
        return obj;
    if ( argc > 0 && JS_IsObject( argv[0] ) )
        w->create( Widget::configFromJson( jsToJson( ctx, argv[0] ) ) );
    return obj;
}

MethodTable buildWidgetMethods() {
    MethodTable t;
    t.add( "create", 1, widgetCreate )
        .add( "close", 0, widgetClose )
        .add( "bringToTop", 0, widgetBringToTop )
        .add( "bringToBottom", 0, widgetBringToBottom )
        .add( "setClickThrough", 1, widgetSetClickThrough )
        .add( "isClickThrough", 0, widgetIsClickThrough )
        .add( "getHwnd", 0, widgetGetHwnd )
        .add( "isChild", 0, widgetIsChild )
        .add( "setTitle", 1, widgetSetTitle )
        .add( "getTitle", 0, widgetGetTitle )
        .add( "setSize", 3, widgetSetSize )
        .add( "getSize", 0, widgetGetSize )
        .add( "getClientSize", 0, widgetGetClientSize )
        .add( "setPosition", 2, widgetSetPosition )
        .add( "getPosition", 0, widgetGetPosition )
        .add( "setRect", 4, widgetSetRect )
        .add( "center", 0, widgetCenter )
        .add( "setTopMost", 1, widgetSetTopMost )
        .add( "setFullscreen", 1, widgetSetFullscreen )
        .add( "isFullscreen", 0, widgetIsFullscreen )
        .add( "maximize", 0, widgetMaximize )
        .add( "minimize", 0, widgetMinimize )
        .add( "restore", 0, widgetRestore )
        .add( "setVisible", 1, widgetSetVisible )
        .add( "isVisible", 0, widgetIsVisible )
        .add( "isMaximized", 0, widgetIsMaximized )
        .add( "isMinimized", 0, widgetIsMinimized )
        .add( "setMinSize", 2, widgetSetMinSize )
        .add( "setMaxSize", 2, widgetSetMaxSize )
        .add( "setOpacity", 1, widgetSetOpacity )
        .add( "getOpacity", 0, widgetGetOpacity );
    return t;
}

namespace {

/**
 * 方法表必须与运行时同寿命：QuickJS 的属性表项惰性初始化时会缓存数组元素地址，
 * 因此这里用静态局部变量持有，不能返回临时对象。
 */
MethodTable &widgetMethods() {
    static MethodTable table = buildWidgetMethods();
    return table;
}

} // namespace

const ClassBinding &widgetClass() {
    static const ClassBinding binding = [] {
        ClassBinding b;
        JS_NewClassID( &b.id );
        b.name = "Widget";
        b.ctor = widgetConstructor;
        b.methods = &widgetMethods();
        b.asWidget = []( void *p ) { return static_cast<Widget *>( p ); };
        b.destroy = []( void *p ) { delete static_cast<Widget *>( p ); };
        return b;
    }();
    return binding;
}

} // namespace detail
} // namespace webviewjs