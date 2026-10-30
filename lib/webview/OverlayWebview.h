#pragma once
#include "webview/WebviewWidget.h"

class OverlayWebview : public WebviewWidget {
public:
    /**
     * @brief 添加一个内容块到覆盖层
     *
     * OverlayWebview 为单例最大化透明覆盖层，可同时显示多个独立内容块。
     * 每个内容块有唯一 id，由 HTML 端管理其渲染和布局。
     * 内容块变更后 HTML 端自动计算所有可见块的联合区域，通过 windowSetRgn 裁剪窗口，
     * 使空白区域鼠标穿透到下层窗口。
     *
     * 各类型的 params 字段说明见 static/html/overlay/<type>/script.js
     *
     * @param id 内容块唯一标识，同 id 会替换已有内容
     * @param type 内容块类型，对应 overlay 目录下的子文件夹名
     * @param x 内容块参考 X 屏幕坐标
     * @param y 内容块参考 Y 屏幕坐标
     * @param sourceWindow 来源 WebView 名称，用于 IPC 回传结果
     * @param params 内容块参数，各类型不同
     */
    static void addBlock( const std::string &id, const std::string &type, int x, int y,
                          const std::string &sourceWindow, const json::value &params );

    /**
     * @brief 移除指定内容块
     * @param id 内容块唯一标识
     */
    static void removeBlock( const std::string &id );

    /**
     * @brief 移除所有内容块
     */
    static void clearBlocks();

protected:
    void onNavigationCompleted( bool isSuccess, WebView * );

private:
    OverlayWebview();
    ~OverlayWebview();

    static OverlayWebview &getInstance();

    void doAddBlock( const std::string &id, const std::string &type, int x, int y,
                     const std::string &sourceWindow, const json::value &params );
    void doRemoveBlock( const std::string &id );
    void doClearBlocks();
    void ensureVisible();

    bool m_ready = false;
    bool m_visible = false;

    struct PendingBlock {
        std::string id;
        std::string type;
        int x = 0;
        int y = 0;
        std::string sourceWindow;
        json::value params;
    };
    std::vector<PendingBlock> m_pendingBlocks;
    std::vector<std::string> m_pendingRemoves;
    bool m_pendingClear = false;
};