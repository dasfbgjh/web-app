#include "OverlayWebview.h"
#include "common/Logger.hpp"

OverlayWebview::OverlayWebview() {
    Config config;
    config.window.topMost = true;
    config.window.noBorder = true;
    config.window.transparentBackground = true;
    config.window.noTaskbar = true;
    config.window.visible = true;
    config.window.width = 1;
    config.window.height = 1;
    config.webview.defaultContextMenusEnabled = false;
    config.webview.devToolsEnabled = false;

    bind( "overlayBlockResult", [this]( const json::array &args, WebView * ) -> json::value {
        if ( args.size() < 2 || !args[0].is_string() || !args[1].is_object() )
            return json::object( { { "isOk", false } } );
        std::string sourceWindow = std::string( args[0].as_string() );
        json::value data = args[1];
        if ( !sourceWindow.empty() )
            sendIpcMessage( { sourceWindow }, data );
        return json::object( { { "isOk", true } } );
    } );

    bind( "overlayBlocksEmpty", [this]( const json::array &, WebView * ) -> json::value {
        m_visible = false;
        Widget::setVisible( false );
        Widget::setClickThrough( true );
        return json::object( { { "isOk", true } } );
    } );

    bind( "executeScript", [this]( const json::array &args, WebView * ) -> json::value {
        if ( args.size() < 3 || !args[0].is_string() || !args[1].is_string() || !args[2].is_int64() )
            return json::object( { { "found", false } } );
        std::string name = std::string( args[0].as_string() );
        std::string script = std::string( args[1].as_string() );
        int seqId = static_cast<int>( args[2].as_int64() );
        bool found = WebView::executeScript(
            name, script,
            [this, seqId]( const std::string &result, WebView * ) {
                json::value msg;
                try {
                    msg = json::parse( result );
                } catch ( ... ) {
                    msg = result;
                }
                postWebMessage( json::object( { { "type", "executeScriptResult" },
                                                { "seqId", seqId },
                                                { "result", msg } } ) );
            } );
        return json::object( { { "found", found } } );
    } );

    bind( "openDevTools", []( const json::array &args, WebView * ) -> json::value {
        if ( args.size() < 1 || !args[0].is_string() )
            return false;
        std::string name = std::string( args[0].as_string() );
        return WebView::openDevToolsWindow( name );
    } );

    setName( "overlay" );
    setVirtualHostNameForResources( "overlay.app", "/html/overlay" );

    setNavigationCompletedCallback( [this]( bool isSuccess, WebView * ) {
        onNavigationCompleted( isSuccess, nullptr );
    } );

    create( config, [this]( bool success, WebviewWidget *wv ) {
        if ( !success ) {
            LOG_WARN << "create overlay webview failed";
            return;
        }
        navigate( getResourceUrl( "/index.html" ) );
    } );
}

OverlayWebview::~OverlayWebview() {
}

OverlayWebview &OverlayWebview::getInstance() {
    static OverlayWebview instance;
    return instance;
}

void OverlayWebview::onNavigationCompleted( bool isSuccess, WebView * ) {
    if ( !isSuccess )
        return;
    m_ready = true;
    Widget::setVisible( false );
    if ( m_pendingClear ) {
        m_pendingClear = false;
        m_pendingBlocks.clear();
        m_pendingRemoves.clear();
    }
    for ( auto &block : m_pendingBlocks )
        doAddBlock( block.id, block.type, block.x, block.y, block.sourceWindow, block.params );
    m_pendingBlocks.clear();
    for ( auto &id : m_pendingRemoves )
        doRemoveBlock( id );
    m_pendingRemoves.clear();
}

void OverlayWebview::ensureVisible() {
    if ( !m_visible ) {
        m_visible = true;
        maximize();
        Widget::setVisible( true );
    }
}

void OverlayWebview::doAddBlock( const std::string &id, const std::string &type, int x, int y,
                                 const std::string &sourceWindow, const json::value &params ) {
    ensureVisible();
    std::string paramsJson = json::serialize( params );
    std::string script = "addBlock('" + id + "','" + type + "'," +
                         std::to_string( x ) + "," +
                         std::to_string( y ) + "," +
                         "'" + sourceWindow + "'," +
                         paramsJson + ")";
    executeScript( script );
}

void OverlayWebview::doRemoveBlock( const std::string &id ) {
    std::string script = "removeBlock('" + id + "')";
    executeScript( script );
}

void OverlayWebview::doClearBlocks() {
    executeScript( "clearBlocks()" );
}

void OverlayWebview::addBlock( const std::string &id, const std::string &type, int x, int y,
                               const std::string &sourceWindow, const json::value &params ) {
    auto &overlay = getInstance();
    if ( overlay.m_ready ) {
        overlay.doAddBlock( id, type, x, y, sourceWindow, params );
    } else {
        overlay.m_pendingBlocks.push_back( { id, type, x, y, sourceWindow, params } );
    }
}

void OverlayWebview::removeBlock( const std::string &id ) {
    auto &overlay = getInstance();
    if ( overlay.m_ready ) {
        overlay.doRemoveBlock( id );
    } else {
        overlay.m_pendingRemoves.push_back( id );
    }
}

void OverlayWebview::clearBlocks() {
    auto &overlay = getInstance();
    if ( overlay.m_ready ) {
        overlay.doClearBlocks();
    } else {
        overlay.m_pendingClear = true;
        overlay.m_pendingBlocks.clear();
        overlay.m_pendingRemoves.clear();
    }
}