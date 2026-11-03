#pragma once

#include "cpp/QuickJS.hpp"
#include "webview/Widget.h"
#include "webview/WebView.h"
#include "webview/WebviewWidget.h"
#include "webview/WebviewWindow.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace webviewjs {
namespace detail {

struct ClassBinding;

/**
 * 回调表。由桥接句柄与 C++ 侧转发 lambda 共享持有，
 * 使句柄被回收后转发逻辑仍能安全判断“回调已失效”。
 */
struct CallbackStore {
    std::unordered_map<std::string, JSValue> map;
};

using CallbackStorePtr = std::shared_ptr<CallbackStore>;

/**
 * JS 对象与 C++ 对象之间的桥接句柄。
 * 只保存对象本身与它所属的类描述：一个对象能被当作 Widget 还是 WebView，
 * 由类描述决定，无需为每个类都维护一组冗余的基类指针。
 * owned 为 true 表示 C++ 对象由 JS 侧拥有，GC 终结时销毁；
 * 为 false 表示对象由其它对象持有（如 WebviewWindow 的子 WebView），仅借用。
 */
struct Handle {
    const ClassBinding *binding = nullptr;
    void *object = nullptr;
    bool owned = true;
    bool released = false;
    CallbackStorePtr callbacks = std::make_shared<CallbackStore>();
};

/**
 * 存活句柄登记表，按运行时分组。
 * 句柄持有 JS 回调引用，回调闭包又常引用 JS 侧的原生对象，二者构成引用环，
 * 仅靠引用计数无法释放，需要在对应引擎销毁前统一断开。
 */
extern std::unordered_map<JSRuntime *, std::vector<Handle *>> g_handles;

/**
 * 方法表构造器，避免使用依赖指定初始化器的宏，兼容各编译器。
 * 同名方法会被后加入的定义替换，避免 JS_SetPropertyFunctionList 遇到重复属性时中止。
 */
class MethodTable {
public:
    MethodTable &add( const char *name, int length, JSCFunction *fn ) {
        JSCFunctionListEntry entry{};
        entry.name = name;
        entry.prop_flags = JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE;
        entry.def_type = JS_DEF_CFUNC;
        entry.magic = 0;
        entry.u.func.length = static_cast<uint8_t>( length );
        entry.u.func.cproto = JS_CFUNC_generic;
        entry.u.func.cfunc.generic = fn;
        for ( auto &existing : m_entries ) {
            if ( std::strcmp( existing.name, name ) == 0 ) {
                existing = entry;
                return *this;
            }
        }
        m_entries.push_back( entry );
        return *this;
    }

    const JSCFunctionListEntry *data() const {
        return m_entries.data();
    }

    int size() const {
        return static_cast<int>( m_entries.size() );
    }

private:
    std::vector<JSCFunctionListEntry> m_entries;
};

/**
 * 一个原生类的完整绑定描述。
 * 每个绑定类在自己的源文件内持有唯一实例：类 ID 在首次访问时生成，
 * 名称、构造函数、方法表与父类也由该文件决定，主入口只需把它们收集起来。
 *
 * 方法表以指针保存：QuickJS 的属性表项惰性初始化时会缓存数组元素地址，
 * 表必须与运行时同寿命，不能按值传递临时对象。
 */
struct ClassBinding {
    JSClassID id = JS_INVALID_CLASS_ID;
    const char *name = nullptr;
    JSCFunction *ctor = nullptr;
    const MethodTable *methods = nullptr;
    const MethodTable *statics = nullptr;
    const ClassBinding *parent = nullptr;

    /**
     * 把对象指针转换成方法实现所需的基类视图，不支持该视图时留空。
     * 例如 WebView 不是 Widget，其 asWidget 为空；
     * WebviewWidget 同时派生自两者，两个转换都提供。
     * 多重继承下基类子对象有偏移，必须由类自己完成转换，不能直接 reinterpret_cast。
     */
    Widget *(*asWidget)( void * ) = nullptr;
    WebView *(*asWebView)( void * ) = nullptr;
    WebviewWidget *(*asWebviewWidget)( void * ) = nullptr;
    WebviewWindow *(*asWebviewWindow)( void * ) = nullptr;

    /**
     * 销毁对象。所有权为 JS 侧时由 GC 终结器调用。
     * 由类自己提供，避免在终结器里对 void* 做错误的类型还原。
     */
    void (*destroy)( void * ) = nullptr;
};

/**
 * 注册一组类绑定。
 * 沿 parent 链递归，保证父类先于子类注册（子类原型需继承父类原型）；
 * 已注册的类会被跳过，因此可重复调用。
 */
void registerClasses( JSContext *ctx, JSRuntime *rt, const std::vector<const ClassBinding *> &classes );

bool isOurClass( JSClassID id );

/**
 * 取得桥接句柄。子类原型链会复用父类方法（例如 WebviewWidget 继承 Widget 方法），
 * 因此不能按固定 class_id 查找，只能先取 opaque 再校验 class_id 是否属于本模块。
 */
Handle *anyHandle( JSContext *ctx, JSValueConst thisVal );
Widget *widgetOf( JSContext *ctx, JSValueConst thisVal );
WebView *webviewOf( JSContext *ctx, JSValueConst thisVal );

/**
 * 同上，但直接接受已取得的句柄，且不抛异常，仅返回 nullptr 表示不支持。
 * 供需要同时访问句柄回调表的实现使用。
 */
Widget *widgetOf( Handle *h );
WebView *webviewOf( Handle *h );
WebviewWidget *webviewWidgetOf( Handle *h );
WebviewWindow *webviewWindowOf( Handle *h );

std::string argString( JSContext *ctx, JSValueConst val, const std::string &def = {} );
int32_t argInt32( JSContext *ctx, JSValueConst val, int32_t def = 0 );
int64_t argInt64( JSContext *ctx, JSValueConst val, int64_t def = 0 );
double argDouble( JSContext *ctx, JSValueConst val, double def = 0.0 );
bool argBool( JSContext *ctx, JSValueConst val, bool def = false );

/**
 * JS 值转 boost::json，借助 JSON.stringify 完成转换。
 * undefined/函数等不可序列化值返回 null。
 */
json::value jsToJson( JSContext *ctx, JSValueConst val );

/**
 * boost::json 转 JS 值，借助 JSON.parse 完成转换，异常时返回 undefined。
 */
JSValue jsonToJS( JSContext *ctx, const json::value &val );

JSValue newString( JSContext *ctx, const std::string &str );
JSValue newStringArray( JSContext *ctx, const std::vector<std::string> &items );
json::object mapToJson( const std::map<std::string, std::string> &map );

/**
 * 保存/替换回调函数，句柄内部持有一次引用计数，销毁时统一释放。
 * 回调表通过共享指针传递给 C++ 侧的转发 lambda，这样即使 JS 对象先被回收，
 * 转发逻辑仍只访问回调表而不会触碰已释放的句柄。
 */
void setCallback( JSContext *ctx, const CallbackStorePtr &store, const std::string &key, JSValueConst fn );
void removeCallback( JSContext *ctx, const CallbackStorePtr &store, const std::string &key );

/**
 * 调用句柄上保存的 JS 回调，异常仅记录日志，不向 C++ 层扩散。
 */
void invokeCallback( JSContext *ctx, const CallbackStorePtr &store, const std::string &key, int argc,
                     JSValueConst *argv );

/**
 * 创建桥接句柄。owned 为 false 时表示仅借用底层对象，GC 时不销毁。
 */
Handle *newHandle( const ClassBinding &binding, void *object, bool owned = true );

JSValue wrapHandle( JSContext *ctx, const ClassBinding &binding, Handle *h );

/**
 * 为其它对象持有的 C++ 对象创建借用句柄，GC 时不销毁底层对象。
 */
JSValue wrapBorrowed( JSContext *ctx, const ClassBinding &binding, void *ptr );

void handleFinalizer( JSRuntime *rt, JSValue val );

/**
 * WebviewWidget/WebviewWindow 异步创建完成后的 JS 回调转发。
 */
void forwardCreated( JSContext *ctx, const CallbackStorePtr &store, bool success );

/**
 * 注册一个不可构造的全局对象（如 App），方法直接挂载在对象自身上。
 */
void registerObject( JSContext *ctx, const char *name, const MethodTable &methods );

/**
 * 把一组函数直接挂到全局对象上（如 setTimeout、clearTimeout）。
 * 同名属性会被覆盖，因此可用于替换标准库 helpers 提供的实现。
 */
void registerGlobals( JSContext *ctx, const MethodTable &functions );

/**
 * 执行运行时的待处理作业（Promise 回调等）。
 * 事件循环每轮都需要调用，否则 await 之后的代码永远不会继续执行。
 */
void pumpJobs( JSRuntime *rt );

/**
 * 断开所有句柄持有的 JS 回调引用。
 * 回调表由句柄与 C++ 侧转发 lambda 共享持有，这里只清空内容并释放引用，
 * 使后续转发逻辑发现回调表为空而安全跳过；句柄本身由随后的 GC 回收。
 */
void releaseHandleCallbacks( JSContext *ctx );

} // namespace detail
} // namespace webviewjs