/**
 * Notification 内容块
 *
 * 在屏幕右下角堆叠显示通知，新消息将旧消息往上顶。
 * 最多同时显示 3 条，超出时移除最旧的。
 * 层次化视觉效果：最新的不透明且原始大小，旧的逐渐淡出缩小。
 *
 * 调用方式（C++ 端）：
 *   OverlayWebview::addBlock("ntf-1", "notification", 0, 0, "main",
 *     { title: "新消息", body: "你有一条未读消息", duration: 5000 });
 *
 * params 字段：
 *   title    {string}  通知标题
 *   body     {string}  通知内容
 *   duration {number}  显示时长（毫秒），默认 5000，0 表示不自动关闭
 *
 * IPC 回传：
 *   { type: "notification", id, action: "close" }   — 用户点击关闭按钮
 *   { type: "notification", id, action: "timeout" } — 超时自动关闭
 */
(function () {
    var order = [];
    var MAX = 3;

    function positionAll() {
        requestAnimationFrame(function () {
            var vw = window.innerWidth;
            var vh = window.innerHeight;
            var total = order.length;
            var bottom = vh - 20;
            for (var i = total - 1; i >= 0; i--) {
                var el = OverlayHost.blocks.get(order[i]);
                if (!el) continue;
                var rect = el.getBoundingClientRect();
                el.style.left = (vw - rect.width - 20) + 'px';
                el.style.top = (bottom - rect.height) + 'px';
                el.style.opacity = (0.5 + 0.5 * ((i + 1) / total)).toFixed(2);
                el.style.transform = 'scale(' + (0.92 + 0.08 * ((i + 1) / total)).toFixed(3) + ')';
                el.style.transformOrigin = 'bottom right';
                bottom -= rect.height + 8;
            }
            OverlayHost.scheduleRgnUpdate();
        });
    }

    function create(id, x, y, sourceWindow, params) {
        while (order.length >= MAX) {
            var oldId = order.shift();
            OverlayHost.removeBlockInternal(oldId);
        }

        var el = document.createElement('div');
        el.className = 'block-notification';
        el.innerHTML =
            '<span class="ntf-close" data-id="' + id + '">&times;</span>' +
            '<div class="ntf-title">' + (params.title || '') + '</div>' +
            '<div class="ntf-body">' + (params.body || '') + '</div>';
        OverlayHost.container.appendChild(el);
        OverlayHost.blocks.set(id, el);
        order.push(id);
        positionAll();

        el.querySelector('.ntf-close').addEventListener('click', function (e) {
            e.stopPropagation();
            window.native.overlayBlockResult(sourceWindow, { type: 'notification', id: id, action: 'close' });
            OverlayHost.removeBlockInternal(id);
            if (order.length > 0) positionAll();
            else OverlayHost.scheduleRgnUpdate();
        });

        var duration = params.duration || 5000;
        if (duration > 0) {
            OverlayHost.timers.set(id, setTimeout(function () {
                window.native.overlayBlockResult(sourceWindow, { type: 'notification', id: id, action: 'timeout' });
                OverlayHost.removeBlockInternal(id);
                if (order.length > 0) positionAll();
                else OverlayHost.scheduleRgnUpdate();
            }, duration));
        }
    }

    function onRemove(id) {
        var idx = order.indexOf(id);
        if (idx !== -1) order.splice(idx, 1);
    }

    OverlayHost.register('notification', {
        create: create,
        onRemove: onRemove,
        isInteractive: true
    });
})();