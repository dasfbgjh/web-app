#ifndef QUICKJS_HPP
#define QUICKJS_HPP

#include "quickjs.h"

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace qjs {

class Engine;

/**
 * 统一的异常类型，封装层中所有失败路径都会抛出该异常。
 * 继承自 std::runtime_error，错误文本可通过 what() 获取。
 */
class Error : public std::runtime_error {
public:
    /**
     * 使用错误信息构造异常对象。
     * @param message 错误描述文本。
     */
    explicit Error( const std::string &message ) : std::runtime_error( message ) {
    }
};

/**
 * 将 JS 异常值转换为可读的字符串描述。
 * @param ctx JS 上下文。
 * @param exception 待转换的异常值，调用方负责其生命周期。
 * @return 异常的可读文本，转换失败时返回默认提示。
 */
std::string exceptionToString( JSContext *ctx, JSValue exception );

/**
 * JSValue 的 RAII 封装，自动完成引用计数的增加与释放。
 * 默认构造得到无效值；拷贝与移动均按引用计数语义处理。
 * 注意：Value 内部持有裸 JSContext 指针，不得比创建它的 Engine 存活更久，
 * 也不得跨 Engine 传递。
 */
class Value {
public:
    /**
     * 构造一个未绑定上下文的无值对象。
     */
    Value() = default;

    /**
     * 接管一个已存在的 JSValue。
     * @param ctx 该值所属的 JS 上下文。
     * @param value 待接管的原生值，所有权转移给 Value，调用方无需再释放。
     */
    Value( JSContext *ctx, JSValue value );

    /**
     * 拷贝构造，对原生值执行一次引用计数递增。
     * @param other 源对象。
     */
    Value( const Value &other );

    /**
     * 移动构造，源对象在移动后变为无效值，避免重复释放。
     * @param other 源对象。
     */
    Value( Value &&other ) noexcept;

    /**
     * 拷贝赋值，先释放自身持有的值，再递增源对象的引用计数。
     * @param other 源对象。
     * @return 自身引用。
     */
    Value &operator=( const Value &other );

    /**
     * 移动赋值，先释放自身持有的值，再接管源对象的值。
     * @param other 源对象。
     * @return 自身引用。
     */
    Value &operator=( Value &&other ) noexcept;

    /**
     * 析构函数，释放持有的原生值。
     */
    ~Value();

    /**
     * 判断当前对象是否绑定了有效的 JS 上下文。
     * @return 已绑定返回 true，默认构造或移动后的对象返回 false。
     */
    bool isValid() const;

    /**
     * 判断值是否为 undefined。
     * @return 是 undefined 返回 true。
     */
    bool isUndefined() const;

    /**
     * 判断值是否为 null。
     * @return 是 null 返回 true。
     */
    bool isNull() const;

    /**
     * 判断值是否为布尔类型。
     * @return 是布尔值返回 true。
     */
    bool isBool() const;

    /**
     * 判断值是否为数字类型。
     * @return 是数字返回 true。
     */
    bool isNumber() const;

    /**
     * 判断值是否为字符串类型。
     * @return 是字符串返回 true。
     */
    bool isString() const;

    /**
     * 判断值是否为对象类型。
     * @return 是对象返回 true。
     */
    bool isObject() const;

    /**
     * 判断值是否为数组。
     * @return 是数组返回 true。
     */
    bool isArray() const;

    /**
     * 判断值是否为可调用的函数。
     * @return 是函数返回 true。
     */
    bool isFunction() const;

    /**
     * 判断值是否为 JS 错误对象。
     * @return 是错误对象返回 true。
     */
    bool isError() const;

    /**
     * 判断值是否为异常占位值。
     * @return 是异常值返回 true。
     */
    bool isException() const;

    /**
     * 将值转换为 int32。
     * @return 转换后的整数。
     * @throw Error 值未绑定上下文或无法转换为 int32 时抛出。
     */
    int32_t toInt32() const;

    /**
     * 将值转换为 int64。
     * @return 转换后的整数。
     * @throw Error 值未绑定上下文或无法转换为 int64 时抛出。
     */
    int64_t toInt64() const;

    /**
     * 将值转换为 double。
     * @return 转换后的浮点数。
     * @throw Error 值未绑定上下文或无法转换为 double 时抛出。
     */
    double toDouble() const;

    /**
     * 将值转换为 bool，遵循 JS 的真值判定规则。
     * @return 转换后的布尔值。
     * @throw Error 值未绑定上下文或转换失败时抛出。
     */
    bool toBool() const;

    /**
     * 将值转换为 UTF-8 字符串。
     * @return 转换后的字符串。
     * @throw Error 值未绑定上下文或无法转换为字符串时抛出。
     */
    std::string toString() const;

    /**
     * 将值序列化为 JSON 文本。
     * @return JSON 字符串；值为 undefined 时返回 "undefined"。
     * @throw Error 值未绑定上下文或序列化失败（如存在循环引用）时抛出。
     */
    std::string toJson() const;

    /**
     * 按属性名读取成员。
     * @param name 属性名。
     * @return 读到的属性值，属性不存在时为 undefined。
     * @throw Error 值未绑定上下文或读取过程抛出 JS 异常时抛出。
     */
    Value get( const std::string &name ) const;

    /**
     * 按下标读取数组成员。
     * @param index 数组下标。
     * @return 读到的元素值，下标越界时为 undefined。
     * @throw Error 值未绑定上下文或读取过程抛出 JS 异常时抛出。
     */
    Value get( uint32_t index ) const;

    /**
     * 按属性名写入成员，值按引用计数规则被复制。
     * @param name 属性名。
     * @param value 待写入的值。
     * @throw Error 值未绑定上下文或写入过程抛出 JS 异常时抛出。
     */
    void set( const std::string &name, const Value &value );

    /**
     * 按下标写入数组成员，值按引用计数规则被复制。
     * @param index 数组下标。
     * @param value 待写入的值。
     * @throw Error 值未绑定上下文或写入过程抛出 JS 异常时抛出。
     */
    void set( uint32_t index, const Value &value );

    /**
     * 以 undefined 作为 this 调用当前函数值。
     * @param args 参数列表。
     * @return 调用结果。
     * @throw Error 值未绑定上下文或调用过程抛出 JS 异常时抛出。
     */
    Value call( const std::vector<Value> &args ) const;

    /**
     * 指定 this 调用当前函数值。
     * @param thisObj 调用时绑定的 this 对象，无效值时使用 undefined。
     * @param args 参数列表。
     * @return 调用结果。
     * @throw Error 值未绑定上下文或调用过程抛出 JS 异常时抛出。
     */
    Value call( const Value &thisObj, const std::vector<Value> &args ) const;

    /**
     * 获取内部持有的 JS 上下文。
     * @return 上下文指针，无效值返回 nullptr。
     */
    JSContext *context() const;

    /**
     * 获取内部持有的原生 JSValue，仅供直接调用 QuickJS C API 时使用。
     * @return 原生值，不转移所有权，调用方不得释放。
     */
    JSValue raw() const;

private:
    friend class Engine;

    void reset();
    JSValue release();

    JSContext *m_ctx = nullptr;
    JSValue m_value = JS_UNDEFINED;
};

/**
 * JavaScript 引擎封装，持有 JSRuntime 与 JSContext 并管理其完整生命周期。
 * 禁止拷贝与移动，析构时按标准顺序释放处理器、上下文与运行时。
 * 所有失败路径统一抛出 Error。
 */
class Engine {
public:
    /**
     * C++ 回调签名：第一个参数为引擎自身，第二个参数为 JS 侧传入的参数。
     */
    using Function = std::function<Value( Engine &engine, const std::vector<Value> &args )>;

    /**
     * 引擎初始化参数。
     */
    struct Options {
        size_t memoryLimit;
        size_t maxStackSize;

        /**
         * 构造初始化参数，默认值为常用的安全上限。
         * @param memoryLimit 运行时内存上限（字节），传 0 表示不限制。
         * @param maxStackSize JS 执行栈上限（字节），传 0 表示使用默认值。
         */
        Options( size_t memoryLimit = 64 * 1024 * 1024, size_t maxStackSize = 1024 * 1024 )
            : memoryLimit( memoryLimit ), maxStackSize( maxStackSize ) {
        }
    };

    /**
     * 创建运行时、初始化标准库处理器并创建上下文。
     * @param options 内存与栈上限配置。
     * @throw Error 运行时或上下文创建失败时抛出。
     */
    explicit Engine( const Options &options = Options() );

    /**
     * 析构函数，按标准顺序释放标准库处理器、上下文与运行时。
     * 调用前必须保证所有 Value 已销毁。
     */
    ~Engine();

    Engine( const Engine &other ) = delete;
    Engine( Engine &&other ) = delete;
    Engine &operator=( const Engine &other ) = delete;
    Engine &operator=( Engine &&other ) = delete;

    /**
     * 获取底层运行时。
     * @return JSRuntime 指针，仅用于调用 QuickJS C API。
     */
    JSRuntime *runtime() const;

    /**
     * 获取底层上下文。
     * @return JSContext 指针，仅用于调用 QuickJS C API。
     */
    JSContext *context() const;

    /**
     * 注入 console.log、print、scriptArgs 等标准助手。
     * @param argc 命令行参数个数，默认 0 表示不注入 scriptArgs。
     * @param argv 命令行参数数组。
     */
    void addStdHelpers( int argc = 0, char **argv = nullptr );

    /**
     * 启用 ES Module 支持，注册模块加载器并内置 std、os 模块。
     */
    void enableModules();

    /**
     * 执行事件循环，处理未完成的 Promise、定时器与 IO 回调。
     */
    void runEventLoop();

    /**
     * 执行一段 JS 源码。
     * @param source 源码文本。
     * @param filename 文件名，用于错误堆栈定位，默认 "<eval>"。
     * @param asModule 为 true 时按 ES Module 执行并等待顶层 await 完成。
     * @return 脚本的完成值；脚本无返回值时为 undefined。
     * @throw Error 编译或执行失败时抛出，包含异常文本与堆栈。
     */
    Value eval( const std::string &source, const std::string &filename = "<eval>", bool asModule = false );

    /**
     * 读取并执行一个 JS 脚本文件。
     * @param path 脚本文件路径。
     * @param asModule 为 true 时强制按模块执行；为 false 时 .mjs 后缀仍按模块执行。
     * @return 脚本的完成值。
     * @throw Error 文件读取失败或执行失败时抛出。
     */
    Value evalFile( const std::string &path, bool asModule = false );

    /**
     * 获取全局对象。
     * @return 指向全局对象的值。
     */
    Value global() const;

    /**
     * 在全局对象上定义属性。
     * @param name 属性名。
     * @param value 属性值。
     * @throw Error 定义过程抛出 JS 异常时抛出。
     */
    void setGlobal( const std::string &name, const Value &value );

    /**
     * 用 C++ 可调用对象创建一个 JS 函数值，不自动挂载到全局。
     * 回调内抛出 C++ 异常会被转换为 JS 异常；也可返回 exception() 的结果。
     * @param name 函数名，用于错误堆栈显示。
     * @param function 回调实现。
     * @param length 函数声明的形参个数，供 JS 侧读取 length 属性。
     * @return 新建的函数值。
     * @throw Error 函数创建失败时抛出。
     */
    Value newFunction( const std::string &name, Function function, int length = 0 );

    /**
     * 用 C++ 可调用对象创建一个 JS 函数并挂载到全局对象。
     * @param name 同时用作全局属性名与函数名。
     * @param function 回调实现。
     * @param length 函数声明的形参个数。
     * @throw Error 函数创建或挂载失败时抛出。
     */
    void registerFunction( const std::string &name, Function function, int length = 0 );

    /**
     * 创建一个空的 JS 对象。
     * @return 新建的对象值。
     */
    Value newObject() const;

    /**
     * 创建一个空的 JS 数组。
     * @return 新建的数组值。
     */
    Value newArray() const;

    /**
     * 用文本创建 JS 字符串，按原始字节长度保留内容。
     * @param text 字符串内容。
     * @return 新建的字符串值。
     */
    Value newString( const std::string &text ) const;

    /**
     * 创建 int32 数值。
     * @param value 整数值。
     * @return 新建的数字值。
     */
    Value newInt( int32_t value ) const;

    /**
     * 创建 double 数值。
     * @param value 浮点数值。
     * @return 新建的数字值。
     */
    Value newNumber( double value ) const;

    /**
     * 创建布尔值。
     * @param value 布尔内容。
     * @return 新建的布尔值。
     */
    Value newBool( bool value ) const;

    /**
     * 创建 null 值。
     * @return null 值。
     */
    Value newNull() const;

    /**
     * 创建 undefined 值。
     * @return undefined 值。
     */
    Value newUndefined() const;

    /**
     * 构造一个内部错误值，可在 C++ 回调中返回以向 JS 抛出异常。
     * @param message 错误信息。
     * @return 异常值，返回后当前上下文处于异常状态。
     */
    Value exception( const std::string &message ) const;

    /**
     * 取出当前上下文的异常并清空异常状态。
     * @return 异常文本，若为错误对象则附带堆栈信息。
     */
    std::string takeExceptionMessage();

    /**
     * 将当前上下文的异常打印到标准错误输出并清空异常状态。
     */
    void dumpError() const;

private:
    static JSValue functionTrampoline( JSContext *ctx, JSValueConst this_val, int argc,
                                       JSValueConst *argv, int magic );

    std::vector<Function> m_functions;
    JSRuntime *m_rt = nullptr;
    JSContext *m_ctx = nullptr;
};

} // namespace qjs

#endif // QUICKJS_HPP