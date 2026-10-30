#include "Dialog.h"
#include "common/Logger.hpp"

int Dialog::exec( Widget *parent ) {
    if ( !getHwnd() ) {
        LOG_WARN << "Dialog::exec() called before create()";
        return -1;
    }
    if ( !parent ) {
        LOG_WARN << "Dialog::exec() called with nullptr parent";
        return -2;
    }
    HWND ownerHwnd = parent->getHwnd();
    if ( !ownerHwnd ) {
        LOG_WARN << "Dialog::exec() called with nullptr parent";
        return -3;
    }

    SetWindowLongPtr( getHwnd(), GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>( ownerHwnd ) );
    EnableWindow( ownerHwnd, FALSE );
    Widget::setVisible( true );

    m_eventLoop.setMessageHandler( [&]( MSG &msg ) -> bool {
        if ( msg.hwnd == ownerHwnd || ( ownerHwnd && IsChild( ownerHwnd, msg.hwnd ) ) ) {
            if ( msg.message == WM_CLOSE ) {
                return true;
            }
            if ( msg.message == WM_NCHITTEST || msg.message == WM_LBUTTONDOWN ||
                 msg.message == WM_RBUTTONDOWN || msg.message == WM_MOUSEACTIVATE ) {
                bringToTop();
                SetForegroundWindow( getHwnd() );
                return true;
            }
        }
        return false;
    } );

    int code = m_eventLoop.exec( false );
    EnableWindow( ownerHwnd, TRUE );
    SetForegroundWindow( ownerHwnd );

    return code;
}

void Dialog::exit( int code ) {
    if ( m_eventLoop.isRunning() ) {
        m_eventLoop.exit( code );
    }
    Widget::close();
}

LRESULT Dialog::onClose() {
    if ( m_eventLoop.isRunning() ) {
        exit( 0 );
    } else {
        Widget::close();
    }
    return 0;
}
