(function () {
    'use strict';

    function nativeCall(name) {
        var args = Array.prototype.slice.call(arguments, 1);
        if (window.native && window.native[name]) return window.native[name].apply(null, args);
    }

    function onLayoutChange(source) {
        var el = document.querySelector('.placeholder');
        if (!el) return;
        var rect = el.getBoundingClientRect();
        var info = {
            source: source,
            x: Math.round(rect.x),
            y: Math.round(rect.y),
            width: Math.round(rect.width),
            height: Math.round(rect.height),
            pageWidth: window.innerWidth,
            pageHeight: window.innerHeight
        };
        console.log('[layout]', JSON.stringify(info));
        if (typeof window.onLayoutUpdate === 'function') {
            window.onLayoutUpdate(info);
        }
    }

    function setupLayoutWatcher() {
        var content = document.querySelector('.placeholder');
        if (!content) return;

        var resizeObserver = new ResizeObserver(function () {
            onLayoutChange('resize');
        });
        resizeObserver.observe(content);

        var lastRect = content.getBoundingClientRect();
        var positionTimer = null;

        function checkPosition() {
            var rect = content.getBoundingClientRect();
            if (Math.round(rect.x) !== Math.round(lastRect.x) ||
                Math.round(rect.y) !== Math.round(lastRect.y)) {
                lastRect = rect;
                onLayoutChange('position');
            } else {
                lastRect = rect;
            }
        }

        var mutationObserver = new MutationObserver(function () {
            if (positionTimer) clearTimeout(positionTimer);
            positionTimer = setTimeout(checkPosition, 16);
        });
        mutationObserver.observe(document.body, {
            childList: true,
            subtree: true,
            attributes: true,
            attributeFilter: ['style', 'class']
        });

        window.addEventListener('resize', function () {
            onLayoutChange('window');
        });

        onLayoutChange('init');
    }

    function setupWindowControls() {
        var EDGE = 10;
        var cursorMap = {
            n: 'n-resize', s: 's-resize', w: 'w-resize', e: 'e-resize',
            nw: 'nw-resize', ne: 'ne-resize', sw: 'sw-resize', se: 'se-resize'
        };

        function getResizeDir(x, y, w, h) {
            var top = y < EDGE, bottom = y > h - EDGE, left = x < EDGE, right = x > w - EDGE;
            if (top && left) return 'nw'; if (top && right) return 'ne';
            if (bottom && left) return 'sw'; if (bottom && right) return 'se';
            if (top) return 'n'; if (bottom) return 's'; if (left) return 'w'; if (right) return 'e';
            return null;
        }

        document.addEventListener('mousemove', function (e) {
            if (e.buttons !== 0) return;
            var dir = getResizeDir(e.clientX, e.clientY, window.innerWidth, window.innerHeight);
            document.body.style.cursor = dir ? cursorMap[dir] : '';
        });

        document.addEventListener('mousedown', function (e) {
            if (e.button !== 0) return;
            var dir = getResizeDir(e.clientX, e.clientY, window.innerWidth, window.innerHeight);
            if (dir) { e.preventDefault(); e.stopPropagation(); nativeCall('windowResize', dir); }
        }, true);

        var titlebar = document.querySelector('.titlebar');
        if (titlebar) {
            titlebar.addEventListener('mousedown', function (e) {
                if (e.target.closest('.titlebar-buttons') || e.target.closest('.theme-toggle')) return;
                if (e.button !== 0) return;
                if (getResizeDir(e.clientX, e.clientY, window.innerWidth, window.innerHeight)) return;
                nativeCall('windowDrag');
            });
        }

        var minBtn = document.querySelector('.titlebar-btn.minimize') || document.querySelector('.traffic-light.minimize');
        var maxBtn = document.querySelector('.titlebar-btn.maximize') || document.querySelector('.traffic-light.zoom');
        var closeBtn = document.querySelector('.titlebar-btn.close') || document.querySelector('.traffic-light.close');

        if (minBtn) minBtn.addEventListener('click', function () { nativeCall('windowMinimize'); });
        if (maxBtn) maxBtn.addEventListener('click', function () { nativeCall('windowMaximize'); });
        if (closeBtn) closeBtn.addEventListener('click', function () { nativeCall('windowClose'); });
    }

    function setupThemeToggle() {
        var btn = document.querySelector('.theme-toggle');
        if (!btn) return;

        var savedTheme = localStorage.getItem('window-color-theme');
        if (savedTheme) {
            document.documentElement.setAttribute('data-theme', savedTheme);
            btn.textContent = savedTheme === 'light' ? '☀️' : '🌙';
        }

        btn.addEventListener('click', function () {
            var html = document.documentElement;
            var isLight = html.getAttribute('data-theme') === 'light';
            var newTheme = isLight ? '' : 'light';
            html.setAttribute('data-theme', newTheme);
            btn.textContent = isLight ? '🌙' : '☀️';
            localStorage.setItem('window-color-theme', newTheme);
        });
    }

    window.WindowKit = {
        nativeCall: nativeCall,
        onLayoutChange: onLayoutChange,
        setup: function () {
            setupWindowControls();
            setupThemeToggle();
            setupLayoutWatcher();
        }
    };

    WindowKit.setup();
})();

(function () {
    var contentName = new URLSearchParams(window.location.search).get('contentName');
    if (contentName) {
        window.onLayoutUpdate = function (info) {
            var dpr = window.devicePixelRatio || 1;
            var px = Math.round(info.x * dpr);
            var py = Math.round(info.y * dpr);
            var pw = Math.round(info.width * dpr);
            var ph = Math.round(info.height * dpr);

            if (window.native && window.native.webviewSetRect) {
                window.native.webviewSetRect(contentName, px, py, pw, ph);
            }
        };

        if (window.native && window.native.webviewAdd) {
            window.native.webviewAdd(contentName, { x: 0, y: 0, width: 0, height: 0 });
        }
        WindowKit.onLayoutChange('init');
    } else {
        var el = document.querySelector('.placeholder');
        if (!el)
            return;
        el.innerHTML = '<iframe src="/index.html" frameborder="0" style="width: 100%; height: 100%;"></iframe>';

        // fetch('/index.html').then(async (res) => {
        //     if (res.status !== 200) {
        //         console.error('Failed to fetch index.html', res.status);
        //         return;
        //     }
        //     var html = await res.text();
        //     el.innerHTML = html;
        // });
    }
})();