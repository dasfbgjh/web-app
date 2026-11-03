/**
 * window.native 类型声明
 *
 * 对应 WebView::generateInjectBindings 注入到页面上下文中的 RPC 桥架。
 * 该文件不产生任何运行时代码，仅用于 JS/TS 编辑器的类型提示与校验。
 *
 * 注入脚本会在页面的 window 上创建 native 对象，并把 C++ 侧 bind 的方法名
 * 统一包装成 Promise：
 *
 *   window.native[method]( ...args ) → Promise<result>
 *
 * 因此所有绑定方法都是异步的，必须 await 或 .then 才能拿到返回值。
 * 原生侧抛出的异常会以 reject(new Error(...)) 的形式返回。
 */

/** 矩形裁剪区域，坐标为窗口客户区坐标，w/h 为宽高（非右下角坐标） */
declare interface NativeRectRegion {
    type: 'rect';
    x?: number;
    y?: number;
    w?: number;
    h?: number;
}

/** 圆角矩形裁剪区域，rx/ry 为圆角半径（内部按 2 倍半径创建） */
declare interface NativeRoundRectRegion {
    type: 'roundRect';
    x?: number;
    y?: number;
    w?: number;
    h?: number;
    rx?: number;
    ry?: number;
}

/** 椭圆裁剪区域，外接矩形为 x/y/w/h */
declare interface NativeEllipseRegion {
    type: 'ellipse';
    x?: number;
    y?: number;
    w?: number;
    h?: number;
}

/** 多边形裁剪区域，points 为至少 3 个 [x, y] 顶点 */
declare interface NativePolygonRegion {
    type: 'polygon';
    points: [number, number][];
}

/** 区域组合，对 regions 中的子区域按 op 做布尔运算，默认 union */
declare interface NativeCombineRegion {
    type: 'combine';
    op?: 'union' | 'intersect' | 'diff' | 'xor';
    regions: NativeRegion[];
}

/** 单个裁剪区域描述，对应 utils::createRgn */
declare type NativeRegion =
    | NativeRectRegion
    | NativeRoundRectRegion
    | NativeEllipseRegion
    | NativePolygonRegion
    | NativeCombineRegion;

/** 窗口左上角屏幕坐标 */
declare interface NativePosition {
    x: number;
    y: number;
}

/** 窗口宽高（像素） */
declare interface NativeSize {
    width: number;
    height: number;
}

/** ipc-message 事件 detail，from 为发送方 WebView 名称 */
declare interface NativeIpcMessageDetail {
    from: string;
    data: any;
}

/**
 * 注入到页面中的原生能力集合。
 *
 * 其中 window* 系列由 WebviewWidget::bindWindowMethods 绑定，
 * webview* 系列由 WebviewWindow::bindWebviewMethods 绑定。
 * 因此某个页面能用到哪些方法取决于承载它的窗口类型：
 *   - WebviewWidget：仅 window* 系列
 *   - WebviewWindow 的主 WebView：window* 与 webview* 系列同时可用
 * 未绑定的方法在 native 对象上不存在。
 */
declare interface NativeApi {
    /**
     * 关闭应用窗口并退出消息循环。
     * 关闭动作会投递到事件循环，因此 Promise 会在窗口真正关闭前返回。
     */
    windowClose(): Promise<null>;

    /** 最小化窗口 */
    windowMinimize(): Promise<null>;

    /** 最大化窗口，若当前已最大化则还原，若为全屏则先还原再最大化 */
    windowMaximize(): Promise<null>;

    /**
     * 开始拖拽窗口，在鼠标左键松开前自动跟随鼠标移动。
     * 通常绑定到自定义标题栏的 mousedown 事件。
     * 窗口处于最大化或全屏时不生效。
     */
    windowDrag(): Promise<null>;

    /**
     * 设置窗口是否始终位于其他窗口之上
     * @param topMost true=置顶，false=取消置顶
     */
    windowSetTopMost(topMost: boolean): Promise<null>;

    /** 获取窗口左上角屏幕坐标 */
    windowGetPosition(): Promise<NativePosition>;

    /** 获取窗口宽高（像素） */
    windowGetSize(): Promise<NativeSize>;

    /**
     * 设置窗口不透明度
     * @param opacity 0=全透明，255=不透明，超出范围会被夹紧
     */
    windowSetOpacity(opacity: number): Promise<null>;

    /**
     * 开始拖拽调整窗口大小，在鼠标左键松开前自动按方向调整尺寸。
     * 窗口处于最大化或全屏时不生效，最小宽度 200px、最小高度 100px。
     * @param direction 由 'n'、's'、'e'、'w' 组合而成，例如 'nw' 左上角、'se' 右下角
     */
    windowResize(direction: string): Promise<null>;

    /**
     * 同时设置窗口位置和大小
     * @param x 窗口左上角 X 屏幕坐标
     * @param y 窗口左上角 Y 屏幕坐标
     * @param width 窗口宽度（像素）
     * @param height 窗口高度（像素）
     */
    windowSetRect(x: number, y: number, width: number, height: number): Promise<null>;

    /**
     * 设置窗口裁剪区域并控制鼠标穿透。
     * 传入空数组或省略参数会移除裁剪区域。
     * @param regions 区域描述数组，多个区域取并集
     * @param clickThrough 仅当 regions 为空时生效，true=启用鼠标穿透
     * @returns 是否设置成功
     */
    windowSetRgn(regions: NativeRegion[], clickThrough?: boolean): Promise<boolean>;

    /**
     * 显示弹窗，交互结果通过 ipc-message 事件发回当前 WebView：
     *
     *   window.addEventListener( 'ipc-message', e => {
     *       e.detail // → { from: 'popup', data: { type, id, label, ... } }
     *   } );
     *
     * @param type 弹窗类型
     * @param x 弹窗参考 X 屏幕坐标
     * @param y 弹窗参考 Y 屏幕坐标
     * @param params 弹窗参数，各类型不同
     */
    showPopup(type: 'context-menu' | 'tooltip', x: number, y: number, params: Record<string, any>): Promise<null>;

    /**
     * 在覆盖层添加一个内容块，交互结果通过 ipc-message 事件发回当前 WebView。
     * @param id 内容块唯一标识，重复 id 会替换已有内容
     * @param type 内容块类型
     * @param x 内容块参考 X 屏幕坐标
     * @param y 内容块参考 Y 屏幕坐标
     * @param params 内容块参数，各类型不同
     */
    showOverlay(id: string, type: 'notification' | 'tooltip' | 'dialog', x: number, y: number, params: Record<string, any>): Promise<null>;

    /**
     * 从覆盖层移除指定内容块
     * @param id 内容块唯一标识
     */
    removeOverlay(id: string): Promise<null>;

    /** 移除覆盖层所有内容块 */
    clearOverlay(): Promise<null>;

    /**
     * 新增子 WebView。
     * 配置项中缺失的 x/y/width/height 默认为 0。
     * @param name 子 WebView 名称，需唯一且非空
     * @param config 子 WebView 配置
     * @returns 是否创建成功，名称已存在或参数非法时返回 false
     */
    webviewAdd(name: string, config?: WebviewWidgetConfig): Promise<boolean>;

    /**
     * 移除指定子 WebView
     * @param name 子 WebView 名称
     * @returns 是否移除成功
     */
    webviewRemove(name: string): Promise<boolean>;

    /**
     * 设置子 WebView 在父窗口中的位置
     * @returns 名称不存在或参数非法时返回 null
     */
    webviewSetPosition(name: string, x: number, y: number): Promise<null>;

    /**
     * 设置子 WebView 大小
     * @returns 名称不存在或参数非法时返回 null
     */
    webviewSetSize(name: string, width: number, height: number): Promise<null>;

    /**
     * 设置子 WebView 在父窗口中的位置和大小
     * @returns 名称不存在或参数非法时返回 null
     */
    webviewSetRect(name: string, x: number, y: number, width: number, height: number): Promise<null>;

    /** 获取全部子 WebView 名称 */
    webviewGetNames(): Promise<string[]>;

    /** 获取子 WebView 数量 */
    webviewGetCount(): Promise<number>;

    /**
     * 设置子 WebView 在父窗口中的 Z 序
     * @param bottomName 位于下层的子 WebView 名称
     * @param topName 位于上层的子 WebView 名称
     * @returns 是否调整成功
     */
    webviewChangeZOrder(bottomName: string, topName: string): Promise<boolean>;

    /**
     * 导航子 WebView 到指定 URL
     * @returns 名称不存在或参数非法时返回 false
     */
    webviewNavigate(name: string, url: string): Promise<boolean>;

    /**
     * 在子 WebView 中执行 JavaScript
     * @returns 名称不存在或参数非法时返回 false
     */
    webviewExecuteScript(name: string, script: string): Promise<boolean>;

    /**
     * 设置子 WebView 的 User-Agent
     * @returns 是否设置成功
     */
    webviewSetUserAgent(name: string, userAgent: string): Promise<boolean>;

    /**
     * 打开子 WebView 的开发者工具窗口
     * @returns 名称不存在时返回 false
     */
    webviewOpenDevTools(name: string): Promise<boolean>;

    /**
     * 向指定 WebView 发送 IPC 消息。
     *
     * 注意：这是注入脚本直接实现的方法，不经过 RPC 桥架，因此同步返回 void，
     * 而非 Promise；发送目标为自己时会被自动过滤。
     *
     * 接收方通过 ipc-message 事件拿到数据：
     *
     *   window.addEventListener( 'ipc-message', e => {
     *       e.detail // → { from: 'senderName', data: ... }
     *   } );
     *
     * @param to 目标 WebView 名称，单个名称或名称数组
     * @param data 消息数据，需可被 JSON 序列化
     */
    sendIpcMessage(to: string | string[], data: any): void;
}

declare interface Window {
    /** 原生能力集合，由 WebView 在文档创建时注入 */
    native: NativeApi;
}

