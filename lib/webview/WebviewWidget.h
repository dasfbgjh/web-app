#pragma once

#include "Widget.h"
#include "WebView.h"

class WebviewWidget : public Widget, public WebView {
public:
    using CreatedCallback = std::function<void( bool success, WebviewWidget *ptr )>;

    struct Config {
        Widget::Config window;
        WebView::Config webview;
        Widget *targetWindow = nullptr; // 目标窗口，JS操作窗口将作用于该窗口，默认为当前窗口
    };

    static json::value configToJson( const Config &config );
    static Config configFromJson( const json::value &value );

    WebviewWidget();
    ~WebviewWidget() override;

    WebviewWidget( const WebviewWidget & ) = delete;
    WebviewWidget &operator=( const WebviewWidget & ) = delete;
    WebviewWidget( WebviewWidget && ) = delete;
    WebviewWidget &operator=( WebviewWidget && ) = delete;

    bool create( const Config &config, CreatedCallback onCreated = nullptr );
    void close();

    /**
     * 设置窗口透明时是否自动设置webview透明背景，仅在create前调用生效
     * @param transparent {boolean} true=自动设置webview透明背景，false=不自动设置webview透明背景，默认true
     */
    void setAutoWebviewTransparent( bool transparent );

    void setWebViewMargin( int left, int top, int right, int bottom );

protected:
    LRESULT onSize( int width, int height, int type ) override;
    LRESULT onDestroy() override;

    virtual void onWebviewCreate( WebView::Config &config );
    virtual void onWebviewCreateComplete( bool success );

private:
    void resizeWebView();
    void bindWindowMethods();

    /**
     * JS: await window.native.windowClose()
     * 关闭应用并退出消息循环
     */
    json::value onClose( const json::array &args );

    /**
     * JS: await window.native.windowMinimize()
     * 最小化窗口
     */
    json::value onMinimize( const json::array &args );

    /**
     * JS: await window.native.windowMaximize()
     * 最大化窗口，若当前为全屏则先还原再最大化
     */
    json::value onMaximize( const json::array &args );

    /**
     * JS: await window.native.windowDrag()
     * 开始拖拽窗口，在鼠标左键松开前自动跟随鼠标移动窗口位置
     * 通常绑定到自定义标题栏的 mousedown 事件
     */
    json::value onDrag( const json::array &args );

    /**
     * JS: await window.native.windowSetTopMost(topMost)
     * @param topMost {boolean} true=置顶，false=取消置顶
     * 设置窗口是否始终位于其他窗口之上
     */
    json::value onSetTopMost( const json::array &args );

    /**
     * JS: const pos = await window.native.windowGetPosition()
     * @returns {{ x: number, y: number }} 窗口左上角屏幕坐标
     * 获取窗口在屏幕上的位置
     */
    json::value onGetPosition( const json::array &args );

    /**
     * JS: const size = await window.native.windowGetSize()
     * @returns {{ width: number, height: number }} 窗口宽高（像素）
     * 获取窗口大小
     */
    json::value onGetSize( const json::array &args );

    /**
     * JS: await window.native.windowSetOpacity(opacity)
     * @param opacity {number} 不透明度，0=全透明，255=不透明
     * 设置窗口不透明度（小于255时自动启用分层窗口）
     */
    json::value onSetOpacity( const json::array &args );

    /**
     * JS: await window.native.windowResize(direction)
     * @param direction {string} 拖拽方向，由 'n','s','e','w' 组合表示窗口边缘
     *   - 'n' 上边, 's' 下边, 'e' 右边, 'w' 左边
     *   - 例: 'nw' 左上角, 'se' 右下角, 'e' 右边
     * 开始拖拽调整窗口大小，在鼠标左键松开前自动根据拖拽方向调整窗口尺寸
     * 全屏状态下不生效，最小宽度200px，最小高度100px
     */
    json::value onResize( const json::array &args );

    /**
     * JS: await window.native.windowSetRect(x, y, width, height)
     * @param x {number} 窗口左上角 X 屏幕坐标
     * @param y {number} 窗口左上角 Y 屏幕坐标
     * @param width {number} 窗口宽度（像素）
     * @param height {number} 窗口高度（像素）
     * 同时设置窗口位置和大小
     */
    json::value onSetRect( const json::array &args );

    /**
     * JS: await window.native.windowSetRgn(regions, clickThrough)
     * @param regions {Array} 区域配置数组，格式同 utils::createRgn
     *   传入空数组或 null 移除裁剪区域
     * @param clickThrough {boolean} 可选，当 regions 为空时是否启用鼠标穿透
     * 设置窗口裁剪区域，并控制鼠标穿透
     */
    json::value onSetWindowRgn( const json::array &args );

    /**
     * JS: await window.native.showPopup(type, x, y, params)
     * @param type {string} 弹窗类型: "context-menu" | "tooltip"
     * @param x {number} 弹窗参考 X 屏幕坐标
     * @param y {number} 弹窗参考 Y 屏幕坐标
     * @param params {Object} 弹窗参数，各类型不同
     * 显示弹窗，结果通过 ipc-message 事件发回
     *   window.addEventListener('ipc-message', e => {
     *       e.detail → { from: 'popup', data: { type, id, label, ... } }
     *   });
     */
    json::value onShowPopup( const json::array &args );

    /**
     * JS: await window.native.showOverlay(id, type, x, y, params)
     * @param id {string} 内容块唯一标识，同 id 替换已有内容
     * @param type {string} 内容块类型: "notification" | "tooltip" | "dialog"
     * @param x {number} 内容块参考 X 屏幕坐标
     * @param y {number} 内容块参考 Y 屏幕坐标
     * @param params {Object} 内容块参数
     * 在覆盖层添加一个内容块，结果通过 ipc-message 事件发回
     */
    json::value onShowOverlay( const json::array &args );

    /**
     * JS: await window.native.removeOverlay(id)
     * @param id {string} 内容块唯一标识
     * 从覆盖层移除指定内容块
     */
    json::value onRemoveOverlay( const json::array &args );

    /**
     * JS: await window.native.clearOverlay()
     * 移除覆盖层所有内容块
     */
    json::value onClearOverlay( const json::array &args );

    int m_marginLeft = 0;
    int m_marginTop = 0;
    int m_marginRight = 0;
    int m_marginBottom = 0;
    bool m_autoWebviewTransparent = true; // 窗口透明时是否自动设置webview透明背景
    Widget *m_targetWindow = nullptr;
};