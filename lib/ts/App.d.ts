/**
 * App 绑定类型声明
 *
 * 对应 AppBinding.cpp 注册到全局作用域的 App 单例对象。
 * 该文件不产生任何运行时代码，仅用于 JS/TS 编辑器的类型提示与校验。
 */

declare const App: {
    /**
     * 启动事件循环并阻塞，直到调用 quit() 或最后一个窗口关闭。
     * 循环期间会持续执行 JS 待处理作业（Promise 回调等）。
     * @returns 事件循环的退出码
     */
    run(): number;

    /**
     * 退出所有事件循环，可在任意 JS 回调中调用。
     * @param exitCode 进程退出码，默认 0
     */
    quit( exitCode?: number ): void;
};