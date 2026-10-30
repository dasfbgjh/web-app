#pragma once

#include <string>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <Windows.h>
#include <windowsx.h>
#include "common/utils.h"

class Widget {
public:
    /**
     * @brief 窗口消息处理回调
     *
     * 在默认虚函数处理之前调用。
     * 返回 true 表示已处理该消息（跳过默认虚函数处理），
     * 返回 false 表示未处理（继续执行默认虚函数处理）。
     *
     * @param hwnd 窗口句柄
     * @param msg 消息 ID
     * @param wParam WPARAM
     * @param lParam LPARAM
     * @return true 已处理，false 未处理
     */
    using MsgHandler = std::function<bool( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )>;

    struct Config {
        std::string className;                     ///< 窗口类名，为空则自动生成
        std::string title;                         ///< 窗口标题
        int x = CW_USEDEFAULT;                     ///< 窗口 X 坐标
        int y = CW_USEDEFAULT;                     ///< 窗口 Y 坐标
        int width = CW_USEDEFAULT;                 ///< 窗口宽度
        int height = CW_USEDEFAULT;                ///< 窗口高度
        DWORD style = WS_OVERLAPPEDWINDOW;         ///< 窗口风格，默认为标准可调整大小窗口
        DWORD exStyle = 0;                         ///< 扩展窗口风格
        UINT classStyle = CS_HREDRAW | CS_VREDRAW; ///< 窗口类风格
        HICON icon = nullptr;                      ///< 窗口图标，nullptr 则使用默认
        HICON iconSmall = nullptr;                 ///< 窗口小图标
        HCURSOR cursor = nullptr;                  ///< 鼠标光标，nullptr 则使用默认箭头
        HBRUSH background = nullptr;               ///< 背景画刷，nullptr 则使用默认
        HWND parent = nullptr;                     ///< 父窗口句柄，非空时作为子控件
        HMENU menu = nullptr;                      ///< 菜单句柄（子控件时作为控件 ID）
        bool center = true;                        ///< 是否居中显示
        bool topMost = false;                      ///< 是否置顶
        bool noBorder = false;                     ///< 无边框（设置后 style 中移除 WS_OVERLAPPEDWINDOW 并加 WS_POPUP）
        bool resizable = true;                     ///< 是否可调整大小
        bool hasTitleBar = true;                   ///< 是否有标题栏
        bool hasSysMenu = true;                    ///< 是否有系统菜单（关闭/最小化/最大化按钮）
        bool hasMinButton = true;                  ///< 是否有最小化按钮
        bool hasMaxButton = true;                  ///< 是否有最大化按钮
        bool hasCloseButton = true;                ///< 是否有关闭按钮（仅影响 Config 逻辑，Win32 不支持单独移除关闭按钮）
        bool clipChildren = true;                  ///< 是否裁剪子窗口区域（WebView2 需要此标志）
        bool clipSiblings = true;                  ///< 是否裁剪兄弟窗口区域
        bool visible = true;                       ///< 创建后是否可见
        BYTE opacity = 255;                        ///< 窗口不透明度，0=全透明，255=不透明（小于255时自动启用分层窗口）
        bool transparentBackground = false;        ///< 背景是否透明，为 true 时不绘制背景（子控件时添加 WS_EX_TRANSPARENT）
        int borderRadius = 0;                      ///< 窗口圆角半径（像素），0=无圆角；顶级窗口需配合 noBorder=true + transparentBackground=true，子控件通过窗口区域裁剪实现
        bool clickThrough = false;                 ///< 鼠标穿透，鼠标事件透传到下层窗口
        bool noTaskbar = false;                    ///< 不在任务栏显示图标（设置 WS_EX_TOOLWINDOW）
    };

    static json::value configToJson( const Config &config );
    static Config configFromJson( const json::value &value );

    Widget();
    virtual ~Widget();

    Widget( const Widget & ) = delete;
    Widget &operator=( const Widget & ) = delete;
    Widget( Widget && ) = delete;
    Widget &operator=( Widget && ) = delete;

    /**
     * @brief 创建控件
     *
     * parent 为空时创建独立窗口，非空时创建子控件（自动添加 WS_CHILD | WS_VISIBLE）
     *
     * @param config 控件配置项
     * @return true 表示创建成功
     */
    bool create( const Config &config );

    /**
     * @brief 关闭控件（隐藏窗口并设置关闭标志）
     *
     * 窗口不会被销毁，仅隐藏并标记为已关闭。
     * 实际销毁在析构时进行。可通过 setVisible(true) 重新显示窗口并清除关闭标志。
     */
    void close();

    /**
     * @brief 将窗口提升到 Z 序顶部（单次置顶）
     *
     * 顶级窗口/子控件均使用 HWND_TOP，一次性提到最前，
     * 其他窗口激活后仍可覆盖。始终置顶请用 setTopMost(true)。
     * 同时将所有子窗口提升到顶部。
     */
    void bringToTop();

    /**
     * @brief 将窗口降到 Z 序底部
     *
     * 顶级窗口：取消 TOPMOST 并置于所有非置顶窗口底部
     * 子控件：置于兄弟窗口底部（HWND_BOTTOM）
     * 同时将所有子窗口降到底部
     */
    void bringToBottom();

    /**
     * @brief 改变窗口 Z 序
     * @param bottom 在下的窗口
     * @param top 在上的窗口
     * @return true 表示成功
     */
    static bool changeZOrder( Widget *bottom, Widget *top );

    /**
     * @brief 设置鼠标穿透
     * @param enable true=鼠标事件透传到下层窗口，false=正常接收
     */
    void setClickThrough( bool enable );

    /**
     * @brief 是否鼠标穿透
     */
    bool isClickThrough() const;

    /**
     * @brief 设置窗口裁剪区域
     *
     * 区域外的部分不可见、不绘制、鼠标穿透到下层窗口。
     * 调用后区域归系统所有，不要手动 DeleteObject。
     * 传入 nullptr 移除裁剪区域，恢复为完整矩形窗口。
     *
     * @param rgn 裁剪区域句柄，nullptr 表示移除裁剪
     * @param repaint 是否立即重绘，默认 true
     */
    void setWindowRgn( HRGN rgn, bool repaint = true );

    /**
     * @brief 获取窗口句柄
     * @return 窗口句柄，未创建时返回 nullptr
     */
    HWND getHwnd() const;

    /**
     * @brief 是否为子控件
     * @return true 表示是子控件（parent 非空）
     */
    bool isChild() const;

    /**
     * @brief 设置窗口消息处理回调
     *
     * 回调在默认虚函数之前调用。返回 true 表示已处理（跳过默认），
     * 返回 false 表示未处理（继续执行默认虚函数）。
     *
     * @param handler 消息处理回调
     */
    void setMessageHandler( MsgHandler handler );

    void setTitle( const std::string &title );
    std::string getTitle() const;
    void setSize( int width, int height, bool repaint = true );
    void getSize( int &width, int &height ) const;
    void getClientSize( int &width, int &height ) const;
    void setPosition( int x, int y );
    void getPosition( int &x, int &y ) const;
    void setRect( int x, int y, int width, int height );
    void center();
    void setTopMost( bool topMost );
    void setFullscreen( bool fullscreen );
    bool isFullscreen() const;
    void maximize();
    void minimize();
    void restore();
    void setVisible( bool visible );
    bool isVisible() const;
    bool isMaximized() const;
    bool isMinimized() const;
    void setMinSize( int minWidth, int minHeight );
    void setMaxSize( int maxWidth, int maxHeight );
    void setOpacity( BYTE opacity );
    BYTE getOpacity() const;

protected:
    void updateWindowRgn();

    /**
     * @brief 消息分发虚函数
     *
     * 将 Windows 消息解析后分发到对应的 onXxx 虚函数。
     * 可重写以处理自定义消息。
     *
     * @param msg 消息 ID
     * @param wp WPARAM
     * @param lp LPARAM
     * @return 消息处理结果
     */
    virtual LRESULT wndProc( UINT msg, WPARAM wp, LPARAM lp );

    /**
     * @brief 调用默认窗口过程
     */
    LRESULT defWndProc( UINT msg, WPARAM wp, LPARAM lp ) const;

    // ── 窗口生命周期 ──
    virtual LRESULT onCreate( CREATESTRUCTW *cs );
    virtual LRESULT onClose();
    virtual LRESULT onDestroy();
    virtual LRESULT onSize( int width, int height, int type );
    virtual LRESULT onMove( int x, int y );

    // ── 绘制 ──
    virtual LRESULT onPaint();
    virtual LRESULT onEraseBkgnd( HDC hdc );

    // ── 鼠标 ──
    enum class MouseButton {
        Left,
        Middle,
        Right
    };
    virtual LRESULT onMouseMove( int x, int y, UINT flags );
    virtual LRESULT onMouseDown( MouseButton button, int x, int y, UINT flags );
    virtual LRESULT onMouseUp( MouseButton button, int x, int y, UINT flags );
    virtual LRESULT onMouseDblClk( MouseButton button, int x, int y, UINT flags );
    virtual LRESULT onMouseWheel( int x, int y, int delta, UINT flags );

    // ── 键盘 ──
    virtual LRESULT onKeyDown( int keyCode, UINT flags );
    virtual LRESULT onKeyUp( int keyCode, UINT flags );
    virtual LRESULT onChar( int charCode, UINT flags );
    virtual LRESULT onSysKeyDown( int keyCode, UINT flags );
    virtual LRESULT onSysKeyUp( int keyCode, UINT flags );

    // ── 焦点 / 激活 ──
    virtual LRESULT onSetFocus( HWND oldFocus );
    virtual LRESULT onKillFocus( HWND newFocus );
    virtual LRESULT onActivate( int state, HWND otherWnd, bool minimized );

    // ── 尺寸约束 ──
    virtual LRESULT onGetMinMaxInfo( MINMAXINFO *mmi );

    // ── 非客户区命中测试 ──
    virtual LRESULT onNcHitTest( int x, int y );

private:
    HICON defaultIcon();

    /**
     * @brief 更新窗口逻辑可见状态及 s_windowCount 计数
     *
     * 仅在状态实际变化时更新：
     *   - 变为可见：m_visible=true，非子窗口 s_windowCount++
     *   - 变为不可见：m_visible=false，非子窗口 s_windowCount-- 并检查退出
     *
     * @param visible 目标可见状态
     */
    void updateVisibleState( bool visible );

    static LRESULT CALLBACK staticWndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );

    HWND m_hwnd = nullptr;                // 窗口句柄
    HWND m_parent = nullptr;              // 父窗口句柄
    HINSTANCE m_instance = nullptr;       // 应用实例句柄
    std::wstring m_className;             // 窗口类名
    MsgHandler m_msgHandler;              // 消息处理函数
    bool m_fullscreen = false;            // 是否全屏
    DWORD m_savedStyle = 0;               // 保存的窗口样式
    DWORD m_savedExStyle = 0;             // 保存的窗口扩展样式
    RECT m_savedRect = {};                // 保存的窗口矩形
    int m_minWidth = 0;                   // 最小宽度
    int m_minHeight = 0;                  // 最小高度
    int m_maxWidth = 0;                   // 最大宽度
    int m_maxHeight = 0;                  // 最大高度
    bool m_classRegistered = false;       // 是否注册窗口类
    BYTE m_opacity = 255;                 // 窗口透明度
    bool m_noBorder = false;              // 是否无边框
    bool m_transparentBackground = false; // 是否透明背景
    int m_borderRadius = 0;               // 边框圆角半径
    bool m_clickThrough = false;          // 是否点击穿透
    bool m_visible = false;               // 窗口逻辑可见状态（影响 s_windowCount 计数）
};