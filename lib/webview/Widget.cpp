#include "Widget.h"
#include "EventLoop.h"
#include "common/utils.h"
#include "common/Logger.hpp"
#include "resource.h"

Widget::Widget() = default;

json::value Widget::configToJson( const Config &config ) {
    return json::object{
        { "className", config.className },
        { "title", config.title },
        { "x", config.x },
        { "y", config.y },
        { "width", config.width },
        { "height", config.height },
        { "style", static_cast<int64_t>( config.style ) },
        { "exStyle", static_cast<int64_t>( config.exStyle ) },
        { "classStyle", static_cast<int64_t>( config.classStyle ) },
        { "center", config.center },
        { "topMost", config.topMost },
        { "noBorder", config.noBorder },
        { "resizable", config.resizable },
        { "hasTitleBar", config.hasTitleBar },
        { "hasSysMenu", config.hasSysMenu },
        { "hasMinButton", config.hasMinButton },
        { "hasMaxButton", config.hasMaxButton },
        { "hasCloseButton", config.hasCloseButton },
        { "clipChildren", config.clipChildren },
        { "clipSiblings", config.clipSiblings },
        { "visible", config.visible },
        { "opacity", static_cast<int>( config.opacity ) },
        { "transparentBackground", config.transparentBackground },
        { "borderRadius", config.borderRadius },
        { "clickThrough", config.clickThrough },
        { "noTaskbar", config.noTaskbar },
    };
}

Widget::Config Widget::configFromJson( const json::value &value ) {
    Config cfg;
    if ( !value.is_object() )
        return cfg;

    const auto &obj = value.as_object();
    cfg.className = json::jsonGetString( obj, "className", cfg.className );
    cfg.title = json::jsonGetString( obj, "title", cfg.title );
    cfg.x = static_cast<int>( json::jsonGetInt( obj, "x", cfg.x ) );
    cfg.y = static_cast<int>( json::jsonGetInt( obj, "y", cfg.y ) );
    cfg.width = static_cast<int>( json::jsonGetInt( obj, "width", cfg.width ) );
    cfg.height = static_cast<int>( json::jsonGetInt( obj, "height", cfg.height ) );
    cfg.style = static_cast<DWORD>( json::jsonGetInt( obj, "style", cfg.style ) );
    cfg.exStyle = static_cast<DWORD>( json::jsonGetInt( obj, "exStyle", cfg.exStyle ) );
    cfg.classStyle = static_cast<UINT>( json::jsonGetInt( obj, "classStyle", cfg.classStyle ) );
    cfg.center = json::jsonGetBool( obj, "center", cfg.center );
    cfg.topMost = json::jsonGetBool( obj, "topMost", cfg.topMost );
    cfg.noBorder = json::jsonGetBool( obj, "noBorder", cfg.noBorder );
    cfg.resizable = json::jsonGetBool( obj, "resizable", cfg.resizable );
    cfg.hasTitleBar = json::jsonGetBool( obj, "hasTitleBar", cfg.hasTitleBar );
    cfg.hasSysMenu = json::jsonGetBool( obj, "hasSysMenu", cfg.hasSysMenu );
    cfg.hasMinButton = json::jsonGetBool( obj, "hasMinButton", cfg.hasMinButton );
    cfg.hasMaxButton = json::jsonGetBool( obj, "hasMaxButton", cfg.hasMaxButton );
    cfg.hasCloseButton = json::jsonGetBool( obj, "hasCloseButton", cfg.hasCloseButton );
    cfg.clipChildren = json::jsonGetBool( obj, "clipChildren", cfg.clipChildren );
    cfg.clipSiblings = json::jsonGetBool( obj, "clipSiblings", cfg.clipSiblings );
    cfg.visible = json::jsonGetBool( obj, "visible", cfg.visible );
    cfg.opacity = static_cast<BYTE>( json::jsonGetInt( obj, "opacity", cfg.opacity ) );
    cfg.transparentBackground = json::jsonGetBool( obj, "transparentBackground", cfg.transparentBackground );
    cfg.borderRadius = static_cast<int>( json::jsonGetInt( obj, "borderRadius", cfg.borderRadius ) );
    cfg.clickThrough = json::jsonGetBool( obj, "clickThrough", cfg.clickThrough );
    cfg.noTaskbar = json::jsonGetBool( obj, "noTaskbar", cfg.noTaskbar );
    return cfg;
}

Widget::~Widget() {
    if ( m_hwnd ) {
        if ( m_visible )
            setVisible( false );
        DestroyWindow( m_hwnd );
    }
    if ( m_classRegistered && m_instance ) {
        UnregisterClassW( m_className.c_str(), m_instance );
        m_classRegistered = false;
    }
}

bool Widget::create( const Config &config ) {
    if ( m_hwnd )
        return false;

    m_instance = GetModuleHandleW( nullptr );
    m_parent = config.parent;

    std::wstring wClassName;
    if ( config.className.empty() ) {
        static int counter = 0;
        wClassName = L"Widget_" + std::to_wstring( ++counter );
    } else {
        wClassName = utils::strToWiden( config.className );
    }
    m_className = wClassName;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof( wc );
    wc.style = config.classStyle;
    wc.lpfnWndProc = staticWndProc;
    wc.hInstance = m_instance;
    wc.hIcon = config.icon ? config.icon : defaultIcon();
    wc.hIconSm = config.iconSmall ? config.iconSmall : wc.hIcon;
    wc.hCursor = config.cursor ? config.cursor : LoadCursorW( nullptr, (LPCWSTR)IDC_ARROW );
    wc.hbrBackground = config.transparentBackground
                           ? nullptr
                           : ( config.background ? config.background : (HBRUSH)( COLOR_WINDOW + 1 ) );
    wc.lpszClassName = wClassName.c_str();

    if ( !RegisterClassExW( &wc ) ) {
        DWORD err = GetLastError();
        if ( err != 0x00000582 )
            return false;
    }
    m_classRegistered = true;

    DWORD style = config.style;
    DWORD exStyle = config.exStyle;

    if ( m_parent ) {
        style &= ~WS_OVERLAPPEDWINDOW;
        style |= WS_CHILD;
        if ( config.visible )
            style |= WS_VISIBLE;
    } else {
        if ( config.noBorder ) {
            style &= ~WS_OVERLAPPEDWINDOW;
            style |= WS_POPUP | WS_MINIMIZEBOX;
        } else {
            if ( !config.hasTitleBar )
                style &= ~WS_CAPTION;
            if ( !config.hasSysMenu )
                style &= ~WS_SYSMENU;
            if ( !config.hasMinButton )
                style &= ~WS_MINIMIZEBOX;
            if ( !config.hasMaxButton )
                style &= ~WS_MAXIMIZEBOX;
            if ( !config.resizable )
                style &= ~WS_THICKFRAME;
        }
    }

    if ( config.clipChildren )
        style |= WS_CLIPCHILDREN;
    if ( config.clipSiblings )
        style |= WS_CLIPSIBLINGS;

    if ( config.topMost )
        exStyle |= WS_EX_TOPMOST;

    if ( config.opacity < 255 )
        exStyle |= WS_EX_LAYERED;

    if ( config.transparentBackground && m_parent )
        exStyle |= WS_EX_TRANSPARENT;

    if ( config.clickThrough ) {
        exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
    }

    if ( config.noTaskbar )
        exStyle |= WS_EX_TOOLWINDOW;
    else if ( config.noBorder )
        exStyle |= WS_EX_APPWINDOW;

    m_noBorder = config.noBorder;
    m_transparentBackground = config.transparentBackground;
    m_borderRadius = config.borderRadius;
    m_clickThrough = config.clickThrough;

    std::wstring wTitle = utils::strToWiden( config.title );

    m_hwnd = CreateWindowExW(
        exStyle, wClassName.c_str(), wTitle.c_str(), style,
        config.x, config.y, config.width, config.height,
        m_parent, config.menu, m_instance, this );

    if ( !m_hwnd )
        return false;

    if ( config.opacity < 255 ) {
        m_opacity = config.opacity;
        SetLayeredWindowAttributes( m_hwnd, 0, m_opacity, LWA_ALPHA );
    } else if ( config.clickThrough ) {
        SetLayeredWindowAttributes( m_hwnd, 0, 255, LWA_ALPHA );
    }

    if ( !m_parent && config.center )
        center();

    updateWindowRgn();

    if ( !m_parent && config.visible )
        ShowWindow( m_hwnd, SW_SHOW );

    UpdateWindow( m_hwnd );

    updateVisibleState( config.visible );

    return true;
}

void Widget::bringToTop() {
    if ( !m_hwnd )
        return;
    SetWindowPos( m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
    EnumChildWindows( m_hwnd, []( HWND child, LPARAM ) -> BOOL {
        SetWindowPos( child, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
        return TRUE; }, 0 );
}

void Widget::bringToBottom() {
    if ( !m_hwnd )
        return;
    if ( m_parent ) {
        SetWindowPos( m_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
    } else {
        SetWindowPos( m_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
        SetWindowPos( m_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
    }
    EnumChildWindows( m_hwnd, []( HWND child, LPARAM ) -> BOOL {
        SetWindowPos( child, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
        return TRUE; }, 0 );
}

bool Widget::changeZOrder( Widget *bottom, Widget *top ) {
    if ( !bottom || !top )
        return false;
    HWND bottomHwnd = bottom->getHwnd();
    HWND topHwnd = top->getHwnd();
    if ( !bottomHwnd || !topHwnd )
        return false;

    SetWindowPos( bottomHwnd, topHwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
    return true;
}

void Widget::setClickThrough( bool enable ) {
    if ( !m_hwnd || m_clickThrough == enable )
        return;
    m_clickThrough = enable;
    LONG exStyle = GetWindowLong( m_hwnd, GWL_EXSTYLE );
    if ( enable ) {
        exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
        SetWindowLong( m_hwnd, GWL_EXSTYLE, exStyle );
        SetLayeredWindowAttributes( m_hwnd, 0, m_opacity, LWA_ALPHA );
    } else {
        exStyle &= ~( WS_EX_LAYERED | WS_EX_TRANSPARENT );
        SetWindowLong( m_hwnd, GWL_EXSTYLE, exStyle );
    }
    SetWindowPos( m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED );
}

bool Widget::isClickThrough() const {
    return m_clickThrough;
}

void Widget::setWindowRgn( HRGN rgn, bool repaint ) {
    if ( !m_hwnd )
        return;
    ::SetWindowRgn( m_hwnd, rgn, repaint ? TRUE : FALSE );
}

void Widget::close() {
    setVisible( false );
}

HWND Widget::getHwnd() const {
    return m_hwnd;
}

bool Widget::isChild() const {
    return m_parent != nullptr;
}

void Widget::setMessageHandler( MsgHandler handler ) {
    m_msgHandler = std::move( handler );
}

void Widget::setTitle( const std::string &title ) {
    if ( m_hwnd )
        SetWindowTextW( m_hwnd, utils::strToWiden( title ).c_str() );
}

std::string Widget::getTitle() const {
    if ( !m_hwnd )
        return {};
    int len = GetWindowTextLengthW( m_hwnd );
    if ( len == 0 )
        return {};
    std::wstring buf( len + 1, L'\0' );
    GetWindowTextW( m_hwnd, buf.data(), len + 1 );
    return utils::strToNarrow( buf );
}

void Widget::setSize( int width, int height, bool repaint ) {
    if ( !m_hwnd )
        return;
    RECT rc;
    if ( m_parent ) {
        GetWindowRect( m_hwnd, &rc );
        POINT pt = { rc.left, rc.top };
        ScreenToClient( m_parent, &pt );
        MoveWindow( m_hwnd, pt.x, pt.y, width, height, repaint );
    } else {
        GetWindowRect( m_hwnd, &rc );
        MoveWindow( m_hwnd, rc.left, rc.top, width, height, repaint );
    }
}

void Widget::getSize( int &width, int &height ) const {
    width = 0;
    height = 0;
    if ( !m_hwnd )
        return;
    RECT rc;
    GetWindowRect( m_hwnd, &rc );
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;
}

void Widget::getClientSize( int &width, int &height ) const {
    width = 0;
    height = 0;
    if ( !m_hwnd )
        return;
    RECT rc;
    GetClientRect( m_hwnd, &rc );
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;
}

void Widget::setPosition( int x, int y ) {
    if ( !m_hwnd )
        return;
    if ( m_parent ) {
        RECT rc;
        GetClientRect( m_hwnd, &rc );
        MoveWindow( m_hwnd, x, y, rc.right - rc.left, rc.bottom - rc.top, TRUE );
    } else {
        RECT rc;
        GetWindowRect( m_hwnd, &rc );
        MoveWindow( m_hwnd, x, y, rc.right - rc.left, rc.bottom - rc.top, TRUE );
    }
}

void Widget::setRect( int x, int y, int width, int height ) {
    if ( !m_hwnd )
        return;
    MoveWindow( m_hwnd, x, y, width, height, TRUE );
}

void Widget::getPosition( int &x, int &y ) const {
    x = 0;
    y = 0;
    if ( !m_hwnd )
        return;
    if ( m_parent ) {
        POINT pt = { 0, 0 };
        ClientToScreen( m_hwnd, &pt );
        x = pt.x;
        y = pt.y;
    } else {
        RECT rc;
        GetWindowRect( m_hwnd, &rc );
        x = rc.left;
        y = rc.top;
    }
}

void Widget::center() {
    if ( !m_hwnd || m_parent )
        return;
    RECT rc;
    GetWindowRect( m_hwnd, &rc );
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int screenW = GetSystemMetrics( SM_CXSCREEN );
    int screenH = GetSystemMetrics( SM_CYSCREEN );
    int x = ( screenW - w ) / 2;
    int y = ( screenH - h ) / 2;
    MoveWindow( m_hwnd, x, y, w, h, TRUE );
}

void Widget::setTopMost( bool topMost ) {
    if ( !m_hwnd )
        return;
    SetWindowPos( m_hwnd, topMost ? HWND_TOPMOST : HWND_NOTOPMOST,
                  0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
}

void Widget::setFullscreen( bool fullscreen ) {
    if ( !m_hwnd || m_parent || m_fullscreen == fullscreen )
        return;

    updateVisibleState( true );

    if ( fullscreen ) {
        GetWindowRect( m_hwnd, &m_savedRect );
        m_savedStyle = GetWindowLongW( m_hwnd, GWL_STYLE );
        m_savedExStyle = GetWindowLongW( m_hwnd, GWL_EXSTYLE );

        SetWindowRgn( m_hwnd, nullptr, FALSE );

        SetWindowLongW( m_hwnd, GWL_STYLE, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE );
        SetWindowLongW( m_hwnd, GWL_EXSTYLE, m_savedExStyle | WS_EX_TOPMOST );

        int screenW = GetSystemMetrics( SM_CXSCREEN );
        int screenH = GetSystemMetrics( SM_CYSCREEN );
        SetWindowPos( m_hwnd, HWND_TOPMOST, 0, 0, screenW, screenH, 0 );
    } else {
        SetWindowLongW( m_hwnd, GWL_STYLE, m_savedStyle );
        SetWindowLongW( m_hwnd, GWL_EXSTYLE, m_savedExStyle );
        SetWindowPos( m_hwnd, HWND_NOTOPMOST,
                      m_savedRect.left, m_savedRect.top,
                      m_savedRect.right - m_savedRect.left,
                      m_savedRect.bottom - m_savedRect.top,
                      0 );

        updateWindowRgn();
    }

    m_fullscreen = fullscreen;
}

bool Widget::isFullscreen() const {
    return m_fullscreen;
}

void Widget::maximize() {
    if ( !m_hwnd )
        return;
    updateVisibleState( true );
    ShowWindow( m_hwnd, SW_MAXIMIZE );
}

void Widget::minimize() {
    if ( !m_hwnd )
        return;
    updateVisibleState( true );
    ShowWindow( m_hwnd, SW_MINIMIZE );
}

void Widget::restore() {
    if ( !m_hwnd )
        return;
    updateVisibleState( true );
    ShowWindow( m_hwnd, SW_RESTORE );
}

void Widget::setVisible( bool visible ) {
    if ( !m_hwnd )
        return;
    updateVisibleState( visible );
    ShowWindow( m_hwnd, visible ? SW_SHOW : SW_HIDE );
}

bool Widget::isVisible() const {
    return IsWindowVisible( m_hwnd );
}

bool Widget::isMaximized() const {
    if ( !m_hwnd )
        return false;
    return IsZoomed( m_hwnd ) != 0;
}

bool Widget::isMinimized() const {
    if ( !m_hwnd )
        return false;
    return IsIconic( m_hwnd ) != 0;
}

void Widget::setMinSize( int minWidth, int minHeight ) {
    m_minWidth = minWidth;
    m_minHeight = minHeight;
}

void Widget::setMaxSize( int maxWidth, int maxHeight ) {
    m_maxWidth = maxWidth;
    m_maxHeight = maxHeight;
}

void Widget::setOpacity( BYTE opacity ) {
    m_opacity = opacity;
    if ( !m_hwnd )
        return;
    DWORD exStyle = GetWindowLongW( m_hwnd, GWL_EXSTYLE );
    if ( opacity < 255 ) {
        exStyle |= WS_EX_LAYERED;
        SetWindowLongW( m_hwnd, GWL_EXSTYLE, exStyle );
        SetLayeredWindowAttributes( m_hwnd, 0, m_opacity, LWA_ALPHA );
    } else {
        exStyle &= ~WS_EX_LAYERED;
        SetWindowLongW( m_hwnd, GWL_EXSTYLE, exStyle );
    }
}

BYTE Widget::getOpacity() const {
    return m_opacity;
}

void Widget::updateWindowRgn() {
    if ( !m_hwnd || m_fullscreen )
        return;
    if ( m_borderRadius > 0 ) {
        RECT rc;
        GetClientRect( m_hwnd, &rc );
        HRGN rgn = CreateRoundRectRgn( 0, 0, rc.right - rc.left + 1, rc.bottom - rc.top + 1,
                                       m_borderRadius * 2, m_borderRadius * 2 );
        SetWindowRgn( m_hwnd, rgn, TRUE );
    } else {
        SetWindowRgn( m_hwnd, nullptr, TRUE );
    }
}

// ══════════════════════════════════════════════════════════════════
//  消息分发与虚函数默认实现
// ══════════════════════════════════════════════════════════════════

LRESULT Widget::defWndProc( UINT msg, WPARAM wp, LPARAM lp ) const {
    return DefWindowProcW( m_hwnd, msg, wp, lp );
}

LRESULT Widget::wndProc( UINT msg, WPARAM wp, LPARAM lp ) {
    switch ( msg ) {

    // ── 窗口生命周期 ──
    case WM_CREATE:
        return onCreate( reinterpret_cast<CREATESTRUCTW *>( lp ) );
    case WM_CLOSE:
        return onClose();
    case WM_DESTROY:
        return onDestroy();
    case WM_SIZE:
        return onSize( LOWORD( lp ), HIWORD( lp ), static_cast<int>( wp ) );
    case WM_MOVE:
        return onMove( GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ) );

    // ── 绘制 ──
    case WM_PAINT:
        return onPaint();
    case WM_ERASEBKGND:
        return onEraseBkgnd( reinterpret_cast<HDC>( wp ) );

    // ── 鼠标 ──
    case WM_MOUSEMOVE:
        return onMouseMove( GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_LBUTTONDOWN:
        return onMouseDown( MouseButton::Left, GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_LBUTTONUP:
        return onMouseUp( MouseButton::Left, GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_LBUTTONDBLCLK:
        return onMouseDblClk( MouseButton::Left, GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_RBUTTONDOWN:
        return onMouseDown( MouseButton::Right, GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_RBUTTONUP:
        return onMouseUp( MouseButton::Right, GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_RBUTTONDBLCLK:
        return onMouseDblClk( MouseButton::Right, GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_MBUTTONDOWN:
        return onMouseDown( MouseButton::Middle, GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_MBUTTONUP:
        return onMouseUp( MouseButton::Middle, GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_MBUTTONDBLCLK:
        return onMouseDblClk( MouseButton::Middle, GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ), static_cast<UINT>( wp ) );
    case WM_MOUSEWHEEL:
        return onMouseWheel( GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ),
                             GET_WHEEL_DELTA_WPARAM( wp ), GET_KEYSTATE_WPARAM( wp ) );

    // ── 键盘 ──
    case WM_KEYDOWN:
        return onKeyDown( static_cast<int>( wp ), static_cast<UINT>( lp ) );
    case WM_KEYUP:
        return onKeyUp( static_cast<int>( wp ), static_cast<UINT>( lp ) );
    case WM_CHAR:
        return onChar( static_cast<int>( wp ), static_cast<UINT>( lp ) );
    case WM_SYSKEYDOWN:
        return onSysKeyDown( static_cast<int>( wp ), static_cast<UINT>( lp ) );
    case WM_SYSKEYUP:
        return onSysKeyUp( static_cast<int>( wp ), static_cast<UINT>( lp ) );

    // ── 焦点 / 激活 ──
    case WM_SETFOCUS:
        return onSetFocus( reinterpret_cast<HWND>( wp ) );
    case WM_KILLFOCUS:
        return onKillFocus( reinterpret_cast<HWND>( wp ) );
    case WM_ACTIVATE:
        return onActivate( LOWORD( wp ), reinterpret_cast<HWND>( lp ), HIWORD( wp ) != 0 );

    // ── 尺寸约束 ──
    case WM_GETMINMAXINFO:
        return onGetMinMaxInfo( reinterpret_cast<MINMAXINFO *>( lp ) );

    // ── 非客户区命中测试 ──
    case WM_NCHITTEST:
        return onNcHitTest( GET_X_LPARAM( lp ), GET_Y_LPARAM( lp ) );

    default:
        return defWndProc( msg, wp, lp );
    }
}

// ── 窗口生命周期默认实现 ──

LRESULT Widget::onCreate( CREATESTRUCTW * ) {
    return 0;
}

LRESULT Widget::onClose() {
    close();
    return 0;
}

LRESULT Widget::onDestroy() {
    m_hwnd = nullptr;
    if ( m_classRegistered && m_instance ) {
        UnregisterClassW( m_className.c_str(), m_instance );
        m_classRegistered = false;
    }
    updateVisibleState( false );
    return 0;
}

LRESULT Widget::onSize( int, int, int ) {
    updateWindowRgn();
    return defWndProc( WM_SIZE, 0, 0 );
}

LRESULT Widget::onMove( int, int ) {
    return defWndProc( WM_MOVE, 0, 0 );
}

// ── 绘制默认实现 ──

LRESULT Widget::onPaint() {
    PAINTSTRUCT ps;
    BeginPaint( m_hwnd, &ps );
    EndPaint( m_hwnd, &ps );
    return 0;
}

LRESULT Widget::onEraseBkgnd( HDC hdc ) {
    if ( m_transparentBackground )
        return TRUE;
    return defWndProc( WM_ERASEBKGND, reinterpret_cast<WPARAM>( hdc ), 0 );
}

// ── 鼠标默认实现 ──

LRESULT Widget::onMouseMove( int x, int y, UINT flags ) {
    return defWndProc( WM_MOUSEMOVE, flags, MAKELPARAM( x, y ) );
}

LRESULT Widget::onMouseDown( MouseButton button, int x, int y, UINT flags ) {
    SetCapture( m_hwnd );
    UINT msg = button == MouseButton::Left    ? WM_LBUTTONDOWN
               : button == MouseButton::Right ? WM_RBUTTONDOWN
                                              : WM_MBUTTONDOWN;
    return defWndProc( msg, flags, MAKELPARAM( x, y ) );
}

LRESULT Widget::onMouseUp( MouseButton button, int x, int y, UINT flags ) {
    ReleaseCapture();
    UINT msg = button == MouseButton::Left    ? WM_LBUTTONUP
               : button == MouseButton::Right ? WM_RBUTTONUP
                                              : WM_MBUTTONUP;
    return defWndProc( msg, flags, MAKELPARAM( x, y ) );
}

LRESULT Widget::onMouseDblClk( MouseButton button, int x, int y, UINT flags ) {
    UINT msg = button == MouseButton::Left    ? WM_LBUTTONDBLCLK
               : button == MouseButton::Right ? WM_RBUTTONDBLCLK
                                              : WM_MBUTTONDBLCLK;
    return defWndProc( msg, flags, MAKELPARAM( x, y ) );
}

LRESULT Widget::onMouseWheel( int x, int y, int delta, UINT flags ) {
    return defWndProc( WM_MOUSEWHEEL, MAKEWPARAM( flags, delta ), MAKELPARAM( x, y ) );
}

// ── 键盘默认实现 ──

LRESULT Widget::onKeyDown( int keyCode, UINT flags ) {
    return defWndProc( WM_KEYDOWN, static_cast<WPARAM>( keyCode ), static_cast<LPARAM>( flags ) );
}

LRESULT Widget::onKeyUp( int keyCode, UINT flags ) {
    return defWndProc( WM_KEYUP, static_cast<WPARAM>( keyCode ), static_cast<LPARAM>( flags ) );
}

LRESULT Widget::onChar( int charCode, UINT flags ) {
    return defWndProc( WM_CHAR, static_cast<WPARAM>( charCode ), static_cast<LPARAM>( flags ) );
}

LRESULT Widget::onSysKeyDown( int keyCode, UINT flags ) {
    return defWndProc( WM_SYSKEYDOWN, static_cast<WPARAM>( keyCode ), static_cast<LPARAM>( flags ) );
}

LRESULT Widget::onSysKeyUp( int keyCode, UINT flags ) {
    return defWndProc( WM_SYSKEYUP, static_cast<WPARAM>( keyCode ), static_cast<LPARAM>( flags ) );
}

// ── 焦点 / 激活默认实现 ──

LRESULT Widget::onSetFocus( HWND oldFocus ) {
    return defWndProc( WM_SETFOCUS, reinterpret_cast<WPARAM>( oldFocus ), 0 );
}

LRESULT Widget::onKillFocus( HWND newFocus ) {
    return defWndProc( WM_KILLFOCUS, reinterpret_cast<WPARAM>( newFocus ), 0 );
}

LRESULT Widget::onActivate( int state, HWND otherWnd, bool minimized ) {
    return defWndProc( WM_ACTIVATE, MAKEWPARAM( state, minimized ? 1 : 0 ), reinterpret_cast<LPARAM>( otherWnd ) );
}

// ── 尺寸约束默认实现 ──

LRESULT Widget::onGetMinMaxInfo( MINMAXINFO *mmi ) {
    if ( m_noBorder && !m_parent ) {
        HMONITOR monitor = MonitorFromWindow( m_hwnd, MONITOR_DEFAULTTONEAREST );
        if ( monitor ) {
            MONITORINFO mi = { sizeof( mi ) };
            GetMonitorInfoW( monitor, &mi );
            mmi->ptMaxPosition.x = mi.rcWork.left;
            mmi->ptMaxPosition.y = mi.rcWork.top;
            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
    }
    if ( m_minWidth > 0 )
        mmi->ptMinTrackSize.x = m_minWidth;
    if ( m_minHeight > 0 )
        mmi->ptMinTrackSize.y = m_minHeight;
    if ( m_maxWidth > 0 )
        mmi->ptMaxTrackSize.x = m_maxWidth;
    if ( m_maxHeight > 0 )
        mmi->ptMaxTrackSize.y = m_maxHeight;
    return 0;
}

// ── 非客户区命中测试默认实现 ──

LRESULT Widget::onNcHitTest( int x, int y ) {
    if ( m_clickThrough )
        return HTTRANSPARENT;
    return defWndProc( WM_NCHITTEST, 0, MAKELPARAM( x, y ) );
}

void Widget::updateVisibleState( bool visible ) {
    if ( visible && !m_visible ) {
        m_visible = true;
        if ( !m_parent )
            EventLoop::onWindowVisibleChanged( true );
    } else if ( !visible && m_visible ) {
        m_visible = false;
        if ( !m_parent )
            EventLoop::onWindowVisibleChanged( false );
    }
}

HICON Widget::defaultIcon() {
    const unsigned char *data;
    int size = resource_get( "/html/app/favicon.ico", &data );
    if ( size < 0 || !data )
        return LoadIconW( nullptr, (LPCWSTR)IDI_APPLICATION );
    return utils::iconFromMemory( data, size );
}

// ══════════════════════════════════════════════════════════════════
//  静态窗口过程
// ══════════════════════════════════════════════════════════════════

LRESULT CALLBACK Widget::staticWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
    Widget *self = nullptr;

    if ( msg == WM_NCCREATE ) {
        auto *cs = reinterpret_cast<CREATESTRUCTW *>( lParam );
        self = reinterpret_cast<Widget *>( cs->lpCreateParams );
        SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( self ) );
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<Widget *>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );
    }

    if ( !self )
        return DefWindowProcW( hwnd, msg, wParam, lParam );

    if ( msg == WM_DESTROY ) {
        self->m_hwnd = nullptr;
        SetWindowLongPtrW( hwnd, GWLP_USERDATA, 0 );
    }

    if ( self->m_msgHandler ) {
        if ( self->m_msgHandler( hwnd, msg, wParam, lParam ) )
            return 0;
    }

    return self->wndProc( msg, wParam, lParam );
}