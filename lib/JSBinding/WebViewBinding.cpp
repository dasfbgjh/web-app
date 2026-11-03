#include "WebViewBinding.h"

#include "common/Logger.hpp"

#include <map>
#include <string>
#include <vector>

namespace webviewjs {
namespace detail {

namespace {

JSValue webviewCreate( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    int64_t parent = argc > 0 ? argInt64( ctx, argv[0] ) : 0;
    WebView::Config cfg = argc > 1 ? WebView::configFromJson( jsToJson( ctx, argv[1] ) ) : WebView::Config{};
    const CallbackStorePtr store = h->callbacks;
    if ( argc > 2 )
        setCallback( ctx, store, "created", argv[2] );

    bool ok = w->create( reinterpret_cast<HWND>( static_cast<intptr_t>( parent ) ), cfg,
                         [ctx, store]( bool success, WebView * ) { forwardCreated( ctx, store, success ); } );
    return JS_NewBool( ctx, ok );
}

JSValue webviewClose( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->close();
    return JS_UNDEFINED;
}

JSValue webviewNavigate( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->navigate( argc > 0 ? argString( ctx, argv[0] ) : std::string() ) );
}

JSValue webviewNavigateToString( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->navigateToString( argc > 0 ? argString( ctx, argv[0] ) : std::string() ) );
}

JSValue webviewExecuteScript( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    std::string script = argc > 0 ? argString( ctx, argv[0] ) : std::string();
    const CallbackStorePtr store = h->callbacks;
    if ( argc > 1 )
        setCallback( ctx, store, "scriptResult", argv[1] );

    w->executeScript( script, [ctx, store]( const std::string &result, WebView * ) {
        JSValue arg = newString( ctx, result );
        invokeCallback( ctx, store, "scriptResult", 1, &arg );
        JS_FreeValue( ctx, arg );
    } );
    return JS_UNDEFINED;
}

JSValue webviewAddScriptToExecuteOnDocumentCreated( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    std::string script = argc > 0 ? argString( ctx, argv[0] ) : std::string();
    const CallbackStorePtr store = h->callbacks;
    if ( argc > 1 )
        setCallback( ctx, store, "documentCreatedScript", argv[1] );

    w->addScriptToExecuteOnDocumentCreated( script, [ctx, store]( const std::string &result, WebView * ) {
        JSValue arg = newString( ctx, result );
        invokeCallback( ctx, store, "documentCreatedScript", 1, &arg );
        JS_FreeValue( ctx, arg );
    } );
    return JS_UNDEFINED;
}

JSValue webviewPostWebMessage( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    if ( argc < 1 )
        return JS_ThrowTypeError( ctx, "postWebMessage 需要消息内容" );
    w->postWebMessageToAllFrames( jsToJson( ctx, argv[0] ) );
    return JS_UNDEFINED;
}

JSValue webviewResize( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->resize( argc > 0 ? argInt32( ctx, argv[0] ) : 0,
               argc > 1 ? argInt32( ctx, argv[1] ) : 0,
               argc > 2 ? argInt32( ctx, argv[2] ) : 0,
               argc > 3 ? argInt32( ctx, argv[3] ) : 0 );
    return JS_UNDEFINED;
}

JSValue webviewSetVisible( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setVisible( argc > 0 ? argBool( ctx, argv[0], true ) : true );
    return JS_UNDEFINED;
}

JSValue webviewIsVisible( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->isVisible() );
}

JSValue webviewSetZoomFactor( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setZoomFactor( argc > 0 ? argDouble( ctx, argv[0], 1.0 ) : 1.0 );
    return JS_UNDEFINED;
}

JSValue webviewGetZoomFactor( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewFloat64( ctx, w->getZoomFactor() );
}

JSValue webviewGoBack( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->goBack();
    return JS_UNDEFINED;
}

JSValue webviewGoForward( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->goForward();
    return JS_UNDEFINED;
}

JSValue webviewCanGoBack( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->canGoBack() );
}

JSValue webviewCanGoForward( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->canGoForward() );
}

JSValue webviewReload( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->reload();
    return JS_UNDEFINED;
}

JSValue webviewStop( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->stop();
    return JS_UNDEFINED;
}

JSValue webviewOpenDevToolsWindow( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->openDevToolsWindow();
    return JS_UNDEFINED;
}

JSValue webviewGetSource( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return newString( ctx, w->getSource() );
}

JSValue webviewGetDocumentTitle( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return newString( ctx, w->getDocumentTitle() );
}

JSValue webviewGetBrowserVersion( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return newString( ctx, w->getBrowserVersion() );
}

JSValue webviewSetName( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    w->setName( argc > 0 ? argString( ctx, argv[0] ) : std::string() );
    return JS_UNDEFINED;
}

JSValue webviewGetName( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return newString( ctx, w->getName() );
}

JSValue webviewSendIpcMessage( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;

    std::vector<std::string> targets;
    if ( argc > 0 ) {
        json::value list = jsToJson( ctx, argv[0] );
        if ( list.is_array() ) {
            for ( const json::value &item : list.as_array() )
                if ( item.is_string() )
                    targets.emplace_back( item.as_string() );
        }
    }
    json::value data = argc > 1 ? jsToJson( ctx, argv[1] ) : json::value();
    return JS_NewBool( ctx, w->sendIpcMessage( targets, data ) );
}

JSValue webviewSetUserAgent( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewBool( ctx, w->setUserAgent( argc > 0 ? argString( ctx, argv[0] ) : std::string() ) );
}

JSValue webviewGetUserAgent( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return newString( ctx, w->getUserAgent() );
}

JSValue webviewSetVirtualHostNameForResources( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    std::string host = argc > 0 ? argString( ctx, argv[0] ) : std::string();
    std::string prefix = argc > 1 ? argString( ctx, argv[1] ) : std::string();
    return JS_NewBool( ctx, w->setVirtualHostNameForResources( host, prefix ) );
}

JSValue webviewGetVirtualHostName( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    std::string host, prefix;
    w->getVirtualHostName( host, prefix );
    JSValue obj = JS_NewObject( ctx );
    JS_SetPropertyStr( ctx, obj, "hostName", newString( ctx, host ) );
    JS_SetPropertyStr( ctx, obj, "resourcePrefix", newString( ctx, prefix ) );
    return obj;
}

JSValue webviewGetResourceUrl( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return newString( ctx, w->getResourceUrl( argc > 0 ? argString( ctx, argv[0] ) : std::string() ) );
}

JSValue webviewAddApiRoute( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );
    if ( argc < 3 || !JS_IsFunction( ctx, argv[2] ) )
        return JS_ThrowTypeError( ctx, "addApiRoute 需要 method、path 与回调函数" );

    std::string method = argString( ctx, argv[0] );
    std::string route = argString( ctx, argv[1] );
    std::string key = "api:" + method + ":" + route;
    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, key, argv[2] );

    w->addApiRoute( method, route,
                    [ctx, store, key]( const std::map<std::string, std::string> &query,
                                       const std::map<std::string, std::string> &headers,
                                       const std::string &body,
                                       std::map<std::string, std::string> &responseHeaders,
                                       std::string &responseBody,
                                       int &statusCode ) {
                        auto it = store->map.find( key );
                        if ( it == store->map.end() )
                            return;

                        json::object request;
                        request["query"] = mapToJson( query );
                        request["headers"] = mapToJson( headers );
                        request["body"] = body;

                        JSValue arg = jsonToJS( ctx, json::value( request ) );
                        JSValue ret = JS_Call( ctx, it->second, JS_UNDEFINED, 1, &arg );
                        JS_FreeValue( ctx, arg );
                        if ( JS_IsException( ret ) ) {
                            JSValue ex = JS_GetException( ctx );
                            LOG_WARN << "API 路由回调异常: " << qjs::exceptionToString( ctx, ex );
                            JS_FreeValue( ctx, ex );
                            return;
                        }

                        json::value result = jsToJson( ctx, ret );
                        JS_FreeValue( ctx, ret );
                        if ( !result.is_object() )
                            return;

                        const json::object &obj = result.as_object();
                        if ( obj.contains( "headers" ) && obj.at( "headers" ).is_object() ) {
                            for ( const auto &kv : obj.at( "headers" ).as_object() )
                                if ( kv.value().is_string() )
                                    responseHeaders[std::string( kv.key() )] = std::string( kv.value().as_string() );
                        }
                        if ( obj.contains( "body" ) && obj.at( "body" ).is_string() )
                            responseBody = std::string( obj.at( "body" ).as_string() );
                        if ( obj.contains( "statusCode" ) && obj.at( "statusCode" ).is_int64() )
                            statusCode = static_cast<int>( obj.at( "statusCode" ).as_int64() );
                    } );
    return JS_UNDEFINED;
}

JSValue webviewRemoveApiRoute( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    std::string method = argc > 0 ? argString( ctx, argv[0] ) : std::string();
    std::string route = argc > 1 ? argString( ctx, argv[1] ) : std::string();
    removeCallback( ctx, h->callbacks, "api:" + method + ":" + route );
    w->removeApiRoute( method, route );
    return JS_UNDEFINED;
}

JSValue webviewBind( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );
    if ( argc < 2 || !JS_IsFunction( ctx, argv[1] ) )
        return JS_ThrowTypeError( ctx, "bind 需要函数名与回调函数" );

    std::string name = argString( ctx, argv[0] );
    std::string key = "bind:" + name;
    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, key, argv[1] );

    w->bind( name, [ctx, store, key]( const json::array &args, WebView * ) -> json::value {
        auto it = store->map.find( key );
        if ( it == store->map.end() )
            return json::value();

        std::vector<JSValue> values;
        values.reserve( args.size() );
        for ( const json::value &item : args )
            values.push_back( jsonToJS( ctx, item ) );

        JSValue ret = JS_Call( ctx, it->second, JS_UNDEFINED, static_cast<int>( values.size() ), values.data() );
        for ( JSValue v : values )
            JS_FreeValue( ctx, v );

        if ( JS_IsException( ret ) ) {
            JSValue ex = JS_GetException( ctx );
            LOG_WARN << "绑定函数异常: " << qjs::exceptionToString( ctx, ex );
            JS_FreeValue( ctx, ex );
            return json::value();
        }
        json::value result = jsToJson( ctx, ret );
        JS_FreeValue( ctx, ret );
        return result;
    } );
    return JS_UNDEFINED;
}

JSValue webviewUnbind( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    std::string name = argc > 0 ? argString( ctx, argv[0] ) : std::string();
    removeCallback( ctx, h->callbacks, "bind:" + name );
    w->unbind( name );
    return JS_UNDEFINED;
}

JSValue webviewGetBoundNames( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return newStringArray( ctx, w->getBoundNames() );
}

JSValue webviewSetNavigationCompletedCallback( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, "navigationCompleted", argc > 0 ? argv[0] : JS_UNDEFINED );
    w->setNavigationCompletedCallback( [ctx, store]( bool success, WebView * ) {
        JSValue arg = JS_NewBool( ctx, success );
        invokeCallback( ctx, store, "navigationCompleted", 1, &arg );
        JS_FreeValue( ctx, arg );
    } );
    return JS_UNDEFINED;
}

JSValue webviewSetNavigationStartingCallback( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, "navigationStarting", argc > 0 ? argv[0] : JS_UNDEFINED );
    w->setNavigationStartingCallback( [ctx, store]( const std::string &uri, WebView * ) -> bool {
        auto it = store->map.find( "navigationStarting" );
        if ( it == store->map.end() )
            return true;
        JSValue arg = newString( ctx, uri );
        JSValue ret = JS_Call( ctx, it->second, JS_UNDEFINED, 1, &arg );
        JS_FreeValue( ctx, arg );
        if ( JS_IsException( ret ) ) {
            JS_FreeValue( ctx, JS_GetException( ctx ) );
            return true;
        }
        bool keep = argBool( ctx, ret, true );
        JS_FreeValue( ctx, ret );
        return keep;
    } );
    return JS_UNDEFINED;
}

JSValue webviewSetWebMessageReceivedCallback( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, "webMessageReceived", argc > 0 ? argv[0] : JS_UNDEFINED );
    w->setWebMessageReceivedCallback( [ctx, store]( const json::value &message, WebView * ) {
        JSValue arg = jsonToJS( ctx, message );
        invokeCallback( ctx, store, "webMessageReceived", 1, &arg );
        JS_FreeValue( ctx, arg );
    } );
    return JS_UNDEFINED;
}

JSValue webviewSetDocumentTitleChangedCallback( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, "documentTitleChanged", argc > 0 ? argv[0] : JS_UNDEFINED );
    w->setDocumentTitleChangedCallback( [ctx, store]( const std::string &title, WebView * ) {
        JSValue arg = newString( ctx, title );
        invokeCallback( ctx, store, "documentTitleChanged", 1, &arg );
        JS_FreeValue( ctx, arg );
    } );
    return JS_UNDEFINED;
}

JSValue webviewSetConsoleMessageCallback( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, "consoleMessage", argc > 0 ? argv[0] : JS_UNDEFINED );
    w->setConsoleMessageCallback( [ctx, store]( int level, const std::string &message, WebView * ) {
        JSValue args[2] = { JS_NewInt32( ctx, level ), newString( ctx, message ) };
        invokeCallback( ctx, store, "consoleMessage", 2, args );
        JS_FreeValue( ctx, args[0] );
        JS_FreeValue( ctx, args[1] );
    } );
    return JS_UNDEFINED;
}

JSValue webviewSetNewWindowRequestedCallback( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, "newWindowRequested", argc > 0 ? argv[0] : JS_UNDEFINED );
    w->setNewWindowRequestedCallback( [ctx, store]( const std::string &url, bool userInitiated, WebView * ) -> bool {
        auto it = store->map.find( "newWindowRequested" );
        if ( it == store->map.end() )
            return false;
        JSValue args[2] = { newString( ctx, url ), JS_NewBool( ctx, userInitiated ) };
        JSValue ret = JS_Call( ctx, it->second, JS_UNDEFINED, 2, args );
        JS_FreeValue( ctx, args[0] );
        JS_FreeValue( ctx, args[1] );
        if ( JS_IsException( ret ) ) {
            JS_FreeValue( ctx, JS_GetException( ctx ) );
            return false;
        }
        bool intercept = argBool( ctx, ret, false );
        JS_FreeValue( ctx, ret );
        return intercept;
    } );
    return JS_UNDEFINED;
}

JSValue webviewSetWebResourceRequestedCallback( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, "webResourceRequested", argc > 0 ? argv[0] : JS_UNDEFINED );
    w->setWebResourceRequestedCallback(
        [ctx, store]( const std::string &url,
                      const std::string &method,
                      const std::map<std::string, std::string> &headers,
                      std::map<std::string, std::string> &responseHeaders,
                      std::string &responseBody,
                      int &statusCode ) -> bool {
            auto it = store->map.find( "webResourceRequested" );
            if ( it == store->map.end() )
                return false;

            json::object request;
            request["url"] = url;
            request["method"] = method;
            request["headers"] = mapToJson( headers );

            JSValue arg = jsonToJS( ctx, json::value( request ) );
            JSValue ret = JS_Call( ctx, it->second, JS_UNDEFINED, 1, &arg );
            JS_FreeValue( ctx, arg );
            if ( JS_IsException( ret ) ) {
                JS_FreeValue( ctx, JS_GetException( ctx ) );
                return false;
            }

            json::value result = jsToJson( ctx, ret );
            JS_FreeValue( ctx, ret );
            if ( !result.is_object() )
                return false;

            const json::object &obj = result.as_object();
            if ( !obj.contains( "intercept" ) || !obj.at( "intercept" ).as_bool() )
                return false;
            if ( obj.contains( "headers" ) && obj.at( "headers" ).is_object() ) {
                for ( const auto &kv : obj.at( "headers" ).as_object() )
                    if ( kv.value().is_string() )
                        responseHeaders[std::string( kv.key() )] = std::string( kv.value().as_string() );
            }
            if ( obj.contains( "body" ) && obj.at( "body" ).is_string() )
                responseBody = std::string( obj.at( "body" ).as_string() );
            if ( obj.contains( "statusCode" ) && obj.at( "statusCode" ).is_int64() )
                statusCode = static_cast<int>( obj.at( "statusCode" ).as_int64() );
            return true;
        } );
    return JS_UNDEFINED;
}

JSValue webviewSetWebResourceResponseReceivedCallback( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    Handle *h = anyHandle( ctx, thisVal );
    WebView *w = webviewOf( h );
    if ( !w )
        return JS_ThrowTypeError( ctx, "该对象不支持 WebView 操作" );

    const CallbackStorePtr store = h->callbacks;
    setCallback( ctx, store, "webResourceResponseReceived", argc > 0 ? argv[0] : JS_UNDEFINED );
    w->setWebResourceResponseReceivedCallback(
        [ctx, store]( const std::string &url,
                      const std::string &method,
                      int statusCode,
                      const std::string &reason,
                      const std::map<std::string, std::string> &headers,
                      const std::string &body ) {
            json::object response;
            response["url"] = url;
            response["method"] = method;
            response["statusCode"] = statusCode;
            response["reasonPhrase"] = reason;
            response["headers"] = mapToJson( headers );
            response["body"] = body;

            JSValue arg = jsonToJS( ctx, json::value( response ) );
            invokeCallback( ctx, store, "webResourceResponseReceived", 1, &arg );
            JS_FreeValue( ctx, arg );
        } );
    return JS_UNDEFINED;
}

JSValue webviewAddWebResourceRequestedFilter( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    std::string filter = argc > 0 ? argString( ctx, argv[0] ) : std::string();
    int context = argc > 1 ? argInt32( ctx, argv[1], static_cast<int32_t>( COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL ) )
                           : static_cast<int32_t>( COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL );
    return JS_NewBool( ctx, w->addWebResourceRequestedFilter(
                                filter, static_cast<COREWEBVIEW2_WEB_RESOURCE_CONTEXT>( context ) ) );
}

JSValue webviewRemoveWebResourceRequestedFilter( JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    std::string filter = argc > 0 ? argString( ctx, argv[0] ) : std::string();
    int context = argc > 1 ? argInt32( ctx, argv[1], static_cast<int32_t>( COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL ) )
                           : static_cast<int32_t>( COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL );
    return JS_NewBool( ctx, w->removeWebResourceRequestedFilter(
                                filter, static_cast<COREWEBVIEW2_WEB_RESOURCE_CONTEXT>( context ) ) );
}

JSValue webviewGetParentHwnd( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewInt64( ctx, static_cast<int64_t>( reinterpret_cast<intptr_t>( w->getParentHwnd() ) ) );
}

JSValue webviewGetRenderHwnd( JSContext *ctx, JSValueConst thisVal, int, JSValueConst * ) {
    WebView *w = webviewOf( ctx, thisVal );
    if ( !w )
        return JS_EXCEPTION;
    return JS_NewInt64( ctx, static_cast<int64_t>( reinterpret_cast<intptr_t>( w->getRenderHwnd() ) ) );
}

JSValue webviewStaticExecuteScript( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    if ( argc < 2 )
        return JS_ThrowTypeError( ctx, "executeScriptByName 需要 name 与 script" );
    std::string name = argString( ctx, argv[0] );
    std::string script = argString( ctx, argv[1] );
    JSValue callback = argc > 2 && JS_IsFunction( ctx, argv[2] ) ? JS_DupValue( ctx, argv[2] ) : JS_UNDEFINED;

    bool ok = WebView::executeScript( name, script, [ctx, callback]( const std::string &result, WebView * ) {
        if ( JS_IsFunction( ctx, callback ) ) {
            JSValue arg = newString( ctx, result );
            JSValue ret = JS_Call( ctx, callback, JS_UNDEFINED, 1, &arg );
            if ( JS_IsException( ret ) )
                JS_FreeValue( ctx, JS_GetException( ctx ) );
            JS_FreeValue( ctx, ret );
            JS_FreeValue( ctx, arg );
        }
        JS_FreeValue( ctx, callback );
    } );
    if ( !ok )
        JS_FreeValue( ctx, callback );
    return JS_NewBool( ctx, ok );
}

JSValue webviewStaticOpenDevTools( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    if ( argc < 1 )
        return JS_ThrowTypeError( ctx, "openDevToolsByName 需要 name" );
    return JS_NewBool( ctx, WebView::openDevToolsWindow( argString( ctx, argv[0] ) ) );
}

} // namespace

JSValue webViewConstructor( JSContext *ctx, JSValueConst, int argc, JSValueConst *argv ) {
    WebView *w = new WebView();
    Handle *h = newHandle( webViewClass(), w );
    JSValue obj = wrapHandle( ctx, webViewClass(), h );
    if ( JS_IsException( obj ) )
        return obj;

    if ( argc > 0 ) {
        int64_t parent = argInt64( ctx, argv[0] );
        WebView::Config cfg = argc > 1 ? WebView::configFromJson( jsToJson( ctx, argv[1] ) ) : WebView::Config{};
        const CallbackStorePtr store = h->callbacks;
        if ( argc > 2 )
            setCallback( ctx, store, "created", argv[2] );
        w->create( reinterpret_cast<HWND>( static_cast<intptr_t>( parent ) ), cfg,
                   [ctx, store]( bool success, WebView * ) { forwardCreated( ctx, store, success ); } );
    }
    return obj;
}

MethodTable buildWebViewMethods() {
    MethodTable t;
    t.add( "create", 3, webviewCreate )
        .add( "close", 0, webviewClose )
        .add( "navigate", 1, webviewNavigate )
        .add( "navigateToString", 1, webviewNavigateToString )
        .add( "executeScript", 2, webviewExecuteScript )
        .add( "addScriptToExecuteOnDocumentCreated", 2, webviewAddScriptToExecuteOnDocumentCreated )
        .add( "postWebMessage", 1, webviewPostWebMessage )
        .add( "resize", 4, webviewResize )
        .add( "setVisible", 1, webviewSetVisible )
        .add( "isVisible", 0, webviewIsVisible )
        .add( "setZoomFactor", 1, webviewSetZoomFactor )
        .add( "getZoomFactor", 0, webviewGetZoomFactor )
        .add( "goBack", 0, webviewGoBack )
        .add( "goForward", 0, webviewGoForward )
        .add( "canGoBack", 0, webviewCanGoBack )
        .add( "canGoForward", 0, webviewCanGoForward )
        .add( "reload", 0, webviewReload )
        .add( "stop", 0, webviewStop )
        .add( "openDevToolsWindow", 0, webviewOpenDevToolsWindow )
        .add( "getSource", 0, webviewGetSource )
        .add( "getDocumentTitle", 0, webviewGetDocumentTitle )
        .add( "getBrowserVersion", 0, webviewGetBrowserVersion )
        .add( "setName", 1, webviewSetName )
        .add( "getName", 0, webviewGetName )
        .add( "sendIpcMessage", 2, webviewSendIpcMessage )
        .add( "setUserAgent", 1, webviewSetUserAgent )
        .add( "getUserAgent", 0, webviewGetUserAgent )
        .add( "setVirtualHostNameForResources", 2, webviewSetVirtualHostNameForResources )
        .add( "getVirtualHostName", 0, webviewGetVirtualHostName )
        .add( "getResourceUrl", 1, webviewGetResourceUrl )
        .add( "addApiRoute", 3, webviewAddApiRoute )
        .add( "removeApiRoute", 2, webviewRemoveApiRoute )
        .add( "bind", 2, webviewBind )
        .add( "unbind", 1, webviewUnbind )
        .add( "getBoundNames", 0, webviewGetBoundNames )
        .add( "setNavigationCompletedCallback", 1, webviewSetNavigationCompletedCallback )
        .add( "setNavigationStartingCallback", 1, webviewSetNavigationStartingCallback )
        .add( "setWebMessageReceivedCallback", 1, webviewSetWebMessageReceivedCallback )
        .add( "setDocumentTitleChangedCallback", 1, webviewSetDocumentTitleChangedCallback )
        .add( "setConsoleMessageCallback", 1, webviewSetConsoleMessageCallback )
        .add( "setNewWindowRequestedCallback", 1, webviewSetNewWindowRequestedCallback )
        .add( "setWebResourceRequestedCallback", 1, webviewSetWebResourceRequestedCallback )
        .add( "setWebResourceResponseReceivedCallback", 1, webviewSetWebResourceResponseReceivedCallback )
        .add( "addWebResourceRequestedFilter", 2, webviewAddWebResourceRequestedFilter )
        .add( "removeWebResourceRequestedFilter", 2, webviewRemoveWebResourceRequestedFilter )
        .add( "getParentHwnd", 0, webviewGetParentHwnd )
        .add( "getRenderHwnd", 0, webviewGetRenderHwnd );
    return t;
}

MethodTable buildWebViewStaticMethods() {
    MethodTable t;
    t.add( "executeScript", 3, webviewStaticExecuteScript )
        .add( "openDevToolsByName", 1, webviewStaticOpenDevTools );
    return t;
}

namespace {

/**
 * 方法表必须与运行时同寿命：QuickJS 的属性表项惰性初始化时会缓存数组元素地址，
 * 因此这里用静态局部变量持有，不能返回临时对象。
 */
MethodTable &webViewMethods() {
    static MethodTable table = buildWebViewMethods();
    return table;
}

MethodTable &webViewStaticMethods() {
    static MethodTable table = buildWebViewStaticMethods();
    return table;
}

} // namespace

const ClassBinding &webViewClass() {
    static const ClassBinding binding = [] {
        ClassBinding b;
        JS_NewClassID( &b.id );
        b.name = "WebView";
        b.ctor = webViewConstructor;
        b.methods = &webViewMethods();
        b.statics = &webViewStaticMethods();
        b.asWebView = []( void *p ) { return static_cast<WebView *>( p ); };
        b.destroy = []( void *p ) { delete static_cast<WebView *>( p ); };
        return b;
    }();
    return binding;
}

} // namespace detail
} // namespace webviewjs