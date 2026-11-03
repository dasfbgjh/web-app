/**
 * WebviewWidget 绑定类型声明
 *
 * 对应 WebviewWidgetBinding.cpp。该类在 Widget 原型链之上复用了 WebView 的
 * 方法表，因此实例同时拥有 Widget 与 WebView 的全部方法。
 * 该文件不产生任何运行时代码，仅用于 JS/TS 编辑器的类型提示与校验。
 */

/** 窗口配置与 WebView 配置平铺在同一个对象中 */
declare type WebviewWidgetConfig = WidgetConfig & WebViewConfig;

declare class WebviewWidget {
    constructor(config?: WebviewWidgetConfig, onCreated?: WebViewCreatedHandler);

    /** 传入配置对象时才真正创建，未传配置时直接返回 true */
    create(config?: WebviewWidgetConfig, onCreated?: WebViewCreatedHandler): boolean;
    close(): void;

    /** 窗口透明时是否自动把 WebView 背景设为透明，仅 create 前调用生效 */
    setAutoWebviewTransparent(transparent?: boolean): void;
    /** 设置 WebView 相对窗口的四边距 */
    setWebViewMargin(left?: number, top?: number, right?: number, bottom?: number): void;
}

/** 继承 Widget 与 WebView 的全部方法（create/close 已被本类重写） */
declare interface WebviewWidget extends Omit<Widget, "create" | "close">, Omit<WebView, "create" | "close"> { }