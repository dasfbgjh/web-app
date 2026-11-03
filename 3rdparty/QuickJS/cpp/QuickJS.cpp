#include "QuickJS.hpp"

#include "quickjs-libc.h"

#include <utility>

namespace qjs {

std::string exceptionToString( JSContext *ctx, JSValue exception ) {
    const char *str = JS_ToCString( ctx, exception );
    std::string message = str ? str : "未知的 JS 异常";
    if ( str ) {
        JS_FreeCString( ctx, str );
    }
    return message;
}

Value::Value( JSContext *ctx, JSValue value ) : m_ctx( ctx ), m_value( value ) {
}

Value::Value( const Value &other ) : m_ctx( other.m_ctx ), m_value( other.m_value ) {
    if ( m_ctx ) {
        m_value = JS_DupValue( m_ctx, m_value );
    }
}

Value::Value( Value &&other ) noexcept : m_ctx( other.m_ctx ), m_value( other.m_value ) {
    other.m_ctx = nullptr;
    other.m_value = JS_UNDEFINED;
}

Value &Value::operator=( const Value &other ) {
    if ( this != &other ) {
        reset();
        m_ctx = other.m_ctx;
        m_value = other.m_value;
        if ( m_ctx ) {
            m_value = JS_DupValue( m_ctx, m_value );
        }
    }
    return *this;
}

Value &Value::operator=( Value &&other ) noexcept {
    if ( this != &other ) {
        reset();
        m_ctx = other.m_ctx;
        m_value = other.m_value;
        other.m_ctx = nullptr;
        other.m_value = JS_UNDEFINED;
    }
    return *this;
}

Value::~Value() {
    reset();
}

void Value::reset() {
    if ( m_ctx ) {
        JS_FreeValue( m_ctx, m_value );
        m_ctx = nullptr;
    }
    m_value = JS_UNDEFINED;
}

JSValue Value::release() {
    JSValue value = m_value;
    m_ctx = nullptr;
    m_value = JS_UNDEFINED;
    return value;
}

bool Value::isValid() const {
    return m_ctx != nullptr;
}

bool Value::isUndefined() const {
    return JS_IsUndefined( m_value );
}

bool Value::isNull() const {
    return JS_IsNull( m_value );
}

bool Value::isBool() const {
    return JS_IsBool( m_value );
}

bool Value::isNumber() const {
    return JS_IsNumber( m_value );
}

bool Value::isString() const {
    return JS_IsString( m_value );
}

bool Value::isObject() const {
    return JS_IsObject( m_value );
}

bool Value::isArray() const {
    return m_ctx != nullptr && JS_IsArray( m_ctx, m_value ) > 0;
}

bool Value::isFunction() const {
    return m_ctx != nullptr && JS_IsFunction( m_ctx, m_value );
}

bool Value::isError() const {
    return m_ctx != nullptr && JS_IsError( m_ctx, m_value );
}

bool Value::isException() const {
    return JS_IsException( m_value );
}

int32_t Value::toInt32() const {
    int32_t result = 0;
    if ( !m_ctx || JS_ToInt32( m_ctx, &result, m_value ) < 0 ) {
        throw Error( "JS 值无法转换为 int32" );
    }
    return result;
}

int64_t Value::toInt64() const {
    int64_t result = 0;
    if ( !m_ctx || JS_ToInt64( m_ctx, &result, m_value ) < 0 ) {
        throw Error( "JS 值无法转换为 int64" );
    }
    return result;
}

double Value::toDouble() const {
    double result = 0;
    if ( !m_ctx || JS_ToFloat64( m_ctx, &result, m_value ) < 0 ) {
        throw Error( "JS 值无法转换为 double" );
    }
    return result;
}

bool Value::toBool() const {
    int result = -1;
    if ( !m_ctx || ( result = JS_ToBool( m_ctx, m_value ) ) < 0 ) {
        throw Error( "JS 值无法转换为 bool" );
    }
    return result != 0;
}

std::string Value::toString() const {
    if ( !m_ctx ) {
        throw Error( "JS 值未绑定上下文" );
    }
    const char *str = JS_ToCString( m_ctx, m_value );
    if ( !str ) {
        throw Error( "JS 值无法转换为字符串" );
    }
    std::string text = str;
    JS_FreeCString( m_ctx, str );
    return text;
}

std::string Value::toJson() const {
    if ( !m_ctx ) {
        throw Error( "JS 值未绑定上下文" );
    }
    JSValue json = JS_JSONStringify( m_ctx, m_value, JS_UNDEFINED, JS_UNDEFINED );
    if ( JS_IsException( json ) ) {
        throw Error( exceptionToString( m_ctx, JS_GetException( m_ctx ) ) );
    }
    if ( JS_IsUndefined( json ) ) {
        return "undefined";
    }
    std::string text = Value( m_ctx, json ).toString();
    return text;
}

Value Value::get( const std::string &name ) const {
    if ( !m_ctx ) {
        throw Error( "JS 值未绑定上下文" );
    }
    JSValue value = JS_GetPropertyStr( m_ctx, m_value, name.c_str() );
    if ( JS_IsException( value ) ) {
        throw Error( exceptionToString( m_ctx, JS_GetException( m_ctx ) ) );
    }
    return Value( m_ctx, value );
}

Value Value::get( uint32_t index ) const {
    if ( !m_ctx ) {
        throw Error( "JS 值未绑定上下文" );
    }
    JSValue value = JS_GetPropertyUint32( m_ctx, m_value, index );
    if ( JS_IsException( value ) ) {
        throw Error( exceptionToString( m_ctx, JS_GetException( m_ctx ) ) );
    }
    return Value( m_ctx, value );
}

void Value::set( const std::string &name, const Value &value ) {
    if ( !m_ctx ) {
        throw Error( "JS 值未绑定上下文" );
    }
    if ( JS_SetPropertyStr( m_ctx, m_value, name.c_str(), JS_DupValue( m_ctx, value.raw() ) ) < 0 ) {
        throw Error( exceptionToString( m_ctx, JS_GetException( m_ctx ) ) );
    }
}

void Value::set( uint32_t index, const Value &value ) {
    if ( !m_ctx ) {
        throw Error( "JS 值未绑定上下文" );
    }
    if ( JS_SetPropertyUint32( m_ctx, m_value, index, JS_DupValue( m_ctx, value.raw() ) ) < 0 ) {
        throw Error( exceptionToString( m_ctx, JS_GetException( m_ctx ) ) );
    }
}

Value Value::call( const std::vector<Value> &args ) const {
    return call( Value(), args );
}

Value Value::call( const Value &thisObj, const std::vector<Value> &args ) const {
    if ( !m_ctx ) {
        throw Error( "调用了一个未绑定上下文的 JS 值" );
    }
    std::vector<JSValue> rawArgs( args.size() );
    for ( size_t i = 0; i < args.size(); ++i ) {
        rawArgs[i] = args[i].raw();
    }
    JSValue thisValue = thisObj.isValid() ? thisObj.raw() : JS_UNDEFINED;
    JSValue result = JS_Call( m_ctx, m_value, thisValue, static_cast<int>( rawArgs.size() ),
                              rawArgs.empty() ? nullptr : rawArgs.data() );
    if ( JS_IsException( result ) ) {
        throw Error( exceptionToString( m_ctx, JS_GetException( m_ctx ) ) );
    }
    return Value( m_ctx, result );
}

JSContext *Value::context() const {
    return m_ctx;
}

JSValue Value::raw() const {
    return m_value;
}

Engine::Engine( const Options &options ) {
    m_rt = JS_NewRuntime();
    if ( !m_rt ) {
        throw Error( "JS_NewRuntime 失败" );
    }
    if ( options.memoryLimit > 0 ) {
        JS_SetMemoryLimit( m_rt, options.memoryLimit );
    }
    if ( options.maxStackSize > 0 ) {
        JS_SetMaxStackSize( m_rt, options.maxStackSize );
    }
    js_std_init_handlers( m_rt );

    m_ctx = JS_NewContext( m_rt );
    if ( !m_ctx ) {
        js_std_free_handlers( m_rt );
        JS_FreeRuntime( m_rt );
        m_rt = nullptr;
        throw Error( "JS_NewContext 失败" );
    }
    JS_SetContextOpaque( m_ctx, this );
}

Engine::~Engine() {
    if ( m_rt ) {
        js_std_free_handlers( m_rt );
    }
    if ( m_ctx ) {
        JS_FreeContext( m_ctx );
        m_ctx = nullptr;
    }
    if ( m_rt ) {
        /* 上下文释放后仍可能存在引用环（例如原生对象持有 JS 回调，回调闭包又引用该对象），
         * 仅靠引用计数无法回收，必须先跑一次 GC 才能释放运行时。 */
        JS_RunGC( m_rt );
        JS_FreeRuntime( m_rt );
        m_rt = nullptr;
    }
}

JSRuntime *Engine::runtime() const {
    return m_rt;
}

JSContext *Engine::context() const {
    return m_ctx;
}

void Engine::addStdHelpers( int argc, char **argv ) {
    js_std_add_helpers( m_ctx, argc, argv );
}

void Engine::enableModules() {
    JS_SetModuleLoaderFunc2( m_rt, nullptr, js_module_loader, js_module_check_attributes, nullptr );
    js_init_module_std( m_ctx, "std" );
    js_init_module_os( m_ctx, "os" );
}

void Engine::runEventLoop() {
    js_std_loop( m_ctx );
}

Value Engine::eval( const std::string &source, const std::string &filename, bool asModule ) {
    if ( asModule ) {
        JSValue compiled = JS_Eval( m_ctx, source.c_str(), source.size(), filename.c_str(),
                                    JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY );
        if ( JS_IsException( compiled ) ) {
            throw Error( takeExceptionMessage() );
        }
        js_module_set_import_meta( m_ctx, compiled, 1, 1 );
        JSValue result = js_std_await( m_ctx, JS_EvalFunction( m_ctx, compiled ) );
        if ( JS_IsException( result ) ) {
            throw Error( takeExceptionMessage() );
        }
        return Value( m_ctx, result );
    }

    JSValue result = JS_Eval( m_ctx, source.c_str(), source.size(), filename.c_str(),
                              JS_EVAL_TYPE_GLOBAL );
    if ( JS_IsException( result ) ) {
        throw Error( takeExceptionMessage() );
    }
    return Value( m_ctx, result );
}

Value Engine::evalFile( const std::string &path, bool asModule ) {
    size_t length = 0;
    uint8_t *buffer = js_load_file( m_ctx, &length, path.c_str() );
    if ( !buffer ) {
        throw Error( "无法读取脚本文件: " + path );
    }
    std::string source( reinterpret_cast<const char *>( buffer ), length );
    js_free( m_ctx, buffer );

    bool module = asModule;
    if ( !module && path.size() > 4 && path.compare( path.size() - 4, 4, ".mjs" ) == 0 ) {
        module = true;
    }
    return eval( source, path, module );
}

Value Engine::global() const {
    return Value( m_ctx, JS_GetGlobalObject( m_ctx ) );
}

void Engine::setGlobal( const std::string &name, const Value &value ) {
    global().set( name, value );
}

Value Engine::newFunction( const std::string &name, Function function, int length ) {
    m_functions.push_back( std::move( function ) );
    int magic = static_cast<int>( m_functions.size() - 1 );

    JSValue fn = JS_NewCFunctionMagic( m_ctx, functionTrampoline, name.c_str(), length,
                                       JS_CFUNC_generic_magic, magic );
    if ( JS_IsException( fn ) ) {
        throw Error( takeExceptionMessage() );
    }
    return Value( m_ctx, fn );
}

void Engine::registerFunction( const std::string &name, Function function, int length ) {
    setGlobal( name, newFunction( name, std::move( function ), length ) );
}

Value Engine::newObject() const {
    return Value( m_ctx, JS_NewObject( m_ctx ) );
}

Value Engine::newArray() const {
    return Value( m_ctx, JS_NewArray( m_ctx ) );
}

Value Engine::newString( const std::string &text ) const {
    return Value( m_ctx, JS_NewStringLen( m_ctx, text.c_str(), text.size() ) );
}

Value Engine::newInt( int32_t value ) const {
    return Value( m_ctx, JS_NewInt32( m_ctx, value ) );
}

Value Engine::newNumber( double value ) const {
    return Value( m_ctx, JS_NewFloat64( m_ctx, value ) );
}

Value Engine::newBool( bool value ) const {
    return Value( m_ctx, JS_NewBool( m_ctx, value ? 1 : 0 ) );
}

Value Engine::newNull() const {
    return Value( m_ctx, JS_NULL );
}

Value Engine::newUndefined() const {
    return Value( m_ctx, JS_UNDEFINED );
}

Value Engine::exception( const std::string &message ) const {
    return Value( m_ctx, JS_ThrowInternalError( m_ctx, "%s", message.c_str() ) );
}

std::string Engine::takeExceptionMessage() {
    if ( !m_ctx ) {
        return "JS 上下文已释放";
    }
    JSValue exception = JS_GetException( m_ctx );
    std::string message = exceptionToString( m_ctx, exception );

    if ( JS_IsError( m_ctx, exception ) ) {
        JSValue stack = JS_GetPropertyStr( m_ctx, exception, "stack" );
        if ( !JS_IsException( stack ) && JS_IsString( stack ) ) {
            const char *str = JS_ToCString( m_ctx, stack );
            if ( str ) {
                message += "\n";
                message += str;
                JS_FreeCString( m_ctx, str );
            }
        }
        JS_FreeValue( m_ctx, stack );
    }

    JS_FreeValue( m_ctx, exception );
    return message;
}

void Engine::dumpError() const {
    js_std_dump_error( m_ctx );
}

JSValue Engine::functionTrampoline( JSContext *ctx, JSValueConst this_val, int argc,
                                    JSValueConst *argv, int magic ) {
    Engine *engine = static_cast<Engine *>( JS_GetContextOpaque( ctx ) );
    if ( !engine || magic < 0 || static_cast<size_t>( magic ) >= engine->m_functions.size() ) {
        return JS_ThrowInternalError( ctx, "无效的 C++ 函数回调" );
    }

    std::vector<Value> args;
    args.reserve( static_cast<size_t>( argc ) );
    for ( int i = 0; i < argc; ++i ) {
        args.emplace_back( ctx, JS_DupValue( ctx, argv[i] ) );
    }

    try {
        Value result = engine->m_functions[static_cast<size_t>( magic )]( *engine, args );
        return result.release();
    } catch ( const std::exception &e ) {
        return JS_ThrowInternalError( ctx, "%s", e.what() );
    } catch ( ... ) {
        return JS_ThrowInternalError( ctx, "C++ 回调抛出未知异常" );
    }
}

} // namespace qjs