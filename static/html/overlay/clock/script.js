/**
 * Clock 内容块
 *
 * 在指定位置显示一个实时时钟，每秒更新时间和日期。
 * 可选 12/24 小时制，可选显示秒。
 *
 * 调用方式（C++ 端）：
 *   OverlayWebview::addBlock("clock-1", "clock", x, y, "main",
 *     { hour12: false, showSeconds: true });
 *
 * params 字段：
 *   hour12       {boolean}  是否使用 12 小时制，默认 false（24 小时制）
 *   showSeconds  {boolean}  是否显示秒，默认 true
 *
 * IPC 回传：
 *   { type: "clock", id, action: "close" } — 用户点击关闭
 */
(function () {
    var tickIntervals = new Map();

    function updateDisplay(el, params) {
        var now = new Date();
        var hours = now.getHours();
        var minutes = now.getMinutes();
        var seconds = now.getSeconds();
        var suffix = '';

        if (params.hour12) {
            suffix = hours >= 12 ? ' PM' : ' AM';
            hours = hours % 12;
            if (hours === 0) hours = 12;
        }

        var timeStr = pad(hours) + ':' + pad(minutes);
        if (params.showSeconds !== false) {
            timeStr += ':' + pad(seconds);
        }
        timeStr += suffix;

        var year = now.getFullYear();
        var month = now.getMonth() + 1;
        var day = now.getDate();
        var weekdays = ['日', '一', '二', '三', '四', '五', '六'];
        var weekday = weekdays[now.getDay()];
        var dateStr = year + '/' + pad(month) + '/' + pad(day) + ' 周' + weekday;

        el.querySelector('.clk-time').textContent = timeStr;
        el.querySelector('.clk-date').textContent = dateStr;
    }

    function pad(n) {
        return n < 10 ? '0' + n : '' + n;
    }

    function create(id, x, y, sourceWindow, params) {
        params = params || {};

        var el = document.createElement('div');
        el.className = 'block-clock';
        el.innerHTML =
            '<div class="clk-time"></div>' +
            '<div class="clk-date"></div>';
        OverlayHost.container.appendChild(el);
        OverlayHost.blocks.set(id, el);

        updateDisplay(el, params);
        OverlayHost.positionAt(el, x, y);

        var intervalId = setInterval(function () {
            if (!OverlayHost.blocks.has(id)) {
                clearInterval(intervalId);
                tickIntervals.delete(id);
                return;
            }
            updateDisplay(el, params);
        }, 1000);
        tickIntervals.set(id, intervalId);
    }

    function onRemove(id) {
        if (tickIntervals.has(id)) {
            clearInterval(tickIntervals.get(id));
            tickIntervals.delete(id);
        }
    }

    OverlayHost.register('clock', {
        create: create,
        onRemove: onRemove,
        isInteractive: false
    });
})();