#include "TrayIcon.h"
#include "common/utils.h"

static UINT s_wmTrayCallback = 0;
static int s_classRefCount = 0;
static const wchar_t *kClassName = L"TrayIconMsgWnd";

TrayIcon::TrayIcon() {
    m_instance = GetModuleHandleW( nullptr );
    if ( !s_wmTrayCallback )
        s_wmTrayCallback = RegisterWindowMessageW( L"TrayIconCallback" );
}

TrayIcon::~TrayIcon() {
    destroy();
}

void TrayIcon::registerClass() {
    WNDCLASSEXW wc = { sizeof( wc ) };
    wc.lpfnWndProc = msgWndProc;
    wc.hInstance = m_instance;
    wc.lpszClassName = kClassName;
    RegisterClassExW( &wc );
}

bool TrayIcon::createMessageWindow() {
    if ( s_classRefCount == 0 )
        registerClass();
    s_classRefCount++;

    m_msgHwnd = CreateWindowExW( 0, kClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, m_instance, this );
    if ( !m_msgHwnd ) {
        s_classRefCount--;
        return false;
    }
    return true;
}

bool TrayIcon::create( const Config &config ) {
    if ( m_created )
        return false;

    if ( !createMessageWindow() )
        return false;

    m_msgId = s_wmTrayCallback;

    m_nid = {};
    m_nid.cbSize = sizeof( NOTIFYICONDATAW );
    m_nid.hWnd = m_msgHwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = m_msgId;
    m_nid.hIcon = config.icon;

    if ( !config.tip.empty() ) {
        std::wstring wTip = utils::strToWiden( config.tip );
        wcsncpy_s( m_nid.szTip, _TRUNCATE, wTip.c_str(), wTip.size() );
    }

    if ( config.showOnCreate ) {
        notify( NIM_ADD );
        m_visible = true;
    }

    m_created = true;
    return true;
}

void TrayIcon::destroy() {
    if ( !m_created )
        return;

    if ( m_visible )
        notify( NIM_DELETE );

    if ( m_msgHwnd ) {
        DestroyWindow( m_msgHwnd );
        m_msgHwnd = nullptr;
        s_classRefCount--;
    }

    if ( m_menu ) {
        DestroyMenu( m_menu );
        m_menu = nullptr;
    }

    m_cmdMap.clear();
    m_created = false;
    m_visible = false;
}

void TrayIcon::notify( DWORD action ) {
    Shell_NotifyIconW( action, &m_nid );
}

void TrayIcon::show() {
    if ( !m_created || m_visible )
        return;
    notify( NIM_ADD );
    m_visible = true;
}

void TrayIcon::hide() {
    if ( !m_created || !m_visible )
        return;
    notify( NIM_DELETE );
    m_visible = false;
}

bool TrayIcon::isVisible() const {
    return m_visible;
}

void TrayIcon::setIcon( HICON icon ) {
    m_nid.hIcon = icon;
    m_nid.uFlags = NIF_ICON;
    if ( m_visible )
        notify( NIM_MODIFY );
}

void TrayIcon::setTip( const std::string &tip ) {
    std::wstring wTip = utils::strToWiden( tip );
    wcsncpy_s( m_nid.szTip, _TRUNCATE, wTip.c_str(), wTip.size() );
    m_nid.uFlags = NIF_TIP;
    if ( m_visible )
        notify( NIM_MODIFY );
}

void TrayIcon::setOnClick( Callback callback ) {
    m_onClick = std::move( callback );
}

void TrayIcon::setOnDblClick( Callback callback ) {
    m_onDblClick = std::move( callback );
}

void TrayIcon::setOnRightClick( Callback callback ) {
    m_onRightClick = std::move( callback );
}

void TrayIcon::setMenuItems( const std::vector<MenuItem> &items ) {
    m_menuItems = items;
    rebuildMenu();
}

void TrayIcon::buildMenu( HMENU parent, const std::vector<MenuItem> &items, UINT_PTR &nextId ) {
    for ( auto &item : items ) {
        if ( item.separator ) {
            AppendMenuW( parent, MF_SEPARATOR, 0, nullptr );
            continue;
        }

        if ( !item.children.empty() ) {
            HMENU sub = CreatePopupMenu();
            buildMenu( sub, item.children, nextId );
            UINT flags = MF_POPUP;
            if ( item.disabled )
                flags |= MF_GRAYED;
            if ( item.barBreak )
                flags |= MF_MENUBARBREAK;
            AppendMenuW( parent, flags, reinterpret_cast<UINT_PTR>( sub ), utils::strToWiden( item.text ).c_str() );
            continue;
        }

        UINT flags = MF_STRING;
        if ( item.disabled )
            flags |= MF_GRAYED;
        if ( item.checked )
            flags |= MF_CHECKED;
        if ( item.bold )
            flags |= MF_DEFAULT;
        if ( item.barBreak )
            flags |= MF_MENUBARBREAK;

        UINT_PTR id = nextId++;
        AppendMenuW( parent, flags, id, utils::strToWiden( item.text ).c_str() );

        if ( item.callback )
            m_cmdMap[id] = item.callback;

        if ( item.radioCheck ) {
            MENUITEMINFOW mii = {};
            mii.cbSize = sizeof( mii );
            mii.fMask = MIIM_FTYPE;
            mii.fType = MFT_RADIOCHECK;
            SetMenuItemInfoW( parent, id, FALSE, &mii );
        }
    }
}

void TrayIcon::rebuildMenu() {
    if ( m_menu ) {
        DestroyMenu( m_menu );
        m_menu = nullptr;
    }
    m_cmdMap.clear();

    if ( m_menuItems.empty() )
        return;

    m_menu = CreatePopupMenu();
    UINT_PTR nextId = 1;
    buildMenu( m_menu, m_menuItems, nextId );
}

void TrayIcon::showContextMenu() {
    if ( !m_menu )
        return;

    POINT pt = {};
    GetCursorPos( &pt );
    SetForegroundWindow( m_msgHwnd );
    TrackPopupMenu( m_menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_msgHwnd, nullptr );
    PostMessageW( m_msgHwnd, WM_NULL, 0, 0 );
}

LRESULT CALLBACK TrayIcon::msgWndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp ) {
    if ( msg == WM_CREATE ) {
        auto *cs = reinterpret_cast<CREATESTRUCTW *>( lp );
        SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( cs->lpCreateParams ) );
        return 0;
    }

    auto *self = reinterpret_cast<TrayIcon *>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );

    if ( self && msg == self->m_msgId ) {
        switch ( LOWORD( lp ) ) {
        case WM_LBUTTONUP:
            if ( self->m_onClick )
                self->m_onClick( self );
            break;
        case WM_LBUTTONDBLCLK:
            if ( self->m_onDblClick )
                self->m_onDblClick( self );
            break;
        case WM_RBUTTONUP:
            if ( self->m_onRightClick )
                self->m_onRightClick( self );
            else
                self->showContextMenu();
            break;
        }
        return 0;
    }

    if ( msg == WM_COMMAND && self ) {
        UINT_PTR id = LOWORD( wp );
        auto it = self->m_cmdMap.find( id );
        if ( it != self->m_cmdMap.end() && it->second )
            it->second( self );
        return 0;
    }

    return DefWindowProcW( hwnd, msg, wp, lp );
}