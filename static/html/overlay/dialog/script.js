/**
 * Dialog 内容块
 *
 * 在屏幕居中（x=0, y=0）或指定位置显示对话框，包含标题、内容和按钮。
 * 最后一个按钮高亮为 primary 样式。
 * 点击按钮后通过 IPC 回传按钮 id，并自动关闭对话框。
 *
 * 调用方式（C++ 端）：
 *   OverlayWebview::addBlock("dlg-1", "dialog", 0, 0, "main",
 *     { title: "确认", content: "是否继续？",
 *       buttons: [{id:"cancel",label:"取消"}, {id:"ok",label:"确定"}] });
 *
 * params 字段：
 *   title   {string}                      对话框标题
 *   content {string}                      对话框内容
 *   buttons {Array<{id,label}>}           按钮列表，默认 [{id:"ok",label:"确定"}]
 *
 * IPC 回传：
 *   { type: "dialog", id, action: "button", buttonId }
 */
(function () {
    function makeDraggable(el) {
        var handle = document.createElement('div');
        handle.className = 'block-drag-handle';
        el.appendChild(handle);

        handle.addEventListener('mousedown', function (e) {
            if (e.button !== 0) return;
            e.preventDefault();
            var startX = e.clientX;
            var startY = e.clientY;
            var startLeft = el.offsetLeft;
            var startTop = el.offsetTop;

            function onMove(e) {
                var dx = e.clientX - startX;
                var dy = e.clientY - startY;
                var newLeft = startLeft + dx;
                var newTop = startTop + dy;
                var vw = window.innerWidth;
                var vh = window.innerHeight;
                if (newLeft < 0) newLeft = 0;
                if (newTop < 0) newTop = 0;
                if (newLeft + el.offsetWidth > vw) newLeft = vw - el.offsetWidth;
                if (newTop + el.offsetHeight > vh) newTop = vh - el.offsetHeight;
                el.style.left = newLeft + 'px';
                el.style.top = newTop + 'px';
                OverlayHost.scheduleRgnUpdate();
            }

            function onUp() {
                document.removeEventListener('mousemove', onMove);
                document.removeEventListener('mouseup', onUp);
            }

            document.addEventListener('mousemove', onMove);
            document.addEventListener('mouseup', onUp);
        });
    }

    function makeResizable(el) {
        var handle = document.createElement('div');
        handle.className = 'block-resize-handle';
        el.appendChild(handle);

        handle.addEventListener('mousedown', function (e) {
            if (e.button !== 0) return;
            e.preventDefault();
            e.stopPropagation();
            var startX = e.clientX;
            var startY = e.clientY;
            var startW = el.offsetWidth;
            var startH = el.offsetHeight;
            var minW = parseInt(el.style.minWidth) || 80;
            var minH = parseInt(el.style.minHeight) || 40;

            function onMove(e) {
                var dx = e.clientX - startX;
                var dy = e.clientY - startY;
                var newW = Math.max(minW, startW + dx);
                var newH = Math.max(minH, startH + dy);
                el.style.width = newW + 'px';
                el.style.height = newH + 'px';
                OverlayHost.scheduleRgnUpdate();
            }

            function onUp() {
                document.removeEventListener('mousemove', onMove);
                document.removeEventListener('mouseup', onUp);
            }

            document.addEventListener('mousemove', onMove);
            document.addEventListener('mouseup', onUp);
        });
    }

    function create(id, x, y, sourceWindow, params) {
        var el = document.createElement('div');
        el.className = 'block-dialog';

        var buttons = params.buttons || [{ id: 'ok', label: '确定' }];
        var btnHtml = '<div class="dlg-buttons">';
        for (var i = 0; i < buttons.length; i++) {
            var cls = i === buttons.length - 1 ? ' dlg-btn primary' : ' dlg-btn';
            btnHtml += '<button class="' + cls + '" data-btn-id="' + buttons[i].id + '">' + buttons[i].label + '</button>';
        }
        btnHtml += '</div>';

        el.innerHTML =
            '<div class="dlg-title">' + (params.title || '') + '</div>' +
            '<div class="dlg-content">' + (params.content || '') + '</div>' +
            btnHtml;
        OverlayHost.container.appendChild(el);
        OverlayHost.blocks.set(id, el);

        if (x === 0 && y === 0) {
            requestAnimationFrame(function () {
                var rect = el.getBoundingClientRect();
                var vw = window.innerWidth;
                var vh = window.innerHeight;
                el.style.left = ((vw - rect.width) / 2) + 'px';
                el.style.top = ((vh - rect.height) / 2) + 'px';
                OverlayHost.scheduleRgnUpdate();
            });
        } else {
            OverlayHost.positionAt(el, x, y);
        }

        makeDraggable(el);
        makeResizable(el);

        el.querySelectorAll('.dlg-btn').forEach(function (btn) {
            btn.addEventListener('click', function (e) {
                e.stopPropagation();
                var btnId = btn.getAttribute('data-btn-id');
                window.native.overlayBlockResult(sourceWindow, { type: 'dialog', id: id, action: 'button', buttonId: btnId });
                OverlayHost.removeBlockInternal(id);
                OverlayHost.scheduleRgnUpdate();
            });
        });
    }

    OverlayHost.register('dialog', {
        create: create,
        isInteractive: true
    });
})();