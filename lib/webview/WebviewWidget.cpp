#include "WebviewWidget.h"
#include "PopupWebview.h"
#include "OverlayWebview.h"
#include "common/Logger.hpp"
#include "common/Utils.h"
#include "webview/EventLoop.h"

WebviewWidget::WebviewWidget() = default;

json::value WebviewWidget::configToJson( const Config &config ) {
    json::object obj = WebView::configToJson( config.webview ).as_object();
    for ( const auto &kv : Widget::configToJson( config.window ).as_object() )
        obj[kv.key()] = kv.value();
    return obj;
}

WebviewWidget::Config WebviewWidget::configFromJson( const json::value &value ) {
    Config cfg;
    if ( !value.is_object() )
        return cfg;

    cfg.window = Widget::configFromJson( value );
    cfg.webview = WebView::configFromJson( value );
    return cfg;
}

WebviewWidget::~WebviewWidget() {
    close();
}

bool WebviewWidget::create( const Config &config, CreatedCallback onCreated ) {
    Widget::Config winCfg = config.window;
    winCfg.clipChildren = true;
    if ( !Widget::create( winCfg ) )
        return false;

    m_targetWindow = config.targetWindow ? config.targetWindow : this;

    HWND hwnd = Widget::getHwnd();
    WebView::Config wvCfg = config.webview;
    if ( m_autoWebviewTransparent )
        wvCfg.transparentBackground = config.window.transparentBackground;

    bindWindowMethods();
    onWebviewCreate( wvCfg );
    WebView::create( hwnd, wvCfg, [this, onCreated]( bool success, WebView *webView ) {
        onWebviewCreateComplete( success );
        if ( onCreated )
            onCreated( success, this );
    } );

    return true;
}

void WebviewWidget::close() {
    WebView::close();
    Widget::close();
}

void WebviewWidget::setAutoWebviewTransparent( bool transparent ) {
    m_autoWebviewTransparent = transparent;
}

void WebviewWidget::setWebViewMargin( int left, int top, int right, int bottom ) {
    m_marginLeft = left;
    m_marginTop = top;
    m_marginRight = right;
    m_marginBottom = bottom;
    resizeWebView();
}

LRESULT WebviewWidget::onSize( int width, int height, int type ) {
    updateWindowRgn();
    if ( type != SIZE_MINIMIZED )
        resizeWebView();
    return 0;
}

LRESULT WebviewWidget::onDestroy() {
    WebView::close();
    return Widget::onDestroy();
}

void WebviewWidget::onWebviewCreate( WebView::Config &config ) {
}

void WebviewWidget::onWebviewCreateComplete( bool success ) {
    if ( success )
        resizeWebView();
}

void WebviewWidget::resizeWebView() {
    int w, h;
    Widget::getClientSize( w, h );
    int x = m_marginLeft;
    int y = m_marginTop;
    int rw = w - m_marginLeft - m_marginRight;
    int rh = h - m_marginTop - m_marginBottom;
    if ( rw < 0 )
        rw = 0;
    if ( rh < 0 )
        rh = 0;
    WebView::resize( x, y, rw, rh );
}

void WebviewWidget::bindWindowMethods() {
    bind( "windowClose", [this]( const json::array &args, WebView * ) -> json::value {
        return onClose( args );
    } );
    bind( "windowMinimize", [this]( const json::array &args, WebView * ) -> json::value {
        return onMinimize( args );
    } );
    bind( "windowMaximize", [this]( const json::array &args, WebView * ) -> json::value {
        return onMaximize( args );
    } );
    bind( "windowDrag", [this]( const json::array &args, WebView * ) -> json::value {
        return onDrag( args );
    } );
    bind( "windowSetTopMost", [this]( const json::array &args, WebView * ) -> json::value {
        return onSetTopMost( args );
    } );
    bind( "windowGetPosition", [this]( const json::array &args, WebView * ) -> json::value {
        return onGetPosition( args );
    } );
    bind( "windowGetSize", [this]( const json::array &args, WebView * ) -> json::value {
        return onGetSize( args );
    } );
    bind( "windowSetOpacity", [this]( const json::array &args, WebView * ) -> json::value {
        return onSetOpacity( args );
    } );
    bind( "windowResize", [this]( const json::array &args, WebView * ) -> json::value {
        return onResize( args );
    } );
    bind( "windowSetRect", [this]( const json::array &args, WebView * ) -> json::value {
        return onSetRect( args );
    } );
    bind( "windowSetRgn", [this]( const json::array &args, WebView * ) -> json::value {
        return onSetWindowRgn( args );
    } );
    bind( "showPopup", [this]( const json::array &args, WebView * ) -> json::value {
        return onShowPopup( args );
    } );
    bind( "showOverlay", [this]( const json::array &args, WebView * ) -> json::value {
        return onShowOverlay( args );
    } );
    bind( "removeOverlay", [this]( const json::array &args, WebView * ) -> json::value {
        return onRemoveOverlay( args );
    } );
    bind( "clearOverlay", [this]( const json::array &args, WebView * ) -> json::value {
        return onClearOverlay( args );
    } );
}

json::value WebviewWidget::onClose( const json::array & ) {
    Widget *target = m_targetWindow;
    EventLoop::postEvent( [target]() { target->close(); } );
    return nullptr;
}

json::value WebviewWidget::onMinimize( const json::array & ) {
    m_targetWindow->minimize();
    return nullptr;
}

json::value WebviewWidget::onMaximize( const json::array & ) {
    if ( m_targetWindow->isMaximized() )
        m_targetWindow->restore();
    else
        m_targetWindow->maximize();
    return nullptr;
}

json::value WebviewWidget::onDrag( const json::array & ) {
    if ( m_targetWindow->isMaximized() || m_targetWindow->isFullscreen() )
        return nullptr;
    HWND hwnd = m_targetWindow->getHwnd();
    if ( !hwnd )
        return nullptr;
    EventLoop::startDragMonitor( 1, [hwnd]( int count, int x, int y ) {
        if ( count == 0 )
            return;
        RECT rc;
        GetWindowRect( hwnd, &rc );
        MoveWindow( hwnd, rc.left + x, rc.top + y, rc.right - rc.left, rc.bottom - rc.top, TRUE );
    } );
    return nullptr;
}

json::value WebviewWidget::onSetTopMost( const json::array &args ) {
    bool topMost = args.size() > 0 && args[0].is_bool() && args[0].as_bool();
    m_targetWindow->setTopMost( topMost );
    return nullptr;
}

json::value WebviewWidget::onGetPosition( const json::array & ) {
    int x, y;
    m_targetWindow->getPosition( x, y );
    return json::object{ { "x", x }, { "y", y } };
}

json::value WebviewWidget::onGetSize( const json::array & ) {
    int w, h;
    m_targetWindow->getSize( w, h );
    return json::object{ { "width", w }, { "height", h } };
}

json::value WebviewWidget::onSetOpacity( const json::array &args ) {
    if ( args.size() > 0 && args[0].is_int64() ) {
        int val = static_cast<int>( args[0].as_int64() );
        if ( val < 0 )
            val = 0;
        if ( val > 255 )
            val = 255;
        m_targetWindow->setOpacity( static_cast<BYTE>( val ) );
    }
    return nullptr;
}

json::value WebviewWidget::onResize( const json::array &args ) {
    if ( m_targetWindow->isMaximized() || m_targetWindow->isFullscreen() )
        return nullptr;
    HWND hwnd = m_targetWindow->getHwnd();
    if ( !hwnd || args.size() == 0 || !args[0].is_string() )
        return nullptr;
    std::string dir = std::string( args[0].as_string() );
    EventLoop::startDragMonitor( 1, [hwnd, dir]( int count, int dx, int dy ) {
        if ( count == 0 )
            return;
        RECT rc;
        GetWindowRect( hwnd, &rc );
        int left = rc.left, top = rc.top, right = rc.right, bottom = rc.bottom;
        if ( dir.find( 'w' ) != std::string::npos )
            left += dx;
        if ( dir.find( 'e' ) != std::string::npos )
            right += dx;
        if ( dir.find( 'n' ) != std::string::npos )
            top += dy;
        if ( dir.find( 's' ) != std::string::npos )
            bottom += dy;
        const int minW = 200, minH = 100;
        if ( right - left < minW ) {
            if ( dir.find( 'w' ) != std::string::npos )
                left = right - minW;
            else
                right = left + minW;
        }
        if ( bottom - top < minH ) {
            if ( dir.find( 'n' ) != std::string::npos )
                top = bottom - minH;
            else
                bottom = top + minH;
        }
        MoveWindow( hwnd, left, top, right - left, bottom - top, TRUE );
        //
    },
                                 100 );
    return nullptr;
}

json::value WebviewWidget::onSetRect( const json::array &args ) {
    if ( args.size() >= 4 && args[0].is_int64() && args[1].is_int64() && args[2].is_int64() && args[3].is_int64() ) {
        int x = static_cast<int>( args[0].as_int64() );
        int y = static_cast<int>( args[1].as_int64() );
        int w = static_cast<int>( args[2].as_int64() );
        int h = static_cast<int>( args[3].as_int64() );
        m_targetWindow->setRect( x, y, w, h );
    }
    return nullptr;
}

json::value WebviewWidget::onSetWindowRgn( const json::array &args ) {
    if ( args.size() < 1 || !args[0].is_array() )
        return false;

    bool clickThrough = false;
    if ( args.size() >= 2 && args[1].is_bool() )
        clickThrough = args[1].as_bool();

    const auto &rgnList = args[0].as_array();
    if ( rgnList.empty() ) {
        m_targetWindow->setWindowRgn( nullptr );
        m_targetWindow->setClickThrough( clickThrough );
        return true;
    }

    m_targetWindow->setClickThrough( false );
    HRGN rgn = utils::createRgn( rgnList );
    if ( !rgn )
        return false;

    m_targetWindow->setWindowRgn( rgn );
    return true;
}

json::value WebviewWidget::onShowPopup( const json::array &args ) {
    if ( args.size() >= 4 && args[0].is_string() && args[1].is_int64() && args[2].is_int64() && args[3].is_object() ) {
        std::string type = std::string( args[0].as_string() );
        int x = static_cast<int>( args[1].as_int64() );
        int y = static_cast<int>( args[2].as_int64() );
        json::value params = args[3];
        std::string sourceName = getName();
        PopupWebview::show( type, x, y, sourceName, params );
    }
    return nullptr;
}

json::value WebviewWidget::onShowOverlay( const json::array &args ) {
    if ( args.size() >= 5 &&
         args[0].is_string() && args[1].is_string() &&
         args[2].is_int64() && args[3].is_int64() && args[4].is_object() ) {
        std::string id = std::string( args[0].as_string() );
        std::string type = std::string( args[1].as_string() );
        int x = static_cast<int>( args[2].as_int64() );
        int y = static_cast<int>( args[3].as_int64() );
        json::value params = args[4];
        std::string sourceName = getName();
        OverlayWebview::addBlock( id, type, x, y, sourceName, params );
    }
    return nullptr;
}

json::value WebviewWidget::onRemoveOverlay( const json::array &args ) {
    if ( args.size() >= 1 && args[0].is_string() ) {
        std::string id = std::string( args[0].as_string() );
        OverlayWebview::removeBlock( id );
    }
    return nullptr;
}

json::value WebviewWidget::onClearOverlay( const json::array & ) {
    OverlayWebview::clearBlocks();
    return nullptr;
}