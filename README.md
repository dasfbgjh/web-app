# web-app

基于 **C++17 / CMake / WebView2** 构建的 Windows 桌面应用框架，封装了 Win32 原生窗口控件与 Microsoft Edge WebView2 浏览器内核，提供无边框透明窗口、系统托盘、覆盖层弹窗、内嵌资源管理、JS-C++ 互操作等能力，适用于构建现代风格的轻量级桌面应用。

## 项目结构

```
web-app/
├── CMakeLists.txt              # 主构建脚本
├── src/
│   ├── main.cpp                # 应用入口（当前调用 Test.h 中的 test1()）
│   └── Test.h                  # 示例代码：test1() 单 WebView 窗口、TestWindow 多 WebView 容器
├── lib/
│   ├── webview/
│   │   ├── Widget.h/cpp        # Win32 窗口基类，窗口创建、消息分发、圆角/穿透/置顶
│   │   ├── EventLoop.h/cpp     # 事件循环，支持嵌套、线程安全任务投递、定时器、拖拽监控
│   │   ├── WebView.h/cpp       # WebView2 核心封装，导航、JS 互调、资源拦截、IPC
│   │   ├── WebviewWidget.h/cpp # 单 WebView 窗口，绑定窗口控制 JS API
│   │   ├── WebviewWindow.h/cpp # 多 WebView 容器窗口，支持动态添加/移除子 WebView
│   │   ├── OverlayWebview.h/cpp # 覆盖层窗口（单例透明覆盖，多内容块，鼠标穿透裁剪）
│   │   ├── PopupWebview.h/cpp  # 弹窗窗口（单例透明覆盖，毫秒级弹窗切换）
│   │   ├── Dialog.h/cpp        # 模态对话框（基于 WebviewWidget + EventLoop，原生窗口 + WebView2 内容）
│   │   ├── TrayIcon.h/cpp      # 系统托盘图标，支持菜单、子菜单、事件回调
│   │   └── WebView2.h          # WebView2 SDK 头文件
│   ├── common/
│   │   ├── Utils.h/cpp         # 工具函数（UTF-8 转换、Content-Type 推断、进程管理、文件对话框）
│   │   └── Logger.hpp          # 日志系统（控制台/文件输出，多级别）
│   └── http/
│       ├── HttpServer.h/cpp    # HTTP 服务器（Boost.Beast，路由/中间件/静态文件）
│       └── HttpClient.h/cpp    # HTTP 客户端（异步/同步，可选 SSL）
├── resource/
│   ├── CMakeLists.txt          # 资源嵌入构建脚本
│   ├── src/
│   │   ├── generate.cpp        # 资源代码生成器（文件→C 数组）
│   │   └── resource.cpp        # 资源访问接口实现
│   └── include/
│       └── resource.h          # 资源访问接口
├── appInfo/
│   ├── AppInfo.cmake           # 应用信息配置（版本、图标、清单、RC 文件）
│   ├── config/                 # 模板文件（AppInfo.h.in、manifest、RC、图标）
│   ├── include/                # WinInfo.h、AppExtension.hpp
│   └── output/                 # CMake 配置生成的输出文件
├── static/
│   └── html/
│       ├── app/                # 应用页面（虚拟域名映射到 /html/app）
│       │   ├── index.html      # Popup / Overlay 演示页
│       │   ├── favicon.ico     # 应用图标
│       │   └── window/         # 无边框窗口皮肤：glass / macos / minimal / win11 + window.js
│       ├── overlay/            # 覆盖层内容块
│       │   ├── index.html      # 覆盖层主页面
│       │   ├── clock/          # 时钟组件
│       │   ├── dialog/         # 对话框组件
│       │   ├── executor/       # 执行器组件（在 WebView 中执行 JS）
│       │   ├── notification/   # 通知组件
│       │   └── tooltip/        # 提示框组件
│       ├── popup/              # 弹窗内容
│       │   ├── index.html      # 弹窗主页面
│       │   ├── context-menu/   # 右键菜单组件
│       │   └── tooltip/        # 提示框组件
│       └── TimerHooker.js      # 定时器钩子脚本
├── bin/                        # 构建输出目录（同时是运行时目录）
└── build/                      # CMake 构建缓存
```

> `bin/config/EBWebView/` 是 WebView2 运行时生成的用户数据目录（Cache、Cookies、Local Storage 等），不属于源码，可安全删除以重置浏览器状态。

## 核心模块

### Widget — Win32 窗口基类

封装 Win32 窗口创建与消息分发（**不含消息循环**，循环由 `EventLoop` 负责），提供：

- **窗口配置**：无边框、透明背景、圆角、置顶、鼠标穿透、隐藏任务栏图标等
- **窗口操作**：`minimize()` / `maximize()` / `restore()` / `setFullscreen()` / `setTopMost()` / `setOpacity()` / `setRect()` / `center()`
- **Z 序**：`bringToTop()` / `bringToBottom()` / 静态 `changeZOrder(bottom, top)`
- **形状与穿透**：`setWindowRgn(HRGN)` 裁剪区域、`setClickThrough(bool)` 鼠标穿透
- **消息分发**：按消息类型分发到虚函数 `onCreate` / `onSize` / `onMouseDown` / `onKeyDown` / `onNcHitTest` 等，`setMessageHandler()` 可在默认处理前插入自定义逻辑
- **窗口计数**：可见状态变化时通知 `EventLoop`，用于「最后一个顶级窗口关闭后自动退出」

### EventLoop — 事件循环

独立于 Widget 的事件循环管理，支持：

- **嵌套消息循环**：可在已运行的循环中创建新实例，`exit()` 仅退出当前层，`quit()` 退出所有
- **线程安全任务投递**：`postEvent()` 从任意线程向循环投递待执行任务
- **定时器**：`startTimer()` / `stopTimer()`，基于 `SetTimer` / `KillTimer`
- **拖拽监控**：`startDragMonitor()` / `stopDragMonitor()`，基于 `WH_MOUSE_LL` 全局钩子
- **消息过滤**：`setMessageHandler()` 自定义消息处理函数
- **自动退出**：`exec(quitOnLastWindow = true)` 在最后一个顶级窗口关闭时自动退出；`Widget` 可见状态变化会同步内部窗口计数

### WebView — WebView2 核心封装

对 Microsoft Edge WebView2 的完整封装：

- **生命周期**：自动尝试加载 `WebView2Loader.dll` 或内置 loader（从注册表查找 Runtime）
- **导航控制**：navigate / goBack / goForward / reload / stop / openDevToolsWindow
- **JS ↔ C++ 互操作**：
  - `executeScript()` — C++ 调用 JS（静态重载可按 WebView 名称跨实例执行）
  - `bind()` — 绑定 C++ 函数供 JS 通过 `window.native[name]()` 异步调用
  - `postWebMessage()` / `setWebMessageReceivedCallback()` — 双向消息通信
- **资源拦截**：`setVirtualHostNameForResources()` 将内嵌资源映射到虚拟域名
- **API 路由**：`addApiRoute()` 注册 C++ 端 API，JS 通过 `https://hostName/api/xxx` 访问
- **请求拦截**：`addWebResourceRequestedFilter()` 拦截并修改网络请求；`setWebResourceResponseReceivedCallback()` 只读监控响应
- **IPC 通信**：`setName()` + `sendIpcMessage()` 跨 WebView 实例通信
- **新窗口拦截**：`setNewWindowRequestedCallback()` 拦截 `target="_blank"` 等新窗口请求
- **控制台转发**：`setConsoleMessageCallback()` 将浏览器 console 输出转发到 C++
- **脚本注入**：`addScriptToExecuteOnDocumentCreated()` 在文档创建时自动注入 JS
- **其他**：`setUserAgent()` / `getUserAgent()`、`getRenderHwnd()` 获取渲染子窗口、`configToJson()` / `configFromJson()` 配置序列化

### WebviewWidget — 单 WebView 窗口

继承 `Widget` + `WebView`，自动绑定窗口控制 JS API：

| JS API | 功能 |
|--------|------|
| `window.native.windowClose()` | 关闭窗口（最后一个顶级窗口关闭后退出消息循环） |
| `window.native.windowMinimize()` | 最小化 |
| `window.native.windowMaximize()` | 最大化（已最大化则还原） |
| `window.native.windowDrag()` | 拖拽窗口 |
| `window.native.windowResize(dir)` | 拖拽调整大小（dir: n/s/e/w 组合） |
| `window.native.windowSetTopMost(bool)` | 置顶/取消置顶 |
| `window.native.windowGetPosition()` | 获取窗口位置 |
| `window.native.windowGetSize()` | 获取窗口大小 |
| `window.native.windowSetOpacity(int)` | 设置透明度 |
| `window.native.windowSetRect(x,y,w,h)` | 设置位置和大小 |
| `window.native.windowSetRgn(regions, clickThrough?)` | 设置窗口裁剪区域（空白区域鼠标穿透） |
| `window.native.showPopup(type, x, y, params)` | 显示弹窗，结果经 `ipc-message` 回传 |
| `window.native.showOverlay(id, type, x, y, params)` | 在覆盖层添加内容块 |
| `window.native.removeOverlay(id)` | 移除指定覆盖层内容块 |
| `window.native.clearOverlay()` | 清空覆盖层所有内容块 |

> `Config.targetWindow` 可指定 JS 窗口操作实际作用的窗口（默认为自身），这是 `WebviewWindow` 让子 WebView 控制父窗口的基础。

### WebviewWindow — 多 WebView 容器窗口

在单个窗口中管理多个子 WebView 实例，提供动态添加/移除/布局/Z 序控制：

| JS API | 功能 |
|--------|------|
| `window.native.webviewAdd(name, {x?, y?, width?, height?, url?, ...})` | 添加子 WebView（第二个参数为 `WebviewWidget::Config` 字段对象） |
| `window.native.webviewRemove(name)` | 移除子 WebView |
| `window.native.webviewSetPosition(name, x, y)` | 设置位置 |
| `window.native.webviewSetSize(name, w, h)` | 设置大小 |
| `window.native.webviewSetRect(name, x, y, w, h)` | 设置位置和大小 |
| `window.native.webviewGetNames()` | 获取所有子 WebView 名称 |
| `window.native.webviewGetCount()` | 获取子 WebView 数量 |
| `window.native.webviewChangeZOrder(bottom, top)` | 调整 Z 序 |
| `window.native.webviewNavigate(name, url)` | 导航子 WebView 到指定 URL |
| `window.native.webviewExecuteScript(name, script)` | 在子 WebView 中执行 JS |
| `window.native.webviewSetUserAgent(name, ua)` | 设置子 WebView 的 User-Agent |
| `window.native.webviewOpenDevTools(name)` | 打开子 WebView 的 DevTools |

### OverlayWebview — 覆盖层窗口

单例透明覆盖层，可同时显示多个独立内容块。每个内容块有唯一 `id`，由 HTML 端管理其渲染和布局。内容块变更后 HTML 端自动计算所有可见块的联合区域，通过 `windowSetRgn` 裁剪窗口，使空白区域鼠标穿透到下层窗口。

内部使用虚拟域名 `overlay.app` 映射到 `/html/overlay`，创建时窗口为 1×1 且不可见，首次添加内容块时才 `maximize()` 并显示；内容块全部清空后窗口重新隐藏。

```cpp
// 添加内容块
OverlayWebview::addBlock("executor-1", "executor", x, y, "source-window",
    json::object({ { "defaultName", "player-webview" }, { "defaultScript", "document.title" } }));

// 移除内容块
OverlayWebview::removeBlock("executor-1");

// 清除所有内容块
OverlayWebview::clearBlocks();
```

内置覆盖层组件：

| 组件 | 目录 | 说明 |
|------|------|------|
| Clock | `overlay/clock/` | 时钟显示 |
| Dialog | `overlay/dialog/` | 对话框 |
| Executor | `overlay/executor/` | 在指定 WebView 中执行 JS 脚本 |
| Notification | `overlay/notification/` | 通知消息 |
| Tooltip | `overlay/tooltip/` | 提示框 |

### PopupWebview — 弹窗窗口

单例透明覆盖层，同一时间只显示一个弹窗。通过 `executeScript` 调用 HTML 端 `showPopup(type, x, y, params)` 切换弹窗类型，毫秒级响应。点击弹窗或空白处调用 `hidePopup(result)` 隐藏窗口，结果通过 IPC 发回 `sourceWindow`。

内部使用虚拟域名 `popup.app` 映射到 `/html/popup`，创建时窗口为 1×1 且不可见，`show()` 时 `maximize()` 后显示；HTML 未就绪的首次调用会进入 pending 队列，加载完成后自动执行。

```cpp
// 显示弹窗
PopupWebview::show("context-menu", x, y, "source-window",
    json::object({ { "items", json::array{ "复制", "粘贴", "删除" } } }));
```

内置弹窗组件：

| 组件 | 目录 | 说明 |
|------|------|------|
| Context Menu | `popup/context-menu/` | 右键上下文菜单 |
| Tooltip | `popup/tooltip/` | 提示框 |

### Dialog — 模态对话框

基于 `WebviewWidget` + `EventLoop` 实现的模态对话框。**窗口默认使用标准 Win32 窗口样式**（标题栏、系统菜单、关闭按钮，可通过 `Config` 自定义），**内容区域嵌入 WebView2** 支持 JS ↔ C++ 互操作：

- **原生窗口**：继承 `WebviewWidget`，通过 `GWLP_HWNDPARENT` 设置 owner 窗口（非 `WS_CHILD` 子控件）
- **模态阻塞**：`exec(parent)` 进入嵌套消息循环阻塞当前线程，禁用 owner 窗口，拦截 owner 消息防止用户操作
- **消息拦截**：过滤 owner 窗口的 `WM_CLOSE`（阻止关闭）、`WM_NCHITTEST` / `WM_LBUTTONDOWN` / `WM_RBUTTONDOWN` / `WM_MOUSEACTIVATE`（阻止激活，将焦点拉回对话框）
- **退出恢复**：`exit(code)` 退出消息循环并关闭窗口，自动恢复 owner 窗口 `EnableWindow(TRUE)` + `SetForegroundWindow`
- **关闭按钮**：点击标题栏关闭按钮触发 `onClose()` → `exit(0)`，等效于用户取消
- **返回值**：`exec()` 返回退出码；未 `create()` 或 owner 无效时返回 `-1` / `-2` / `-3`

```cpp
Dialog d;
WebviewWidget::Config cfg;
cfg.window.width = 600;
cfg.window.height = 400;
d.create(cfg, [](bool success, WebviewWidget *wv) {
    wv->setVirtualHostNameForResources("dialog.app", "/html/app");
    wv->navigate(wv->getResourceUrl("/index.html"));
});
int code = d.exec(this);  // 传入 owner Widget，阻塞直到对话框关闭
```

### TrayIcon — 系统托盘

- 创建/销毁托盘图标，设置图标和提示文字
- 左键/右键/双击事件回调
- 右键上下文菜单，支持子菜单、分隔线、禁用、勾选、加粗、单选等

### 文件对话框 — Win32 原生弹窗

封装 Win32 原生文件选择/保存/文件夹选择对话框，所有路径使用 UTF-8 编码。参数均有默认值，可按需传参：

```cpp
// 打开文件选择（单选）
std::vector<std::string> openFileDialog(
    HWND owner = nullptr,
    const std::string &title = "选择文件",
    const std::string &defaultDir = ".",
    const std::string &defaultFilename = "",
    bool multiSelect = false,
    const std::vector<std::pair<std::string, std::string>> &filters = {} );

// 保存文件
std::string saveFileDialog(
    HWND owner = nullptr,
    const std::string &title = "保存文件",
    const std::string &defaultDir = ".",
    const std::string &defaultFilename = "",
    const std::vector<std::pair<std::string, std::string>> &filters = {} );

// 选择文件夹（Vista+ IFileDialog）
std::string selectFolderDialog(
    HWND owner = nullptr,
    const std::string &title = "选择文件夹",
    const std::string &defaultDir = "." );
```

- `filters` 每项为 `{显示名, 规格}`，如 `{"Image Files", "*.png;*.jpg"}`，为空则默认 "All Files (*.*)"
- `multiSelect` 仅 `openFileDialog` 有效，多选时返回所有选中文件路径

```cpp
// 打开文件（单选）
auto files = utils::openFileDialog( hwnd, "Select Image", ".", "", false,
    { {"Image Files", "*.png;*.jpg"}, {"All Files", "*.*"} } );

// 打开文件（多选）
auto files = utils::openFileDialog( hwnd, "选择文件", ".", "", true,
    { {"Text Files", "*.txt"} } );

// 保存文件
auto path = utils::saveFileDialog( hwnd, "Save As", ".", "untitled.txt",
    { {"Text Files", "*.txt"} } );

// 选择文件夹
auto folder = utils::selectFolderDialog( hwnd, "Select Folder" );
```

### Logger — 日志系统

多级别日志输出，支持控制台和文件双通道：

- **级别**：DEBUG / LOG / INFO / WARN / ERR
- **宏接口**：`LOG_DEBUG` / `LOG_LOG` / `LOG_INFO` / `LOG_WARN` / `LOG_ERROR`
- **文件模式**：单文件 / 多文件 / 关闭
- **线程安全**：内部互斥锁保护

### 内嵌资源系统

构建时通过 `generate.cpp` 将 `static/` 目录下的所有文件编译为 C 静态数组，嵌入到可执行文件中，运行时通过 `resource.h` 接口按名称访问，实现单文件部署。

### HTTP 服务（可选）

通过 CMake 变量 `ADD_HTTP_SERVER=ON` 启用：

- **HttpServer**：基于 Boost.Beast 的异步 HTTP 服务器，支持路由、中间件、正则路径匹配、静态文件服务
- **HttpClient**：异步/同步 HTTP 客户端，支持 GET/POST/PUT/DELETE，可选 SSL（OpenSSL）

## 依赖

| 依赖 | 用途 |
|------|------|
| [Boost](https://www.boost.org/) (asio, process, filesystem, regex, json) | 异步 IO、进程管理、文件系统、JSON 解析 |
| [Boost.Beast](https://www.boost.org/)（仅 `ADD_HTTP_SERVER=ON` 时需要） | HTTP 服务端/客户端 |
| [WebView2](https://developer.microsoft.com/microsoft-edge/webview2/) | 浏览器内核（使用系统已安装的 Edge Runtime） |
| [OpenSSL](https://www.openssl.org/)（可选，配合 `ADD_HTTP_SERVER`） | HTTPS 客户端 SSL 支持 |

## 构建要求

- CMake ≥ 3.10
- C++17 编译器（MinGW GCC / MSVC）
- Boost 库（需编译安装，`find_package(... CONFIG)` 查找）
- Windows 操作系统
- Microsoft Edge WebView2 Runtime（运行时依赖）

## 构建

```bash
# 配置（Release 模式，生成 WIN32 GUI 应用）
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 构建
cmake --build build

# 可执行文件输出到 bin/ 目录
```

当前只有一个可执行目标 `test`，产物为 `bin/test.exe`。`CMAKE_BUILD_TYPE` 未指定（如多配置生成器默认空值）时按 Release 处理，即生成无控制台的 WIN32 GUI 程序。

### 可选：启用 HTTP 服务

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DADD_HTTP_SERVER=ON
```

### Debug 模式

Debug 模式下不设置 `WIN32` 入口（保留控制台窗口），并定义 `RESOURCE_PATH` 宏使资源从文件系统直接读取而非内嵌数组。

## 使用示例

### 示例 1：WebviewWindow（多 WebView 容器窗口）

```cpp
WebviewWindow w;
WebviewWindow::Config cfg;
cfg.window.title = "WebviewWindow Demo";
cfg.window.width = 1400;
cfg.window.height = 1000;
cfg.window.center = true;
cfg.window.resizable = true;
cfg.window.noBorder = true;
cfg.window.transparentBackground = true;

w.create(cfg, [](bool success, WebviewWindow *instance) {
    auto *wv = instance->getMainWebview();
    wv->setName("main-window");
    wv->setVirtualHostNameForResources("myapp.app", "/html/app");
    wv->navigate(wv->getResourceUrl("/index.html"));
});

EventLoop loop;
loop.exec();
```

### 示例 2：WebviewWidget（单 WebView 窗口）

```cpp
WebviewWidget w;
WebviewWidget::Config cfg;
cfg.window.title = "WebviewWidget Demo";
cfg.window.noBorder = true;
cfg.window.transparentBackground = true;

w.create(cfg, [](bool success, WebviewWidget *wv) {
    wv->setVirtualHostNameForResources("myapp.app", "/html/app");
    wv->navigate(wv->getResourceUrl("/index.html"));
});

EventLoop loop;
loop.exec();
```

### 示例 3：TrayIcon（系统托盘）

```cpp
TrayIcon tray;
TrayIcon::Config trayCfg;
trayCfg.icon = utils::iconFromFile("path/to/logo.ico");
trayCfg.tip = "My App";
tray.create(trayCfg);

tray.setOnDblClick([](TrayIcon *tray) { tray->showContextMenu(); });

tray.setMenuItems({
    { "显示", [](TrayIcon *tray) { tray->show(); } },
    { "", nullptr, true },  // 分隔线
    { "退出", [](TrayIcon *) { EventLoop::quit(); } }
});

EventLoop loop;
loop.exec();
```

### 示例 4：OverlayWebview（覆盖层）

```cpp
// 在屏幕 (x, y) 位置显示一个时钟覆盖层
OverlayWebview::addBlock("clock-1", "clock", x, y, "main-window",
    json::object({ { "hour12", false }, { "showSeconds", true } }));

// 移除覆盖层
OverlayWebview::removeBlock("clock-1");
```

### 示例 5：PopupWebview（弹窗）

```cpp
// 在 (x, y) 位置显示右键菜单
PopupWebview::show("context-menu", x, y, "main-window",
    json::object({ { "items", json::array{
        json::object({ { "id", "copy" }, { "label", "复制" }, { "shortcut", "Ctrl+C" } }),
        json::object({ { "id", "paste" }, { "label", "粘贴" }, { "shortcut", "Ctrl+V" } }),
        json::object({ { "separator", true } }),
        json::object({ { "id", "delete" }, { "label", "删除" }, { "shortcut", "Del" } })
    } } }));
```

各内置组件的 `params` 字段说明见对应目录下 `script.js` 顶部的注释（如 `overlay/clock/script.js`、`popup/context-menu/script.js`）。

## 注意事项

### WebView 创建前窗口必须可见

创建 `WebviewWidget` / `WebviewWindow` 时，`cfg.window.visible` 必须设为 `true`。WebView2 要求宿主窗口在创建浏览器控件时处于可见状态，否则 WebView 内容将无法渲染（窗口显示为空白）。如果需要窗口初始隐藏，应在 WebView 创建完成后再调用 `Widget::setVisible(false)` 隐藏窗口。

```cpp
// ✅ 正确：先创建可见窗口，回调中再隐藏
cfg.window.visible = true;
w.create(cfg, [](bool success, WebviewWidget *wv) {
    // WebView 已创建，此时可以安全隐藏
    wv->setVisible(false);
    wv->navigate(wv->getResourceUrl("/index.html"));
});

// ❌ 错误：窗口初始隐藏，WebView 不会渲染
cfg.window.visible = false;
w.create(cfg, [](bool success, WebviewWidget *wv) {
    wv->navigate(wv->getResourceUrl("/index.html"));
});
```

### 虚拟域名不要用 `.local` 后缀

`setVirtualHostNameForResources()` 的域名请避免 `xxx.local` 这类写法，否则资源加载会明显变慢（WebView2 会走额外的解析路径）。推荐使用 `xxx.app` 这类普通域名形式。

## 特性总结

- **单文件部署**：静态资源编译期嵌入可执行文件，无需外部文件依赖
- **无边框透明窗口**：支持圆角、透明背景、鼠标穿透、分层窗口
- **窗口区域裁剪**：`setWindowRgn` + 形状描述（矩形/圆角/椭圆/多边形/组合），实现异形窗口与空白穿透
- **JS ↔ C++ 双向通信**：原生函数绑定 + WebMessage + IPC 跨 WebView 通信
- **虚拟主机资源映射**：将内嵌资源映射到 HTTPS 虚拟域名，支持前端路由
- **API 路由**：C++ 端注册 API，JS 端通过虚拟域名访问
- **请求拦截**：可拦截并修改任意网络请求
- **多 WebView 管理**：单窗口内动态创建、布局、Z 序控制多个 WebView 实例
- **覆盖层系统**：透明覆盖层支持多内容块，空白区域自动鼠标穿透
- **弹窗系统**：毫秒级弹窗切换，结果通过 IPC 回传
- **系统托盘**：图标、提示、菜单、子菜单、事件回调
- **静态链接**：`-static-libstdc++ -static-libgcc -static`，无 C++ 运行时 DLL 依赖（WebView2 仍需系统已安装的 Edge Runtime）