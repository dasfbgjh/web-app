/**
 * Tooltip 内容块
 *
 * 在指定位置显示一段简短提示文本，超时后自动消失。
 * 定位到 (x, y) 坐标，自动修正越界。
 *
 * 调用方式（C++ 端）：
 *   OverlayWebview::addBlock("tip-1", "tooltip", x, y, "main", { text: "复制成功", duration: 3000 });
 *
 * params 字段：
 *   text     {string}  提示文本
 *   duration {number}  显示时长（毫秒），默认 3000，0 表示不自动关闭
 *
 * IPC 回传：
 *   { type: "tooltip", id, action: "timeout" }
 */
(function () {
    function create(id, x, y, sourceWindow, params) {
        const el = document.createElement('div');
        el.className = 'block-tooltip';
        el.textContent = params.text || '';
        OverlayHost.container.appendChild(el);
        OverlayHost.blocks.set(id, el);
        OverlayHost.positionAt(el, x, y);

        const duration = params.duration || 3000;
        if (duration > 0) {
            OverlayHost.timers.set(id, setTimeout(() => {
                window.native.overlayBlockResult(sourceWindow, { type: 'tooltip', id: id, action: 'timeout' });
                OverlayHost.removeBlockInternal(id);
                OverlayHost.scheduleRgnUpdate();
            }, duration));
        }
    }

    OverlayHost.register('tooltip', {
        create: create,
        isInteractive: false
    });
})();