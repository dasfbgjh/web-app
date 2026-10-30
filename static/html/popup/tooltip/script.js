/**
 * Tooltip 弹窗
 *
 * 在指定位置显示一段简短提示文本，点击空白处关闭。
 *
 * 调用方式（C++ 端）：
 *   PopupWebview::show("tooltip", x, y, "main", { text: "复制成功" });
 *
 * params 字段：
 *   text {string}  提示文本
 *
 * IPC 回传：
 *   { type: "tooltip", id: null }  — 点击空白处或 Escape 关闭
 */
(function () {
    var el = document.createElement('div');
    el.className = 'tooltip';
    PopupHost.container.appendChild(el);

    function show(x, y, params) {
        el.textContent = params.text || '';
        el.style.display = 'block';
        PopupHost.showOverlay();
        PopupHost.positionAt(el, x, y);
    }

    function hide() {
        el.style.display = 'none';
    }

    PopupHost.register('tooltip', { show: show, hide: hide });
})();