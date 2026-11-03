/**
 * WebviewWindow 绑定类型声明
 *
 * 对应 WebviewWindowBinding.cpp。该类继承 Widget 原型，提供主 WebView 与
 * 多个命名子 WebView 的管理能力。
 * 该文件不产生任何运行时代码，仅用于 JS/TS 编辑器的类型提示与校验。
 */

/** 窗口配置与 WebView 配置平铺在同一个对象中 */
declare type WebviewWindowConfig = WidgetConfig & WebViewConfig;

/**
 * 子 WebView 创建前回调。
 * config 为即将生效的配置，可直接修改；返回配置对象时作为最终配置，
 * 但父窗口句柄等由宿主强制的字段不会被覆盖。
 */
declare type SubWebviewCreateHandler = (
    config: WebviewWidgetConfig,
    subWebview: WebviewWidget
) => WebviewWidgetConfig | void;

/** 子 WebView 创建完成回调 */
declare type SubWebviewCreateCompleteHandler = (success: boolean, subWebview: WebviewWidget) => void;

declare class WebviewWindow {
    constructor(config?: WebviewWindowConfig, onCreated?: WebViewCreatedHandler);

    /** 传入配置对象时才真正创建，未传配置时直接返回 true */
    create(config?: WebviewWindowConfig, onCreated?: WebViewCreatedHandler): boolean;
    close(): void;

    /** 主 WebView，未创建完成时返回 null */
    getMainWebview(): WebviewWidget | null;
    /** 按名称取子 WebView，不存在时返回 null */
    getSubWebview(name?: string): WebviewWidget | null;
    /** 添加命名子 WebView */
    addSubWebview(name: string, config?: WebviewWidgetConfig): boolean;
    removeSubWebview(name?: string): boolean;

    /** 设置子 WebView 创建前回调，不传参数则取消 */
    setSubWebviewCreateCallback(callback?: SubWebviewCreateHandler): void;
    /** 设置子 WebView 创建完成回调，不传参数则取消 */
    setSubWebviewCreateCompleteCallback(callback?: SubWebviewCreateCompleteHandler): void;
}

/** 继承 Widget 的全部方法（create/close 已被本类重写） */
declare interface WebviewWindow extends Omit<Widget, "create" | "close"> {}