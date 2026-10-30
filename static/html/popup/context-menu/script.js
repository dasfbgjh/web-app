/**
 * Context Menu 弹窗
 *
 * 在指定位置显示右键菜单，点击菜单项后关闭并回传结果。
 * 点击空白处或按 Escape 关闭菜单。
 *
 * 调用方式（C++ 端）：
 *   PopupWebview::show("context-menu", x, y, "main",
 *     { items: [{id:"copy",label:"复制",icon:"📋",shortcut:"Ctrl+C"}, {separator:true}, ...] });
 *
 * params 字段：
 *   items {Array<Object>}  菜单项列表，每项字段：
 *     id        {string}   菜单项标识（点击时回传）
 *     label     {string}   显示文本
 *     icon      {string}   可选，图标（emoji 或文字）
 *     shortcut  {string}   可选，快捷键提示
 *     disabled  {boolean}  可选，是否禁用
 *     separator {boolean}  可选，是否为分隔线（此时其他字段无效）
 *
 * IPC 回传：
 *   { type: "context-menu", id, label }  — 点击菜单项
 *   { type: "context-menu", id: null }   — 点击空白处或 Escape 关闭
 */
(function () {
    var el = document.createElement('div');
    el.className = 'context-menu';
    PopupHost.container.appendChild(el);

    function show(x, y, params) {
        var items = params.items || [];
        el.innerHTML = '';

        for (var i = 0; i < items.length; i++) {
            var item = items[i];
            if (item.separator) {
                var sep = document.createElement('div');
                sep.className = 'menu-separator';
                el.appendChild(sep);
                continue;
            }

            var row = document.createElement('div');
            row.className = 'menu-item' + (item.disabled ? ' disabled' : '');
            row.innerHTML =
                (item.icon ? '<span class="icon">' + item.icon + '</span>' : '') +
                '<span class="label">' + (item.label || '') + '</span>' +
                (item.shortcut ? '<span class="shortcut">' + item.shortcut + '</span>' : '');

            if (!item.disabled) {
                (function (itm) {
                    row.addEventListener('click', function (e) {
                        e.stopPropagation();
                        PopupHost.hideAll();
                        window.native.hidePopup({ type: 'context-menu', id: itm.id, label: itm.label });
                    });
                })(item);
            }

            el.appendChild(row);
        }

        el.style.display = 'block';
        PopupHost.showOverlay();
        PopupHost.positionAt(el, x, y);
    }

    function hide() {
        el.style.display = 'none';
    }

    PopupHost.register('context-menu', { show: show, hide: hide });
})();