/**
 * Widget 绑定类型声明
 *
 * 对应 WidgetBinding.cpp 注册到全局作用域的 Widget 构造函数。
 * 该文件不产生任何运行时代码，仅用于 JS/TS 编辑器的类型提示与校验。
 */

/** 窗口/控件配置，字段与 C++ Widget::Config 一一对应 */
declare interface WidgetConfig {
    /** 窗口类名，为空则自动生成 */
    className?: string;
    /** 窗口标题 */
    title?: string;
    /** 窗口 X 坐标，默认 CW_USEDEFAULT */
    x?: number;
    /** 窗口 Y 坐标，默认 CW_USEDEFAULT */
    y?: number;
    /** 窗口宽度，默认 CW_USEDEFAULT */
    width?: number;
    /** 窗口高度，默认 CW_USEDEFAULT */
    height?: number;
    /** 窗口风格，默认 WS_OVERLAPPEDWINDOW */
    style?: number;
    /** 扩展窗口风格 */
    exStyle?: number;
    /** 窗口类风格 */
    classStyle?: number;
    /** 是否居中显示，默认 true */
    center?: boolean;
    /** 是否置顶，默认 false */
    topMost?: boolean;
    /** 无边框，默认 false */
    noBorder?: boolean;
    /** 是否可调整大小，默认 true */
    resizable?: boolean;
    /** 是否有标题栏，默认 true */
    hasTitleBar?: boolean;
    /** 是否有系统菜单，默认 true */
    hasSysMenu?: boolean;
    /** 是否有最小化按钮，默认 true */
    hasMinButton?: boolean;
    /** 是否有最大化按钮，默认 true */
    hasMaxButton?: boolean;
    /** 是否有关闭按钮，默认 true */
    hasCloseButton?: boolean;
    /** 是否裁剪子窗口区域，默认 true */
    clipChildren?: boolean;
    /** 是否裁剪兄弟窗口区域，默认 true */
    clipSiblings?: boolean;
    /** 创建后是否可见，默认 true */
    visible?: boolean;
    /** 不透明度，0=全透明，255=不透明，默认 255 */
    opacity?: number;
    /** 背景是否透明，默认 false */
    transparentBackground?: boolean;
    /** 圆角半径（像素），0=无圆角，默认 0 */
    borderRadius?: number;
    /** 鼠标穿透，默认 false */
    clickThrough?: boolean;
    /** 不在任务栏显示图标，默认 false */
    noTaskbar?: boolean;
}

declare interface WidgetSize {
    width: number;
    height: number;
}

declare interface WidgetPosition {
    x: number;
    y: number;
}

declare class Widget {
    /** 传入配置对象时会立即创建窗口，否则需再调用 create() */
    constructor(config?: WidgetConfig);

    /** 创建控件，parent 为空时创建独立窗口，非空时创建子控件 */
    create(config?: WidgetConfig): boolean;
    /** 关闭控件（隐藏窗口并标记关闭，析构时才真正销毁） */
    close(): void;

    bringToTop(): void;
    bringToBottom(): void;

    setClickThrough(enable?: boolean): void;
    isClickThrough(): boolean;

    /** 原生窗口句柄，子控件时可用于挂载到其它 API */
    getHwnd(): number;
    isChild(): boolean;

    setTitle(title?: string): void;
    getTitle(): string;

    setSize(width?: number, height?: number, repaint?: boolean): void;
    getSize(): WidgetSize;
    getClientSize(): WidgetSize;
    setPosition(x?: number, y?: number): void;
    getPosition(): WidgetPosition;
    setRect(x?: number, y?: number, width?: number, height?: number): void;

    center(): void;
    setTopMost(topMost?: boolean): void;
    setFullscreen(fullscreen?: boolean): void;
    isFullscreen(): boolean;
    maximize(): void;
    minimize(): void;
    restore(): void;

    setVisible(visible?: boolean): void;
    isVisible(): boolean;
    isMaximized(): boolean;
    isMinimized(): boolean;

    setMinSize(minWidth?: number, minHeight?: number): void;
    setMaxSize(maxWidth?: number, maxHeight?: number): void;

    /** 设置不透明度，入参会被裁剪到 0-255 */
    setOpacity(opacity?: number): void;
    getOpacity(): number;
}