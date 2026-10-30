/**
 * Executor 内容块
 *
 * 在覆盖层显示一个可输入 WebView 名称和 JS 代码的执行器，
 * 点击运行后通过 OverlayHost.executeScript 在目标实例上执行代码并显示结果。
 *
 * 调用方式（C++ 端）：
 *   OverlayWebview::addBlock("exe-1", "executor", x, y, "main",
 *     { defaultName: "main", defaultScript: "document.title" });
 *
 * params 字段：
 *   defaultName    {string}  预填的 WebView 实例名称，默认 ""
 *   defaultScript  {string}  预填的 JS 代码，默认 ""
 */
(function () {
    if (!OverlayHost.executeScript) {
        OverlayHost._execSeqId = 0;
        OverlayHost._execResolvers = new Map();

        OverlayHost.executeScript = function (name, script) {
            var seqId = ++OverlayHost._execSeqId;
            return new Promise(async function (resolve) {
                OverlayHost._execResolvers.set(seqId, resolve);
                var found = await window.native.executeScript(name, script, seqId);
                if (!found || !found.found) {
                    OverlayHost._execResolvers.delete(seqId);
                    resolve({ found: false });
                }
            });
        };

        window.chrome.webview.addEventListener('message', function (e) {
            var msg = e.data;
            if (msg && msg.type === 'executeScriptResult') {
                var resolver = OverlayHost._execResolvers.get(msg.seqId);
                if (resolver) {
                    OverlayHost._execResolvers.delete(msg.seqId);
                    resolver({ found: true, result: msg.result });
                }
            }
        });
    }

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
        params = params || {};

        var el = document.createElement('div');
        el.className = 'block-executor';
        el.innerHTML =
            '<div class="exe-title">Script Executor</div>' +
            '<div class="exe-row">' +
            '<label>Name</label>' +
            '<input type="text" class="exe-name" value="' + escAttr(params.defaultName || '') + '" placeholder="WebView name">' +
            '</div>' +
            '<textarea class="exe-code" placeholder="JavaScript code...">' + escHtml(params.defaultScript || '') + '</textarea>' +
            '<div class="exe-actions">' +
            '<button class="exe-btn exe-btn-close">Close</button>' +
            '<button class="exe-btn exe-btn-devtools">DevTools</button>' +
            '<button class="exe-btn exe-btn-run">Run</button>' +
            '</div>' +
            '<div class="exe-result"></div>' +
            '<div class="exe-status"></div>';
        OverlayHost.container.appendChild(el);
        OverlayHost.blocks.set(id, el);

        var resultEl = el.querySelector('.exe-result');
        var statusEl = el.querySelector('.exe-status');

        el.querySelector('.exe-btn-run').addEventListener('click', function () {
            var name = el.querySelector('.exe-name').value.trim();
            var code = el.querySelector('.exe-code').value;
            if (!name) {
                resultEl.textContent = 'Please enter a WebView name';
                resultEl.className = 'exe-result exe-error';
                return;
            }
            if (!code.trim()) {
                resultEl.textContent = 'Please enter JavaScript code';
                resultEl.className = 'exe-result exe-error';
                return;
            }
            resultEl.textContent = '';
            resultEl.className = 'exe-result';
            statusEl.textContent = 'Running on "' + name + '"...';

            OverlayHost.executeScript(name, code).then(function (res) {
                if (!res.found) {
                    resultEl.textContent = 'WebView "' + name + '" not found';
                    resultEl.className = 'exe-result exe-error';
                    statusEl.textContent = 'Failed';
                    return;
                }
                var output = res.result;
                if (typeof output === 'object' && output !== null) {
                    try { output = JSON.stringify(output, null, 2); } catch (e) { output = String(output); }
                }
                resultEl.textContent = output !== undefined && output !== null ? String(output) : 'undefined';
                resultEl.className = 'exe-result';
                statusEl.textContent = 'Done';
            });
        });

        el.querySelector('.exe-btn-devtools').addEventListener('click', function () {
            var name = el.querySelector('.exe-name').value.trim();
            if (!name) {
                statusEl.textContent = 'Please enter a WebView name';
                return;
            }
            if (window.native && window.native.openDevTools) {
                window.native.openDevTools(name).then(function (result) {
                    statusEl.textContent = result ? 'DevTools opened for "' + name + '"' : 'WebView "' + name + '" not found';
                });
            } else {
                statusEl.textContent = 'openDevTools not available';
            }
        });

        el.querySelector('.exe-btn-close').addEventListener('click', function () {
            OverlayHost.removeBlockInternal(id);
            OverlayHost.scheduleRgnUpdate();
            if (sourceWindow) {
                window.native.overlayBlockResult(sourceWindow, {
                    type: 'executor', id: id, action: 'close'
                });
            }
        });

        OverlayHost.positionAt(el, x, y);

        makeDraggable(el);
        makeResizable(el);
    }

    function onRemove(id) {
    }

    function escHtml(s) {
        return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }

    function escAttr(s) {
        return s.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }

    OverlayHost.register('executor', {
        create: create,
        onRemove: onRemove,
        isInteractive: true
    });
})();