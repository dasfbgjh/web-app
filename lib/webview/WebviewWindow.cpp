#include "WebviewWindow.h"
#include "common/Logger.hpp"
#include <algorithm>

WebviewWindow::WebviewWindow() = default;

json::value WebviewWindow::configToJson( const Config &config ) {
    json::object obj = WebView::configToJson( config.webview ).as_object();
    for ( const auto &kv : Widget::configToJson( config.window ).as_object() )
        obj[kv.key()] = kv.value();
    return obj;
}

WebviewWindow::Config WebviewWindow::configFromJson( const json::value &value ) {
    Config cfg;
    if ( !value.is_object() )
        return cfg;

    cfg.window = Widget::configFromJson( value );
    cfg.webview = WebView::configFromJson( value );
    return cfg;
}

WebviewWindow::~WebviewWindow() {
    close();
}

bool WebviewWindow::create( const Config &config, CreatedCallback onCreated ) {
    Widget::Config winCfg = config.window;
    winCfg.clipChildren = true;
    if ( !Widget::create( winCfg ) )
        return false;

    m_mainWebview = std::make_unique<WebviewWidget>();

    WebviewWidget::Config mainCfg;
    mainCfg.window.parent = Widget::getHwnd();
    mainCfg.window.x = 0;
    mainCfg.window.y = 0;
    mainCfg.window.noBorder = true;
    mainCfg.window.transparentBackground = config.window.transparentBackground;
    mainCfg.webview = config.webview;
    mainCfg.targetWindow = this;

    bindWebviewMethods();
    onMainWebviewCreate( mainCfg, m_mainWebview.get() );
    m_mainWebview->create( mainCfg, [this, onCreated]( bool success, WebviewWidget *wv ) {
        onMainWebviewCreateComplete( success, wv );
        if ( onCreated )
            onCreated( success, this );
    } );

    return true;
}

void WebviewWindow::close() {
    m_subWebviews.clear();
    m_mainWebview.reset();
    Widget::close();
}

WebviewWidget *WebviewWindow::getMainWebview() {
    return m_mainWebview.get();
}

WebviewWidget *WebviewWindow::getSubWebview( const std::string &name ) {
    return findSubWebview( name );
}

bool WebviewWindow::addSubWebview( const std::string &name, WebviewWidget::Config &config ) {
    HWND hwnd = Widget::getHwnd();
    if ( !hwnd )
        return false;
    auto sub = std::make_unique<WebviewWidget>();

    std::string hostName, resourcePrefix;
    m_mainWebview->getVirtualHostName( hostName, resourcePrefix );
    sub->setVirtualHostNameForResources( hostName, resourcePrefix );
    sub->setName( name );

    config.window.parent = hwnd;
    config.window.noBorder = true;

    onSubWebviewCreate( config, sub.get() );
    sub->create( config, [this]( bool success, WebviewWidget *wv ) {
        onSubWebviewCreateComplete( success, wv );
    } );
    m_subWebviews.push_back( std::move( sub ) );

    return true;
}

bool WebviewWindow::removeSubWebview( const std::string &name ) {
    auto it = std::find_if( m_subWebviews.begin(), m_subWebviews.end(),
                            [&name]( const std::unique_ptr<WebviewWidget> &wv ) { return wv->getName() == name; } );
    if ( it == m_subWebviews.end() )
        return false;

    ( *it )->close();
    m_subWebviews.erase( it );
    return true;
}

LRESULT WebviewWindow::onSize( int width, int height, int type ) {
    if ( type != SIZE_MINIMIZED ) {
        if ( m_mainWebview )
            m_mainWebview->setSize( width, height, false );
    }
    return 0;
}

LRESULT WebviewWindow::onDestroy() {
    for ( auto &wv : m_subWebviews )
        wv->WebView::close();
    m_subWebviews.clear();
    if ( m_mainWebview )
        m_mainWebview->WebView::close();
    m_mainWebview.reset();
    return Widget::onDestroy();
}

void WebviewWindow::onMainWebviewCreate( WebviewWidget::Config &config, WebviewWidget *mainWebview ) {
}

void WebviewWindow::onMainWebviewCreateComplete( bool success, WebviewWidget *mainWebview ) {
    if ( success ) {
        int w, h;
        Widget::getClientSize( w, h );
        mainWebview->setSize( w, h, false );
    }
}

void WebviewWindow::onSubWebviewCreate( WebviewWidget::Config &config, WebviewWidget *subWebview ) {
}

void WebviewWindow::onSubWebviewCreateComplete( bool success, WebviewWidget *subWebview ) {
    if ( success ) {
        Widget::changeZOrder( m_mainWebview.get(), subWebview );
    }
}

WebviewWidget *WebviewWindow::findSubWebview( const std::string &name ) {
    for ( auto &wv : m_subWebviews )
        if ( wv->getName() == name )
            return wv.get();
    return nullptr;
}

const WebviewWidget *WebviewWindow::findSubWebview( const std::string &name ) const {
    for ( const auto &wv : m_subWebviews )
        if ( wv->getName() == name )
            return wv.get();
    return nullptr;
}

void WebviewWindow::bindWebviewMethods() {
    if ( !m_mainWebview )
        return;

    m_mainWebview->bind( "webviewAdd", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewAdd( args );
    } );
    m_mainWebview->bind( "webviewRemove", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewRemove( args );
    } );
    m_mainWebview->bind( "webviewSetPosition", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewSetPosition( args );
    } );
    m_mainWebview->bind( "webviewSetSize", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewSetSize( args );
    } );
    m_mainWebview->bind( "webviewSetRect", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewSetRect( args );
    } );
    m_mainWebview->bind( "webviewGetNames", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewGetNames( args );
    } );
    m_mainWebview->bind( "webviewGetCount", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewGetCount( args );
    } );
    m_mainWebview->bind( "webviewChangeZOrder", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewChangeZOrder( args );
    } );
    m_mainWebview->bind( "webviewNavigate", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewNavigate( args );
    } );
    m_mainWebview->bind( "webviewExecuteScript", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewExecuteScript( args );
    } );
    m_mainWebview->bind( "webviewSetUserAgent", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewSetUserAgent( args );
    } );
    m_mainWebview->bind( "webviewOpenDevTools", [this]( const json::array &args, WebView * ) -> json::value {
        return onWebviewOpenDevTools( args );
    } );
}

json::value WebviewWindow::onWebviewAdd( const json::array &args ) {
    if ( args.size() < 1 || !args[0].is_string() )
        return false;

    std::string name = std::string( args[0].as_string() );
    if ( name.empty() || findSubWebview( name ) )
        return false;

    json::value value = args.size() > 1 ? args[1] : json::value( json::object{} );
    if ( !value.is_object() )
        return false;
    const auto &obj = value.as_object();

    WebviewWidget::Config config = WebviewWidget::configFromJson( value );

    if ( !obj.contains( "x" ) )
        config.window.x = 0;
    if ( !obj.contains( "y" ) )
        config.window.y = 0;
    if ( !obj.contains( "width" ) )
        config.window.width = 0;
    if ( !obj.contains( "height" ) )
        config.window.height = 0;

    return addSubWebview( name, config );
}

json::value WebviewWindow::onWebviewRemove( const json::array &args ) {
    if ( args.size() < 1 || !args[0].is_string() )
        return false;

    std::string name = std::string( args[0].as_string() );

    return removeSubWebview( name );
}

json::value WebviewWindow::onWebviewSetPosition( const json::array &args ) {
    if ( args.size() < 3 || !args[0].is_string() || !args[1].is_int64() || !args[2].is_int64() )
        return nullptr;
    auto *wv = findSubWebview( std::string( args[0].as_string() ) );
    if ( !wv )
        return nullptr;
    HWND wvHwnd = wv->getHwnd();
    if ( wvHwnd ) {
        int w, h;
        wv->getSize( w, h );
        MoveWindow( wvHwnd, static_cast<int>( args[1].as_int64() ), static_cast<int>( args[2].as_int64() ), w, h, TRUE );
    }
    return nullptr;
}

json::value WebviewWindow::onWebviewSetSize( const json::array &args ) {
    if ( args.size() < 3 || !args[0].is_string() || !args[1].is_int64() || !args[2].is_int64() )
        return nullptr;
    auto *wv = findSubWebview( std::string( args[0].as_string() ) );
    if ( !wv )
        return nullptr;
    wv->setSize( static_cast<int>( args[1].as_int64() ), static_cast<int>( args[2].as_int64() ) );
    return nullptr;
}

json::value WebviewWindow::onWebviewSetRect( const json::array &args ) {
    if ( args.size() < 5 || !args[0].is_string() || !args[1].is_int64() || !args[2].is_int64() || !args[3].is_int64() || !args[4].is_int64() )
        return nullptr;
    auto *wv = findSubWebview( std::string( args[0].as_string() ) );
    if ( !wv )
        return nullptr;
    wv->setRect( static_cast<int>( args[1].as_int64() ), static_cast<int>( args[2].as_int64() ),
                 static_cast<int>( args[3].as_int64() ), static_cast<int>( args[4].as_int64() ) );
    return nullptr;
}

json::value WebviewWindow::onWebviewGetNames( const json::array & ) {
    json::array names;
    for ( const auto &wv : m_subWebviews )
        names.push_back( json::value( wv->getName() ) );
    return names;
}

json::value WebviewWindow::onWebviewGetCount( const json::array & ) {
    return static_cast<int64_t>( m_subWebviews.size() );
}

json::value WebviewWindow::onWebviewChangeZOrder( const json::array &args ) {
    if ( args.size() < 2 || !args[0].is_string() || !args[1].is_string() )
        return false;
    auto *bottom = findSubWebview( std::string( args[0].as_string() ) );
    auto *top = findSubWebview( std::string( args[1].as_string() ) );
    if ( !bottom || !top )
        return false;
    return Widget::changeZOrder( bottom, top );
}

json::value WebviewWindow::onWebviewNavigate( const json::array &args ) {
    if ( args.size() < 2 || !args[0].is_string() || !args[1].is_string() )
        return false;
    auto *wv = findSubWebview( std::string( args[0].as_string() ) );
    if ( !wv )
        return false;
    wv->navigate( std::string( args[1].as_string() ) );
    return true;
}

json::value WebviewWindow::onWebviewExecuteScript( const json::array &args ) {
    if ( args.size() < 2 || !args[0].is_string() || !args[1].is_string() )
        return false;
    auto *wv = findSubWebview( std::string( args[0].as_string() ) );
    if ( !wv )
        return false;
    wv->executeScript( std::string( args[1].as_string() ) );
    return true;
}

json::value WebviewWindow::onWebviewSetUserAgent( const json::array &args ) {
    if ( args.size() < 2 || !args[0].is_string() || !args[1].is_string() )
        return false;
    auto *wv = findSubWebview( std::string( args[0].as_string() ) );
    if ( !wv )
        return false;
    return wv->setUserAgent( std::string( args[1].as_string() ) );
}

json::value WebviewWindow::onWebviewOpenDevTools( const json::array &args ) {
    if ( args.size() < 1 || !args[0].is_string() )
        return false;
    auto *wv = findSubWebview( std::string( args[0].as_string() ) );
    if ( !wv )
        return false;
    wv->openDevToolsWindow();
    return true;
}