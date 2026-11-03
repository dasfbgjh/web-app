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

    /**
     * @brief 子 WebView 创建前回调
     * @param config 即将生效的配置，可直接修改
     * @param subWebview 即将创建的子 WebView 实例
     */
    using SubWebviewCreateCallback = std::function<void( WebviewWidget::Config &config, WebviewWidget *subWebview )>;

    /**
     * @brief 子 WebView 创建完成回调
     * @param success 是否创建成功
     * @param subWebview 已创建的子 WebView 实例
     */
    using SubWebviewCreateCompleteCallback = std::function<void( bool success, WebviewWidget *subWebview )>;

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

    /**
     * @brief 设置子 WebView 创建前回调，传空则取消
     * 在子 WebView 配置就绪、真正创建之前触发，可修改配置。
     */
    void setSubWebviewCreateCallback( SubWebviewCreateCallback callback );

    /**
     * @brief 设置子 WebView 创建完成回调，传空则取消
     */
    void setSubWebviewCreateCompleteCallback( SubWebviewCreateCompleteCallback callback );

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
    SubWebviewCreateCallback m_subWebviewCreateCallback;
    SubWebviewCreateCompleteCallback m_subWebviewCreateCompleteCallback;
};