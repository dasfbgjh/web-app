#pragma once

#include "Widget.h"
#include "WebviewWidget.h"
#include <functional>
#include <vector>
#include <memory>
#include <string>

class WebviewWindow : public Widget {
public:
    using CreatedCallback = std::function<void( bool success, WebviewWindow *ptr )>;

    struct Config {
        Widget::Config window;
        WebView::Config webview;
    };

    static json::value configToJson( const Config &config );
    static Config configFromJson( const json::value &value );

    WebviewWindow();
    ~WebviewWindow() override;

    WebviewWindow( const WebviewWindow & ) = delete;
    WebviewWindow &operator=( const WebviewWindow & ) = delete;
    WebviewWindow( WebviewWindow && ) = delete;
    WebviewWindow &operator=( WebviewWindow && ) = delete;

    bool create( const Config &config, CreatedCallback onCreated = nullptr );
    void close();

    WebviewWidget *getMainWebview();
    WebviewWidget *getSubWebview( const std::string &name );

    bool addSubWebview( const std::string &name, WebviewWidget::Config &config );
    bool removeSubWebview( const std::string &name );

protected:
    LRESULT onSize( int width, int height, int type ) override;
    LRESULT onDestroy() override;

    virtual void onMainWebviewCreate( WebviewWidget::Config &config, WebviewWidget *mainWebview );
    virtual void onMainWebviewCreateComplete( bool success, WebviewWidget *mainWebview );
    virtual void onSubWebviewCreate( WebviewWidget::Config &config, WebviewWidget *subWebview );
    virtual void onSubWebviewCreateComplete( bool success, WebviewWidget *subWebview );

private:
    WebviewWidget *findSubWebview( const std::string &name );
    const WebviewWidget *findSubWebview( const std::string &name ) const;
    void bindWebviewMethods();

    /**
     * @brief JS: window.native.webviewAdd("name", {x?, y?, width?, height?, url?, ...})
     * 配置项由 WebviewWidget::configFromJson 解析，缺失的位置和大小默认为 0
     */
    json::value onWebviewAdd( const json::array &args );

    /**
     * @brief JS: window.native.webviewRemove("name")
     * 移除指定子 WebView
     */
    json::value onWebviewRemove( const json::array &args );

    /**
     * @brief JS: window.native.webviewSetPosition("name", x, y)
     * 设置子 WebView 在父窗口中的位置
     */
    json::value onWebviewSetPosition( const json::array &args );

    /**
     * @brief JS: window.native.webviewSetSize("name", width, height)
     * 设置子 WebView 大小
     */
    json::value onWebviewSetSize( const json::array &args );

    /**
     * @brief JS: window.native.webviewSetRect("name", x, y, width, height)
     * 设置子 WebView 在父窗口中的位置和大小
     */
    json::value onWebviewSetRect( const json::array &args );

    /**
     * @brief JS: window.native.webviewGetNames()
     * 返回子 WebView 名称数组
     */
    json::value onWebviewGetNames( const json::array &args );

    /**
     * @brief JS: window.native.webviewGetCount()
     * 返回子 WebView 数量
     */
    json::value onWebviewGetCount( const json::array &args );

    /**
     * @brief JS: window.native.webviewChangeZOrder("bottom name", "top name")
     * 设置子 WebView 在父窗口中的 Z 值
     */
    json::value onWebviewChangeZOrder( const json::array &args );

    /**
     * @brief JS: window.native.webviewNavigate("name", "url")
     * 导航子 WebView 到指定 URL
     */
    json::value onWebviewNavigate( const json::array &args );

    /**
     * @brief JS: window.native.webviewExecuteScript("name", "script")
     * 在子 WebView 中执行 JavaScript
     */
    json::value onWebviewExecuteScript( const json::array &args );

    /**
     * @brief JS: window.native.webviewSetUserAgent("name", "userAgent")
     * 设置子 WebView 的 User-Agent
     */
    json::value onWebviewSetUserAgent( const json::array &args );

    /**
     * @brief JS: window.native.webviewOpenDevTools("name")
     * 打开子 WebView 的开发者工具窗口
     */
    json::value onWebviewOpenDevTools( const json::array &args );

    std::unique_ptr<WebviewWidget> m_mainWebview;
    std::vector<std::unique_ptr<WebviewWidget>> m_subWebviews;
};