#include "PopupWebview.h"
#include "common/Logger.hpp"

PopupWebview::PopupWebview() {
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

    bind( "hidePopup", [this]( const json::array &args, WebView * ) -> json::value {
        json::value data = args.size() == 0 ? json::object() : args.at( 0 );
        sendIpcMessage( { m_sourceWindow }, data );
        Widget::setVisible( false );
        return json::object( { { "isOk", true } } );
    } );

    setName( "popup" );
    setVirtualHostNameForResources( "popup.app", "/html/popup" );

    setNavigationCompletedCallback( [this]( bool isSuccess, WebView * ) {
        if ( isSuccess ) {
            m_ready = true;
            Widget::setVisible( false );
            if ( m_hasPending ) {
                m_hasPending = false;
                doShow();
            }
        }
    } );

    create( config, [this]( bool success, WebviewWidget *wv ) {
        if ( !success ) {
            LOG_WARN << "create popup webview failed";
            return;
        }
        navigate( getResourceUrl( "/index.html" ) );
    } );
}

PopupWebview::~PopupWebview() {
}

PopupWebview &PopupWebview::getInstance() {
    static PopupWebview instance;
    return instance;
}

void PopupWebview::doShow() {
    maximize();
    std::string paramsJson = json::serialize( m_pendingParams );
    std::string script = "showPopup('" + m_pendingType + "'," +
                         std::to_string( m_pendingX ) + "," +
                         std::to_string( m_pendingY ) + "," +
                         paramsJson + ")";
    executeScript( script );
}

void PopupWebview::show( const std::string &type, int x, int y, const std::string &sourceWindow, const json::value &params ) {
    auto &popup = getInstance();
    popup.m_sourceWindow = sourceWindow;
    popup.m_pendingType = type;
    popup.m_pendingX = x;
    popup.m_pendingY = y;
    popup.m_pendingSourceWindow = sourceWindow;
    popup.m_pendingParams = params;

    if ( popup.m_ready ) {
        popup.doShow();
    } else {
        popup.m_hasPending = true;
    }
}