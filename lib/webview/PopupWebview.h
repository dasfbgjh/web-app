#pragma once
#include "webview/WebviewWidget.h"

class PopupWebview : public WebviewWidget {
public:
    /**
     * @brief 显示弹窗
     *
     * PopupWebview 为单例全屏透明覆盖层，同一时间只显示一个弹窗。
     * 通过 executeScript 调用 HTML 端 showPopup(type, x, y, params) 切换弹窗类型，毫秒级响应。
     * 点击弹窗或空白处调用 hidePopup(result) 隐藏窗口，结果通过 IPC 发回 sourceWindow。
     * 首次调用时若 WebView 未就绪，会自动延迟到就绪后执行。
     *
     * 各类型的 params 字段说明见 static/html/popup/<type>/script.js
     *
     * @param type 弹窗类型，对应 popup 目录下的子文件夹名
     * @param x 弹窗参考 X 屏幕坐标
     * @param y 弹窗参考 Y 屏幕坐标
     * @param sourceWindow 来源 WebView 名称，用于 IPC 回传结果
     * @param params 弹窗参数，各类型不同
     */
    static void show( const std::string &type, int x, int y, const std::string &sourceWindow, const json::value &params );

private:
    PopupWebview();
    ~PopupWebview();

    static PopupWebview &getInstance();

    void doShow();

    std::string m_sourceWindow;
    bool m_ready = false;

    std::string m_pendingType;
    int m_pendingX = 0;
    int m_pendingY = 0;
    std::string m_pendingSourceWindow;
    json::value m_pendingParams;
    bool m_hasPending = false;
};