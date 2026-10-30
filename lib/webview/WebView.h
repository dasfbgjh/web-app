#pragma once

#include <functional>
#include <string>
#include <map>
#include <vector>
#include <atomic>
#include <mutex>
#include <Windows.h>
#include <ole2.h>
#include <combaseapi.h>
#include "WebView2.h"
#include "common/utils.h"
#include "resource.h"

class WebView {
public:
    /**
     * @brief WebView2 创建完成回调
     * @param success 为 true 表示创建成功
     * @param webView WebView2 实例指针
     */
    using CreatedCallback = std::function<void( bool success, WebView *webView )>;

    /**
     * @brief 导航完成回调
     * @param isSuccess 为 true 表示页面加载成功
     * @param webView WebView2 实例指针
     */
    using NavigationCompletedCallback = std::function<void( bool isSuccess, WebView *webView )>;

    /**
     * @brief 导航开始回调
     * @param uri 导航目标 URL
     * @param webView WebView2 实例指针
     * @return true 表示继续导航，false 表示阻止导航
     */
    using NavigationStartingCallback = std::function<bool( const std::string &uri, WebView *webView )>;

    /**
     * @brief 接收到 Web 消息回调
     * @param message JSON 格式值
     * @param webView WebView2 实例指针
     */
    using WebMessageReceivedCallback = std::function<void( const json::value &message, WebView *webView )>;

    /**
     * @brief 文档标题变更回调
     * @param title 新的文档标题
     * @param webView WebView2 实例指针
     */
    using DocumentTitleChangedCallback = std::function<void( const std::string &title, WebView *webView )>;

    /**
     * @brief JavaScript 执行结果回调
     * @param result JSON 格式字符串
     * @param webView WebView2 实例指针
     */
    using ScriptResultCallback = std::function<void( const std::string &result, WebView *webView )>;

    /**
     * @brief Web 资源请求拦截回调
     *
     * @param requestUrl 请求 URL
     * @param requestMethod 请求方法（GET/POST 等）
     * @param requestHeaders 请求头键值对
     * @param responseHeaders 输出：响应头键值对，设置后请求将被拦截
     * @param responseBody 输出：响应体内容
     * @param statusCode 输出：响应状态码，默认 200
     * @return true 表示拦截该请求（使用 responseHeaders/responseBody 返回内容），
     *         false 表示放行该请求继续正常加载
     */
    using WebResourceRequestedCallback = std::function<bool(
        const std::string &requestUrl,
        const std::string &requestMethod,
        const std::map<std::string, std::string> &requestHeaders,
        std::map<std::string, std::string> &responseHeaders,
        std::string &responseBody,
        int &statusCode )>;

    /**
     * @brief API 路由回调
     * @param queryParams URL 查询参数（? 后的键值对，如 ?id=1&name=alice → {"id":"1","name":"alice"}）
     * @param requestHeaders 请求头
     * @param requestBody 请求体
     * @param responseHeaders 输出：响应头
     * @param responseBody 输出：响应体
     * @param statusCode 输出：HTTP 状态码
     */
    using ApiCallback = std::function<void(
        const std::map<std::string, std::string> &queryParams,
        const std::map<std::string, std::string> &requestHeaders,
        const std::string &requestBody,
        std::map<std::string, std::string> &responseHeaders,
        std::string &responseBody,
        int &statusCode )>;

    /**
     * @brief Web 资源响应接收回调
     *
     * 当 WebView2 收到服务器响应后触发（只读，无法修改响应）。
     * 可用于监控响应状态码、响应头和响应体。
     *
     * @param requestUrl 请求 URL
     * @param requestMethod 请求方法（GET/POST 等）
     * @param statusCode 响应状态码
     * @param reasonPhrase 响应原因短语（如 "OK"、"Not Found"）
     * @param responseHeaders 响应头键值对
     * @param responseBody 响应体内容（异步获取，可能为空表示获取失败）
     */
    using WebResourceResponseReceivedCallback = std::function<void(
        const std::string &requestUrl,
        const std::string &requestMethod,
        int statusCode,
        const std::string &reasonPhrase,
        const std::map<std::string, std::string> &responseHeaders,
        const std::string &responseBody )>;

    /**
     * @brief 新窗口请求拦截回调
     *
     * 当页面中的链接以新窗口方式打开时触发（如 target="_blank" 或 window.open）。
     *
     * @param url 请求打开的 URL
     * @param isUserInitiated 是否由用户操作触发（如点击链接），而非 JS 脚本触发
     * @param webView WebView 实例指针
     * @return true 表示拦截该请求（阻止打开新窗口），
     *         false 表示放行（使用 WebView2 默认行为打开新窗口）
     */
    using NewWindowRequestedCallback = std::function<bool(
        const std::string &url,
        bool isUserInitiated,
        WebView *webView )>;

    /**
     * @brief 浏览器控制台消息回调
     *
     * 通过注入 JS 拦截 console.debug/log/warn/error/info 实现，
     * 消息通过 WebMessage 机制传递到 C++ 端。
     *
     * @param level 日志级别：0=debug, 1=log, 2=info, 3=warn, 4=error
     * @param message 日志消息内容
     * @param webView WebView 实例指针
     */
    using ConsoleMessageCallback = std::function<void(
        int level,
        const std::string &message,
        WebView *webView )>;

    /**
     * @brief 原生函数绑定回调
     *
     * JS 端通过 window.native[name](arg1, arg2, ...) 调用，
     * C++ 端在此回调中处理并返回 JSON 结果。
     *
     * @param args JSON 数组，包含 JS 传入的参数
     * @param webView WebView2 实例指针
     * @return JSON 格式返回值，将回传给 JS
     */
    using NativeCallback = std::function<json::value( const json::array &args, WebView *webView )>;

    /**
     * @brief WebView2 配置项
     */
    struct Config {
        std::string browserPath;                ///< WebView2 Runtime 的固定路径，为空则从注册表查找已安装的 Runtime
        std::string userDataDir;                ///< 用户数据目录，为空则使用程序所在目录下的 config
        std::string initialUrl;                 ///< 初始导航 URL，为空则不自动导航
        std::string additionalBrowserArguments; ///< 附加 Chromium 命令行参数，如 "--disable-web-security --allow-file-access-from-files"
        bool disableWebSecurity = false;        ///< 禁用同源策略（跨域限制），内部追加 --disable-web-security
        bool devToolsEnabled = true;            ///< 是否启用开发者工具（F12）
        bool consoleEnabled = true;             ///< 是否启用控制台消息回调（0=debug, 1=log, 2=info, 3=warn, 4=error）
        bool statusBarEnabled = false;          ///< 是否显示状态栏
        bool defaultContextMenusEnabled = true; ///< 是否启用默认右键上下文菜单
        bool scriptEnabled = true;              ///< 是否启用 JavaScript 执行
        bool webMessageEnabled = true;          ///< 是否启用 WebMessage 通信机制
        bool transparentBackground = false;     ///< WebView2 背景是否透明（设置 DefaultBackgroundColor 为全透明）
    };

    static json::value configToJson( const Config &config );
    static Config configFromJson( const json::value &value );

    WebView();
    ~WebView();

    WebView( const WebView & ) = delete;
    WebView &operator=( const WebView & ) = delete;
    WebView( WebView && ) = delete;
    WebView &operator=( WebView && ) = delete;

    /**
     * @brief 创建 WebView2 实例
     *
     * 内部会自动尝试两种加载方式：
     *   1. 加载 WebView2Loader.dll（如果存在）
     *   2. 内置 loader：从注册表查找 Runtime 并直接加载 EmbeddedBrowserWebView.dll
     *
     * 创建完成后异步调用 onCreated 回调。
     *
     * @param parentHwnd 父窗口句柄
     * @param config 配置项
     * @param onCreated 创建完成回调
     * @return true 表示加载流程已启动，false 表示加载立即失败
     */
    bool create( HWND parentHwnd, const Config &config, CreatedCallback onCreated );

    /**
     * @brief 关闭并释放所有 WebView2 资源
     *
     * 包括 Controller、Environment 和已加载的 DLL
     */
    void close();

    /**
     * @brief 导航到指定 URL
     * @param url 目标 URL
     * @return true 表示导航成功，false 表示导航失败
     */
    bool navigate( const std::string &url );

    /**
     * @brief 将 HTML 字符串直接加载为当前文档
     * @param html HTML 内容
     * @return true 表示加载成功，false 表示加载失败
     */
    bool navigateToString( const std::string &html );

    /**
     * @brief 在当前页面中执行 JavaScript 代码
     * @param script JavaScript 代码
     * @param callback 执行完成回调，返回 JSON 结果，可为 nullptr
     */
    void executeScript( const std::string &script, ScriptResultCallback callback = nullptr );

    /**
     * @brief 在指定 WebView 实例中执行 JavaScript 代码
     * @param name WebView 实例名称
     * @param script JavaScript 代码
     * @param callback 执行完成回调，返回 JSON 结果，可为 nullptr
     * @return true 表示成功找到 WebView 实例并执行脚本，false 表示未找到实例。
     */
    static bool executeScript( const std::string &name, const std::string &script, ScriptResultCallback callback = nullptr );

    /**
     * @brief 添加在文档创建时自动执行的 JavaScript 脚本
     *
     * 脚本将在每次新文档创建时（包括导航和刷新）自动执行，
     * 适合用于注入需要在页面加载前运行的脚本（如全屏控制、消息拦截等）。
     *
     * @param script JavaScript 代码
     * @param callback 添加完成回调，返回脚本 ID，可为 nullptr
     */
    void addScriptToExecuteOnDocumentCreated( const std::string &script, ScriptResultCallback callback = nullptr );

    /**
     * @brief 向 WebView 发送 JSON 格式的 Web 消息
     *
     * 页面中通过 window.chrome.webview.addEventListener 接收
     *
     * @param json JSON 格式消息
     */
    void postWebMessage( const json::value &json );

    /**
     * @brief 设置 WebView 在父窗口中的位置和大小
     * @param x 左上角 X 坐标
     * @param y 左上角 Y 坐标
     * @param width 宽度
     * @param height 高度
     */
    void resize( int x, int y, int width, int height );

    /**
     * @brief 设置 WebView 是否可见
     * @param visible 是否可见
     */
    void setVisible( bool visible );

    /**
     * @brief 查询 WebView 是否可见
     * @return true 表示可见
     */
    bool isVisible() const;

    /**
     * @brief 设置缩放因子
     * @param factor 缩放因子，1.0 为 100%
     */
    void setZoomFactor( double factor );

    /**
     * @brief 获取当前缩放因子
     * @return 当前缩放因子
     */
    double getZoomFactor() const;

    /**
     * @brief 导航到上一页
     */
    void goBack();

    /**
     * @brief 导航到下一页
     */
    void goForward();

    /**
     * @brief 是否可以后退
     * @return true 表示可以后退
     */
    bool canGoBack() const;

    /**
     * @brief 是否可以前进
     * @return true 表示可以前进
     */
    bool canGoForward() const;

    /**
     * @brief 重新加载当前页面
     */
    void reload();

    /**
     * @brief 停止当前导航
     */
    void stop();

    /**
     * @brief 打开 DevTools 调试窗口（等同于按 F12）
     */
    void openDevToolsWindow();

    /**
     * @brief 在指定 WebView 实例中打开 DevTools 调试窗口
     * @param name WebView 实例名称
     * @return true 表示成功找到 WebView 实例并打开 DevTools，false 表示未找到实例
     */
    static bool openDevToolsWindow( const std::string &name );

    /**
     * @brief 获取当前页面 URL
     * @return 当前页面 URL
     */
    std::string getSource() const;

    /**
     * @brief 获取当前页面文档标题
     * @return 文档标题
     */
    std::string getDocumentTitle() const;

    /**
     * @brief 获取 WebView2 Runtime 的浏览器版本号
     * @return 版本号字符串
     */
    std::string getBrowserVersion() const;

    /**
     * @brief 设置 WebView 名称
     * 用于跨 WebView 通信寻址, 发送目标为自己时会自动过滤掉
     *
     * 发送方
     * window.native.sendIpcMessage(
     *     ["targetName"],
     *     { ... }
     * );
     *
     * 接收方
     * window.addEventListener('ipc-message', e => {
     *     e.detail → { from: 'senderName', data: { ... } }
     * });
     *
     * @param name 名称
     */
    void setName( const std::string &name );

    /**
     * @brief 获取 WebView 名称
     * @return 名称
     */
    std::string getName() const;

    /**
     * @brief 向指定 WebView 发送 IPC 消息
     * @param to 目标 WebView 名称
     * @param data IPC 消息数据
     * @return true 表示发送成功
     */
    bool sendIpcMessage( const std::vector<std::string> &to, const json::value &data );

    /**
     * @brief 设置导航完成事件回调
     * @param callback 导航完成回调
     */
    void setNavigationCompletedCallback( NavigationCompletedCallback callback );

    /**
     * @brief 设置导航开始事件回调
     * @param callback 导航开始回调
     */
    void setNavigationStartingCallback( NavigationStartingCallback callback );

    /**
     * @brief 设置 Web 消息接收回调
     * @param callback 消息接收回调
     */
    void setWebMessageReceivedCallback( WebMessageReceivedCallback callback );

    /**
     * @brief 设置文档标题变更回调
     * @param callback 标题变更回调
     */
    void setDocumentTitleChangedCallback( DocumentTitleChangedCallback callback );

    /**
     * @brief 设置 User-Agent 字符串
     *
     * 需在 Create 成功后调用，通过 ICoreWebView2Settings2 接口设置
     *
     * @param userAgent User-Agent 字符串，为空则恢复默认
     * @return true 表示设置成功
     */
    bool setUserAgent( const std::string &userAgent );

    /**
     * @brief 获取当前 User-Agent 字符串
     * @return User-Agent 字符串
     */
    std::string getUserAgent() const;

    /**
     * @brief 添加 Web 资源请求拦截过滤器
     *
     * 只有匹配 filterUri 的请求才会触发 WebResourceRequestedCallback。
     * filterUri 支持通配符，如 "https://*.example.com/*" 或 "*"（拦截所有请求）。
     *
     * @param filterUri URI 过滤模式，支持通配符
     * @param resourceContext 资源类型，见 COREWEBVIEW2_WEB_RESOURCE_CONTEXT 枚举
     * @return true 表示添加成功
     */
    bool addWebResourceRequestedFilter( const std::string &filterUri,
                                        COREWEBVIEW2_WEB_RESOURCE_CONTEXT resourceContext = COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL );

    /**
     * @brief 移除 Web 资源请求拦截过滤器
     * @param filterUri URI 过滤模式
     * @param resourceContext 资源类型
     * @return true 表示移除成功
     */
    bool removeWebResourceRequestedFilter( const std::string &filterUri,
                                           COREWEBVIEW2_WEB_RESOURCE_CONTEXT resourceContext );

    /**
     * @brief 设置 Web 资源请求拦截回调
     * @param callback 请求拦截回调
     */
    void setWebResourceRequestedCallback( WebResourceRequestedCallback callback );

    /**
     * @brief 设置 Web 资源响应接收回调
     *
     * 设置后，当 WebView2 收到服务器响应时触发此回调（只读，无法修改响应）。
     * 可用于监控请求的响应状态码、响应头和响应体。
     * 需要配合 addWebResourceRequestedFilter 使用，只有匹配过滤器的请求才会触发回调。
     *
     * @param callback 响应接收回调
     */
    void setWebResourceResponseReceivedCallback( WebResourceResponseReceivedCallback callback );

    /**
     * @brief 设置新窗口请求拦截回调
     *
     * 设置后，当页面中的链接以新窗口方式打开时（如 target="_blank" 或 window.open），
     * 将触发此回调而非使用 WebView2 默认行为。
     *
     * 回调返回 true 表示拦截（阻止新窗口），返回 false 表示放行。
     *
     * 典型用法：
     * - 拦截并在当前 WebView 中导航：回调中调用 navigate(url) 并返回 true
     * - 完全阻止：回调中返回 true
     * - 条件放行：根据 URL 或 isUserInitiated 决定返回 true 或 false
     *
     * @param callback 新窗口请求回调
     */
    void setNewWindowRequestedCallback( NewWindowRequestedCallback callback );

    /**
     * @brief 设置浏览器控制台消息回调
     *
     * 设置后，浏览器中 console.log/info/warn/error 的输出会转发到此回调。
     * 实现方式：在页面加载前注入 JS 拦截 console 方法，通过 WebMessage 传递到 C++。
     *
     * @param callback 控制台消息回调
     */
    void setConsoleMessageCallback( ConsoleMessageCallback callback );

    /**
     * @brief 设置虚拟主机域名，将内嵌资源映射到该域名
     *
     * 设置后，访问 https://hostName/path 时会拦截请求，
     * 从内嵌资源中查找 resourcePrefix + path 对应的资源并返回。
     * 例如设置域名为 "test.app"、前缀为 "/html"，访问 "https://test.app/index.html" 时
     * 会从内嵌资源 "/html/index.html" 读取内容返回，
     * Content-Type 由 utils::guessContentType 自动推断。
     *
     * 资源查找策略（参照 Server::serveStatic）：
     *   1. 精确匹配 resourcePrefix + path
     *   2. 若路径无扩展名，尝试前端重定向到 path + "/index.html"
     *   3. 若路径无扩展名，尝试前端重定向到 path + ".html"
     *   4. 均未命中，返回 404 并尝试返回 resourcePrefix + "/404.html"
     *
     * @param hostName 虚拟主机域名，如 "test.app"，注意: 不要用xxx.local这样的域名，会导致加载速度变慢
     * @param resourcePrefix 资源路径前缀，如 "/html"，默认为空
     * @return true 表示设置成功
     */
    bool setVirtualHostNameForResources( const std::string &hostName,
                                         const std::string &resourcePrefix = "" );

    /**
     * @brief 获取当前虚拟主机域名和资源路径前缀
     * @param hostName 虚拟主机域名
     * @param resourcePrefix 资源路径前缀
     */
    void getVirtualHostName( std::string &hostName, std::string &resourcePrefix );

    /**
     * @brief 获取内嵌资源的 URL
     *
     * 例如设置域名为 "test.app"、前缀为 "/html"，访问 "https://test.app/index.html" 时
     * 会从内嵌资源 "/html/index.html" 读取内容返回，
     * Content-Type 由 utils::guessContentType 自动推断。
     *
     * @param path 资源路径，如 "/html/index.html"
     * @return 资源 URL，如 "https://test.app/index.html"
     * @note 若 虚拟主机域名为空，返回空字符串
     */
    std::string getResourceUrl( const std::string &path );

    /**
     * @brief 注册 API 路由
     *
     * 当访问 https://hostName/api/[path] 时，若请求方法匹配且 path 前缀匹配 routePath，
     * 则调用对应回调处理请求。
     *
     * 例如：
     *   addApiRoute("GET", "/users", [](reqHeaders, reqBody, respHeaders, respBody) { ... });
     *   则 GET https://hostName/api/users 会触发回调
     *   GET https://hostName/api/users/1 也会触发（前缀匹配）
     *
     * @param method HTTP 方法，如 "GET"、"POST"
     * @param routePath 路由路径，如 "/users"
     * @param callback 处理回调
     */
    void addApiRoute( const std::string &method,
                      const std::string &routePath,
                      ApiCallback callback );

    /**
     * @brief 移除 API 路由
     * @param method HTTP 方法
     * @param routePath 路由路径
     */
    void removeApiRoute( const std::string &method, const std::string &routePath );

    /**
     * @brief 绑定一个 C++ 函数供 JS 调用
     *
     * 绑定后 JS 端可通过 window.native[name](...) 调用。
     * 调用为异步方式，返回 Promise。
     *
     * 示例：
     * @code
     *   webview.bind("add", [](const std::string& args) -> std::string {
     *       // args = "[1, 2]"
     *       return "3";
     *   });
     *   // JS: const result = await window.native.add(1, 2); // result = 3
     * @endcode
     *
     * @param name 函数名
     * @param callback 处理回调
     */
    void bind( const std::string &name, NativeCallback callback );

    /**
     * @brief 解绑一个 C++ 函数
     * @param name 函数名
     */
    void unbind( const std::string &name );

    /**
     * @brief 获取所有绑定的函数名列表
     * @return 所有绑定的函数名列表
     */
    std::vector<std::string> getBoundNames() const;

    /**
     * @brief 获取底层 ICoreWebView2 接口指针
     *
     * 可用于调用 WebView2 SDK 的完整 API
     *
     * @return ICoreWebView2 指针
     */
    ICoreWebView2 *getCoreWebView() const;

    /**
     * @brief 获取底层 ICoreWebView2Controller 接口指针
     * @return ICoreWebView2Controller 指针
     */
    ICoreWebView2Controller *getController() const;

    /**
     * @brief 获取底层 ICoreWebView2Environment 接口指针
     * @return ICoreWebView2Environment 指针
     */
    ICoreWebView2Environment *getEnvironment() const;

    /**
     * @brief 获取父窗口句柄
     * @return 父窗口句柄
     */
    HWND getParentHwnd() const;

    /**
     * @brief 获取 WebView2 渲染子窗口句柄
     *
     * WebView2 在父窗口内创建一个 Chrome_WidgetWin_0 子窗口用于渲染，
     * 此方法通过 EnumChildWindows 查找该子窗口。
     *
     * @return 渲染子窗口句柄，未找到返回 nullptr
     */
    HWND getRenderHwnd() const;

private:
    /**
     * @brief WebView2 Runtime 类型
     */
    enum class RuntimeType { Installed,
                             Embedded };

    /**
     * @brief 从注册表或固定路径查找到的客户端信息
     */
    struct ClientInfo {
        bool found = false;                               ///< 是否找到
        std::wstring dllPath;                             ///< EmbeddedBrowserWebView.dll 的完整路径
        RuntimeType runtimeType = RuntimeType::Installed; ///< Runtime 类型
    };

    /**
     * @brief 加载 WebView2：依次尝试 WebView2Loader.dll 和内置 loader
     * @return true 表示加载成功
     */
    bool loadWebView2();

    /**
     * @brief 尝试通过 WebView2Loader.dll 加载（兼容旧部署方式）
     * @return true 表示加载成功
     */
    bool tryLoadViaWebView2Loader();

    /**
     * @brief 尝试内置 loader：从注册表查找 Runtime 并直接加载 EmbeddedBrowserWebView.dll
     * @return true 表示加载成功
     */
    bool tryLoadBuiltin();

    /**
     * @brief 从注册表查找已安装的 WebView2 Runtime
     * @param system true 查 HKLM，false 查 HKCU
     * @return 客户端信息
     */
    ClientInfo findInstalledClient( bool system ) const;

    /**
     * @brief 从指定目录查找嵌入式 WebView2 Runtime
     * @param dir Runtime 根目录
     * @return 客户端信息
     */
    ClientInfo findEmbeddedClient( const std::wstring &dir ) const;

    /**
     * @brief 获取 DLL 文件的版本字符串
     * @param path DLL 文件路径
     * @return 版本字符串，失败返回空
     */
    std::wstring getFileVersionString( const std::wstring &path );

    /**
     * @brief 根据 Runtime 根目录拼接 EmbeddedBrowserWebView.dll 的完整路径
     * @param dir Runtime 根目录
     * @return DLL 完整路径
     */
    std::wstring makeClientDllPath( const std::wstring &dir ) const;

    /**
     * @brief 读取注册表字符串值
     * @param root 根键
     * @param subKey 子键路径
     * @param valueName 值名称
     * @return 字符串值，失败返回空
     */
    std::wstring queryRegString( HKEY root,
                                 const wchar_t *subKey,
                                 const wchar_t *valueName ) const;

    /**
     * @brief 确保 WebResourceRequested 事件处理器已注册
     *
     * 统一管理虚拟主机和用户回调的事件处理器注册，
     * 有虚拟主机或用户回调时注册，都无时移除。
     */
    void ensureWebResourceRequestedHandler();

    /**
     * @brief 处理 WebResourceRequested 事件
     * @param sender WebView2 实例
     * @param args 事件参数，包含请求的 URL 等信息
     */
    void onWebResourceRequested( ICoreWebView2 *,
                                 ICoreWebView2WebResourceRequestedEventArgs *args );

    /**
     * @brief 环境创建完成事件处理
     */
    void onEnvCompleted( HRESULT hr,
                         ICoreWebView2Environment *env,
                         CreatedCallback onCreated );

    /**
     * @brief 处理 native-rpc 消息
     * @param msg 消息内容
     * @return true 表示成功处理
     */
    bool handleBindCall( const json::value &msg );

    /**
     * @brief 处理 ipc 消息
     * @param msg 消息内容
     * @return true 表示成功处理
     */
    bool handleIpcMessage( const json::value &msg );

    /**
     * @brief 处理 console 消息
     * @param msg 消息内容
     * @return true 表示成功处理
     */
    bool handleConsoleMessage( const json::value &msg );

    /**
     * @brief 注册 WebView2 事件处理器
     *
     * 包括导航完成、消息接收、标题变更、关闭请求
     */
    void registerEventHandlers();

    /**
     * @brief 注入 console 拦截脚本
     *
     * 在页面导航完成后调用，拦截 console.log/info/warn/error
     * 并通过 WebMessage 转发到 C++ 端。
     */
    void injectConsoleIntercept();

    /**
     * @brief 运行 JS 注入器
     *
     * 运行 JS 注入器，确保在页面导航完成后调用。
     */
    void runJsInject();

    /**
     * @brief 生成注入绑定函数的 JS shim
     *
     * 生成一个包含所有已绑定函数的 JS shim，用于在 WebView2 中调用。
     * @param bindings 已绑定的函数映射
     * @return 生成的 JS 字符串
     */
    std::string generateInjectBindings( const std::map<std::string, NativeCallback> &bindings );

    /**
     * @brief 检查资源是否存在
     *
     * 定义了 RESOURCE_PATH 宏时从磁盘查找，否则从内嵌资源查找。
     *
     * @param resName 资源名称（含路径前缀），如 "/html/index.html"
     * @return true 表示资源存在
     */
    static bool staticResourceExists( const std::string &resName );

    /**
     * @brief 加载资源内容
     *
     * 定义了 RESOURCE_PATH 宏时从磁盘读取，否则从内嵌资源读取。
     *
     * @param resName 资源名称（含路径前缀），如 "/html/index.html"
     * @param content 输出：资源内容
     * @param outContentType 输出：Content-Type，由 utils::guessContentType 推断
     * @return true 表示加载成功
     */
    static bool getStaticResource( const std::string &resName,
                                   std::string &content,
                                   std::string &outContentType );

    /**
     * @brief 生成前端重定向 HTML
     *
     * 返回一个包含 JavaScript 的 HTML 页面，页面加载后自动跳转到指定路径，
     * 并保留当前 URL 的查询参数。
     *
     * @param path 重定向目标路径
     * @return 重定向 HTML 字符串
     */
    static std::string staticResourceRedirectPage( const std::string &path );

    /**
     * @brief 服务静态资源
     *
     * 从内嵌资源中查找并返回指定 URL 对应的资源。
     * 若资源不存在，返回 404 并尝试返回 resourcePrefix + "/404.html"。
     *
     * @param path 请求路径
     * @param respBody 输出：资源内容
     * @param respHeadersMap 输出：响应头映射
     * @param statusCode 输出：HTTP状态码
     * @return true 表示服务成功
     */
    bool staticResourceServe( const std::string &path,
                              std::string &respBody,
                              std::map<std::string,
                                       std::string> &respHeadersMap,
                              int &statusCode );

    /**
     * @brief 服务 API 请求
     *
     * 处理前端发送的 API 请求，根据路径调用对应的函数。
     * 若路径不存在，返回 404 并尝试返回 resourcePrefix + "/404.html"。
     *
     * @param method 请求方法（GET/POST/PUT/DELETE）
     * @param url 请求 URL
     * @param apiPath 请求路径（不包含api前缀）
     * @param reqHeadersMap 请求头映射
     * @param requestBody 请求体内容
     * @param respBody 输出：响应内容
     * @param respHeadersMap 输出：响应头映射
     * @param statusCode 输出：HTTP状态码
     * @return true 表示服务成功
     */
    bool apiServer( const std::string &method,
                    const std::string &url,
                    std::string apiPath,
                    const std::map<std::string, std::string> &reqHeadersMap,
                    const std::string &requestBody,
                    std::string &respBody,
                    std::map<std::string, std::string> &respHeadersMap,
                    int &statusCode );

    /**
     * @brief 将 COM 分配的 LPWSTR 转为 std::string 并释放内存
     * @param str COM 分配的字符串指针
     * @return 转换后的 UTF-8 字符串
     */
    std::string coMemToStr( LPWSTR str ) const;

    /// WebView2Loader.dll 导出的公共 API 函数类型
    using CreateEnvFn = HRESULT( STDAPICALLTYPE * )( PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions *, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler * );
    /// EmbeddedBrowserWebView.dll 导出的内部 API 函数类型
    using CreateEnvInternalFn = HRESULT( STDAPICALLTYPE * )( bool, int, PCWSTR, IUnknown *, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler * );

    HMODULE m_clientDll = nullptr;                       ///< 已加载的 DLL 模块句柄（WebView2Loader.dll 或 EmbeddedBrowserWebView.dll）
    CreateEnvFn m_createEnvFn = nullptr;                 ///< WebView2Loader.dll 的公共 API 函数指针
    CreateEnvInternalFn m_createEnvInternalFn = nullptr; ///< EmbeddedBrowserWebView.dll 的内部 API 函数指针
    RuntimeType m_runtimeType = RuntimeType::Installed;  ///< 当前使用的 Runtime 类型

    HWND m_parentHwnd = nullptr; ///< 父窗口句柄
    Config m_config;             ///< 配置项副本
    bool m_oleInitialized = false;

    ICoreWebView2Environment *m_environment = nullptr; ///< WebView2 环境接口
    ICoreWebView2Controller *m_controller = nullptr;   ///< WebView2 控制器接口
    ICoreWebView2 *m_webview = nullptr;                ///< WebView2 核心接口
    ICoreWebView2Settings *m_settings = nullptr;       ///< WebView2 设置接口

    EventRegistrationToken m_navigationCompletedToken = {};         ///< 导航完成事件令牌
    EventRegistrationToken m_navigationStartingToken = {};          ///< 导航开始事件令牌
    EventRegistrationToken m_webMessageReceivedToken = {};          ///< 消息接收事件令牌
    EventRegistrationToken m_documentTitleChangedToken = {};        ///< 标题变更事件令牌
    EventRegistrationToken m_windowCloseRequestedToken = {};        ///< 关闭请求事件令牌
    EventRegistrationToken m_webResourceRequestedToken = {};        ///< 请求拦截事件令牌
    EventRegistrationToken m_newWindowRequestedToken = {};          ///< 新窗口请求事件令牌
    EventRegistrationToken m_webResourceResponseReceivedToken = {}; ///< 响应接收事件令牌

    NavigationCompletedCallback m_onNavigationCompleted;                 ///< 导航完成回调
    NavigationStartingCallback m_onNavigationStarting;                   ///< 导航开始回调
    WebMessageReceivedCallback m_onWebMessageReceived;                   ///< 消息接收回调
    DocumentTitleChangedCallback m_onDocumentTitleChanged;               ///< 标题变更回调
    WebResourceRequestedCallback m_onWebResourceRequested;               ///< 请求拦截回调
    NewWindowRequestedCallback m_onNewWindowRequested;                   ///< 新窗口请求回调
    WebResourceResponseReceivedCallback m_onWebResourceResponseReceived; ///< 响应接收回调
    ConsoleMessageCallback m_onConsoleMessage;                           ///< 控制台消息回调

    std::string m_virtualHostName; ///< 虚拟主机域名
    std::string m_resourcePrefix;  ///< 资源路径前缀
    std::string m_name;            ///< 实例名称

    static std::vector<WebView *> s_instances; ///< 全局实例列表
    static std::mutex s_instancesMutex;        ///< 实例列表互斥锁

    static int s_oleCount;                                  ///< OLE 初始化引用计数
    static int s_dllCount;                                  ///< DLL 加载引用计数
    static HMODULE s_sharedDll;                             ///< 共享 DLL 模块句柄
    static CreateEnvFn s_sharedCreateEnvFn;                 ///< 共享公共 API 函数指针
    static CreateEnvInternalFn s_sharedCreateEnvInternalFn; ///< 共享内部 API 函数指针
    static RuntimeType s_sharedRuntimeType;                 ///< 共享 Runtime 类型
    static std::wstring s_runtimeVersion;                   ///< 已加载 Runtime 的版本号

    std::map<std::string, NativeCallback> m_bindings; ///< 原生函数绑定表
    std::mutex m_bindingsMutex;                       ///< 绑定表互斥锁
    int m_nextSeqId = 0;                              ///< 下一个消息序列号

    std::map<std::string, std::map<std::string, ApiCallback>> m_apiRoutes; ///< API 路由表 (method → path → callback)

    std::vector<std::pair<std::string, COREWEBVIEW2_WEB_RESOURCE_CONTEXT>> m_pendingFilters; ///< 待注册过滤器（create 前缓存）

    bool m_initialized = false; ///< 是否已初始化完成

    static constexpr const char *kVirtualHostProtocol = "https://"; // 虚拟主机协议

    static constexpr const wchar_t *kRegSubKey = L"SOFTWARE\\Microsoft\\EdgeUpdate\\ClientState\\"; ///< Edge Update 注册表子键前缀
    static constexpr const wchar_t *kStableGuid = L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";        ///< Edge Stable 渠道的 GUID
};