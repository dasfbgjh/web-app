/**
 * WebView 绑定类型声明
 *
 * 对应 WebViewBinding.cpp 注册到全局作用域的 WebView 构造函数。
 * 该文件不产生任何运行时代码，仅用于 JS/TS 编辑器的类型提示与校验。
 */

/** WebView2 配置，字段与 C++ WebView::Config 一一对应 */
declare interface WebViewConfig {
    /** WebView2 Runtime 的固定路径，为空则从注册表查找 */
    browserPath?: string;
    /** 用户数据目录，为空则使用程序所在目录下的 config */
    userDataDir?: string;
    /** 初始导航 URL，为空则不自动导航 */
    initialUrl?: string;
    /** initialUrl 的等价别名 */
    url?: string;
    /** 附加 Chromium 命令行参数，如 "--disable-web-security" */
    additionalBrowserArguments?: string;
    /** 禁用同源策略，默认 false */
    disableWebSecurity?: boolean;
    /** 是否启用开发者工具（F12），默认 true */
    devToolsEnabled?: boolean;
    /** 是否启用控制台消息回调，默认 true */
    consoleEnabled?: boolean;
    /** 是否显示状态栏，默认 false */
    statusBarEnabled?: boolean;
    /** 是否启用默认右键菜单，默认 true */
    defaultContextMenusEnabled?: boolean;
    /** 是否启用 JavaScript 执行，默认 true */
    scriptEnabled?: boolean;
    /** 是否启用 WebMessage 通信，默认 true */
    webMessageEnabled?: boolean;
    /** WebView2 背景是否透明，默认 false */
    transparentBackground?: boolean;
}

declare type WebViewHeaders = Record<string, string>;

/** 创建完成回调 */
declare type WebViewCreatedHandler = (success: boolean) => void;
/** 脚本执行结果回调 */
declare type WebViewScriptResultHandler = (result: string) => void;

/** 控制台日志级别：0=debug, 1=log, 2=info, 3=warn, 4=error */
declare type WebViewConsoleLevel = 0 | 1 | 2 | 3 | 4;

/**
 * 资源类型过滤，取值为 COREWEBVIEW2_WEB_RESOURCE_CONTEXT：
 * 0=ALL, 1=DOCUMENT, 2=STYLESHEET, 3=IMAGE, 4=MEDIA, 5=FONT, 6=SCRIPT,
 * 7=XML_HTTP_REQUEST, 8=FETCH, 9=TEXT_TRACK, 10=EVENT_SOURCE, 11=WEBSOCKET,
 * 12=MANIFEST, 13=SIGNED_EXCHANGE, 14=PING, 15=CSP_VIOLATION_REPORT, 16=OTHER
 */
declare type WebViewResourceContext = number;

/** window.native[name](...) 的原生函数签名 */
declare type WebViewNativeFunction = (...args: any[]) => unknown;

/** API 路由请求体 */
declare interface WebViewApiRequest {
    /** URL 查询参数，如 ?id=1 → { id: "1" } */
    query: WebViewHeaders;
    headers: WebViewHeaders;
    body: string;
}

/** API 路由响应体，返回该对象即生效 */
declare interface WebViewApiResponse {
    headers?: WebViewHeaders;
    body?: string;
    /** HTTP 状态码，默认 200 */
    statusCode?: number;
}

declare type WebViewApiHandler = (request: WebViewApiRequest) => WebViewApiResponse | void;

/** 被拦截的资源请求 */
declare interface WebViewResourceRequest {
    url: string;
    method: string;
    headers: WebViewHeaders;
}

/** 拦截结果，intercept=true 时使用 headers/body/statusCode 返回自定义内容 */
declare interface WebViewResourceInterceptResult {
    intercept: boolean;
    headers?: WebViewHeaders;
    body?: string;
    statusCode?: number;
}

/** 已接收的资源响应（只读） */
declare interface WebViewResourceResponseInfo {
    url: string;
    method: string;
    statusCode: number;
    /** 原因短语，如 "OK"、"Not Found" */
    reasonPhrase: string;
    headers: WebViewHeaders;
    /** 响应体，异步获取，可能为空 */
    body: string;
}

declare class WebView {
    /** 传入 parentHwnd 时会立即启动创建流程 */
    constructor(parentHwnd?: number, config?: WebViewConfig, onCreated?: WebViewCreatedHandler);

    /** 在指定名称的 WebView 上执行脚本，返回是否投递成功 */
    static executeScript(name: string, script: string, onResult?: WebViewScriptResultHandler): boolean;
    /** 打开指定名称 WebView 的开发者工具 */
    static openDevToolsByName(name: string): boolean;

    create(parentHwnd?: number, config?: WebViewConfig, onCreated?: WebViewCreatedHandler): boolean;
    close(): void;

    navigate(url?: string): boolean;
    navigateToString(html?: string): boolean;
    executeScript(script?: string, onResult?: WebViewScriptResultHandler): void;
    addScriptToExecuteOnDocumentCreated(script?: string, onResult?: WebViewScriptResultHandler): void;
    postWebMessage(message: unknown): void;

    resize(x?: number, y?: number, width?: number, height?: number): void;
    setVisible(visible?: boolean): void;
    isVisible(): boolean;
    setZoomFactor(factor?: number): void;
    getZoomFactor(): number;

    goBack(): void;
    goForward(): void;
    canGoBack(): boolean;
    canGoForward(): boolean;
    reload(): void;
    stop(): void;
    openDevToolsWindow(): void;

    getSource(): string;
    getDocumentTitle(): string;
    getBrowserVersion(): string;

    setName(name?: string): void;
    getName(): string;

    /** 通过 WebMessage 通道向指定名称的 WebView 发送消息 */
    sendIpcMessage(targets?: string[], data?: unknown): boolean;

    setUserAgent(userAgent?: string): boolean;
    getUserAgent(): string;

    setVirtualHostNameForResources(hostName?: string, resourcePrefix?: string): boolean;
    getVirtualHostName(): { hostName: string; resourcePrefix: string };
    getResourceUrl(path?: string): string;

    /** 注册 API 路由，JS 端通过 fetch 访问 */
    addApiRoute(method: string, path: string, handler: WebViewApiHandler): void;
    removeApiRoute(method?: string, path?: string): void;

    /** 注册 window.native[name] 原生函数 */
    bind(name: string, handler: WebViewNativeFunction): void;
    unbind(name?: string): void;
    getBoundNames(): string[];

    setNavigationCompletedCallback(handler?: WebViewCreatedHandler): void;
    /** 返回 false 可阻止本次导航 */
    setNavigationStartingCallback(handler?: (uri: string) => boolean | void): void;
    setWebMessageReceivedCallback(handler?: (message: unknown) => void): void;
    setDocumentTitleChangedCallback(handler?: (title: string) => void): void;
    setConsoleMessageCallback(handler?: (level: WebViewConsoleLevel, message: string) => void): void;
    /** 返回 true 表示拦截，阻止打开新窗口 */
    setNewWindowRequestedCallback(handler?: (url: string, userInitiated: boolean) => boolean | void): void;
    /** 返回 intercept=true 时用自定义响应替换该请求 */
    setWebResourceRequestedCallback(
        handler?: (request: WebViewResourceRequest) => WebViewResourceInterceptResult | void
    ): void;
    setWebResourceResponseReceivedCallback(handler?: (response: WebViewResourceResponseInfo) => void): void;

    addWebResourceRequestedFilter(filterUri?: string, resourceContext?: WebViewResourceContext): boolean;
    removeWebResourceRequestedFilter(filterUri?: string, resourceContext?: WebViewResourceContext): boolean;

    getParentHwnd(): number;
    getRenderHwnd(): number;
}