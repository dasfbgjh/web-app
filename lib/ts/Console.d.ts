/**
 * console 绑定类型声明
 *
 * 对应 ConsoleBinding.cpp 注册到全局作用域的 console 对象。
 * 该文件不产生任何运行时代码，仅用于 JS/TS 编辑器的类型提示与校验。
 *
 * 与 QuickJS 标准 helpers 直接写 stdout 的实现不同，这里的输出统一走项目 Logger，
 * 因此格式、级别过滤与日志落盘配置都与原生代码一致。
 */

declare const console: {
    /** 调试信息，对应日志级别 DEBUG */
    debug( ...data: unknown[] ): void;
    /** 普通输出，对应日志级别 LOG */
    log( ...data: unknown[] ): void;
    /** 提示信息，对应日志级别 INFO */
    info( ...data: unknown[] ): void;
    /** 警告，对应日志级别 WARN */
    warn( ...data: unknown[] ): void;
    /** 错误，对应日志级别 ERR */
    error( ...data: unknown[] ): void;
};