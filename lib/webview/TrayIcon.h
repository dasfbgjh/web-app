#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <Windows.h>
#include <Shellapi.h>

class TrayIcon {
public:
    using Callback = std::function<void( TrayIcon *tray )>;

    struct MenuItem {
        std::string text;
        Callback callback;
        bool separator = false;
        bool disabled = false;
        bool checked = false;
        bool bold = false;
        bool radioCheck = false;
        bool barBreak = false;
        std::vector<MenuItem> children;
    };

    struct Config {
        HICON icon = nullptr;
        std::string tip;
        bool showOnCreate = true;
    };

    TrayIcon();
    ~TrayIcon();

    TrayIcon( const TrayIcon & ) = delete;
    TrayIcon &operator=( const TrayIcon & ) = delete;
    TrayIcon( TrayIcon && ) = delete;
    TrayIcon &operator=( TrayIcon && ) = delete;

    bool create( const Config &config );
    void destroy();

    void show();
    void hide();
    bool isVisible() const;

    void setIcon( HICON icon );
    void setTip( const std::string &tip );

    void setOnClick( Callback callback );
    void setOnDblClick( Callback callback );
    void setOnRightClick( Callback callback );

    void setMenuItems( const std::vector<MenuItem> &items );
    void showContextMenu();

private:
    static LRESULT CALLBACK msgWndProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
    void registerClass();
    bool createMessageWindow();
    void notify( DWORD action );
    void rebuildMenu();
    void buildMenu( HMENU parent, const std::vector<MenuItem> &items, UINT_PTR &nextId );

    HWND m_msgHwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    UINT m_msgId = 0;
    NOTIFYICONDATAW m_nid = {};
    bool m_visible = false;
    bool m_created = false;

    Callback m_onClick;
    Callback m_onDblClick;
    Callback m_onRightClick;
    std::vector<MenuItem> m_menuItems;
    HMENU m_menu = nullptr;
    std::unordered_map<UINT_PTR, Callback> m_cmdMap;
};