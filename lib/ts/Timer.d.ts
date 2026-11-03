/**
 * 定时器绑定类型声明
 *
 * 对应 TimerBinding.cpp 注册到全局作用域的函数。
 * 该文件不产生任何运行时代码，仅用于 JS/TS 编辑器的类型提示与校验。
 *
 * 注意：定时器由 Win32 消息循环驱动，只有调用 App.run() 启动事件循环后才会触发；
 * 未启动事件循环时，到期的回调不会执行。
 */

/**
 * 延时执行一次回调。
 * @param callback 到期的回调，定时器 ID 之外的附加参数会原样传入
 * @param delay 延迟毫秒数，默认 0；系统消息定时器精度约为 10ms
 * @param args 传给回调的附加参数
 * @returns 定时器 ID，可传给 clearTimeout
 */
declare function setTimeout(
    callback: ( ...args: any[] ) => void,
    delay?: number,
    ...args: any[]
): number;

/**
 * 取消由 setTimeout 创建的定时器。
 * @param id 定时器 ID，无效 ID 会被忽略
 */
declare function clearTimeout( id?: number ): void;

/**
 * 每隔指定时间重复执行回调。
 * @param callback 到期的回调，定时器 ID 之外的附加参数会原样传入
 * @param delay 间隔毫秒数，默认 0；系统消息定时器精度约为 10ms
 * @param args 传给回调的附加参数
 * @returns 定时器 ID，可传给 clearInterval
 */
declare function setInterval(
    callback: ( ...args: any[] ) => void,
    delay?: number,
    ...args: any[]
): number;

/**
 * 取消由 setInterval 创建的定时器。
 * @param id 定时器 ID，无效 ID 会被忽略
 */
declare function clearInterval( id?: number ): void;