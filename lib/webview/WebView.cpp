#include "WebView.h"
#include <fstream>
#include <algorithm>
#include <winver.h>
#include "common/Logger.hpp"
#include "common/Utils.h"

std::vector<WebView *> WebView::s_instances;
std::mutex WebView::s_instancesMutex;

int WebView::s_oleCount = 0;
int WebView::s_dllCount = 0;
HMODULE WebView::s_sharedDll = nullptr;
WebView::CreateEnvFn WebView::s_sharedCreateEnvFn = nullptr;
WebView::CreateEnvInternalFn WebView::s_sharedCreateEnvInternalFn = nullptr;
WebView::RuntimeType WebView::s_sharedRuntimeType = WebView::RuntimeType::Installed;
std::wstring WebView::s_runtimeVersion;

namespace {
template <typename IHandler, REFIID iid>
class ComHandlerBase : public IHandler {
public:
    ComHandlerBase() : m_ref( 1 ) {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void **ppv ) override {
        if ( !ppv )
            return E_POINTER;
        if ( IsEqualIID( riid, iid ) || IsEqualIID( riid, IID_IUnknown ) ) {
            *ppv = static_cast<IHandler *>( this );
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++m_ref;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = --m_ref;
        if ( count == 0 )
            delete this;
        return count;
    }

protected:
    std::atomic<ULONG> m_ref;
};

class EnvCompletedHandler
    : public ComHandlerBase<
          ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
          IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
public:
    using Callback = std::function<void( HRESULT, ICoreWebView2Environment * )>;
    EnvCompletedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( HRESULT hr, ICoreWebView2Environment *env ) override {
        Callback cb = std::move( m_cb );
        m_cb = nullptr;
        if ( cb )
            cb( hr, env );
        return S_OK;
    }

private:
    Callback m_cb;
};

class EnvOptionsHandler : public ICoreWebView2EnvironmentOptions {
public:
    EnvOptionsHandler( const std::wstring &args, const std::wstring &version )
        : m_ref( 1 ), m_args( args ), m_targetVersion( version.empty() ? L"1.0.0.0" : version ) {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void **ppv ) override {
        if ( !ppv )
            return E_POINTER;
        if ( IsEqualIID( riid, IID_ICoreWebView2EnvironmentOptions ) || IsEqualIID( riid, IID_IUnknown ) ) {
            *ppv = static_cast<ICoreWebView2EnvironmentOptions *>( this );
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++m_ref;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = --m_ref;
        if ( c == 0 )
            delete this;
        return c;
    }

    HRESULT STDMETHODCALLTYPE get_AdditionalBrowserArguments( LPWSTR *value ) override {
        if ( !value )
            return E_POINTER;
        *value = (LPWSTR)CoTaskMemAlloc( ( m_args.size() + 1 ) * sizeof( wchar_t ) );
        if ( !*value )
            return E_OUTOFMEMORY;
        wcscpy_s( *value, m_args.size() + 1, m_args.c_str() );
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE put_AdditionalBrowserArguments( LPCWSTR value ) override {
        m_args = value ? value : L"";
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_Language( LPWSTR *value ) override {
        if ( !value )
            return E_POINTER;
        *value = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE put_Language( LPCWSTR ) override {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_TargetCompatibleBrowserVersion( LPWSTR *value ) override {
        if ( !value )
            return E_POINTER;
        *value = (LPWSTR)CoTaskMemAlloc( ( m_targetVersion.size() + 1 ) * sizeof( wchar_t ) );
        if ( !*value )
            return E_OUTOFMEMORY;
        wcscpy_s( *value, m_targetVersion.size() + 1, m_targetVersion.c_str() );
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE put_TargetCompatibleBrowserVersion( LPCWSTR value ) override {
        m_targetVersion = value ? value : L"";
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_AllowSingleSignOnUsingOSPrimaryAccount( BOOL *value ) override {
        if ( !value )
            return E_POINTER;
        *value = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE put_AllowSingleSignOnUsingOSPrimaryAccount( BOOL ) override {
        return S_OK;
    }

private:
    std::atomic<ULONG> m_ref;
    std::wstring m_args;
    std::wstring m_targetVersion;
};

class CtrlCompletedHandler
    : public ComHandlerBase<
          ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
          IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
public:
    using Callback = std::function<void( HRESULT, ICoreWebView2Controller * )>;
    CtrlCompletedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( HRESULT hr, ICoreWebView2Controller *ctrl ) override {
        Callback cb = std::move( m_cb );
        m_cb = nullptr;
        if ( cb )
            cb( hr, ctrl );
        return S_OK;
    }

private:
    Callback m_cb;
};

class NavCompletedHandler
    : public ComHandlerBase<
          ICoreWebView2NavigationCompletedEventHandler,
          IID_ICoreWebView2NavigationCompletedEventHandler> {
public:
    using Callback = std::function<void( ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs * )>;
    NavCompletedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( ICoreWebView2 *sender, ICoreWebView2NavigationCompletedEventArgs *args ) override {
        if ( m_cb )
            m_cb( sender, args );
        return S_OK;
    }

private:
    Callback m_cb;
};

class NavStartingHandler
    : public ComHandlerBase<
          ICoreWebView2NavigationStartingEventHandler,
          IID_ICoreWebView2NavigationStartingEventHandler> {
public:
    using Callback = std::function<void( ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs * )>;
    NavStartingHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( ICoreWebView2 *sender, ICoreWebView2NavigationStartingEventArgs *args ) override {
        if ( m_cb )
            m_cb( sender, args );
        return S_OK;
    }

private:
    Callback m_cb;
};

class AddScriptCompletedHandler
    : public ComHandlerBase<
          ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler,
          IID_ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler> {
public:
    using Callback = std::function<void( HRESULT, LPCWSTR )>;
    AddScriptCompletedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( HRESULT errorCode, LPCWSTR result ) override {
        if ( m_cb )
            m_cb( errorCode, result );
        return S_OK;
    }

private:
    Callback m_cb;
};

class MsgReceivedHandler
    : public ComHandlerBase<
          ICoreWebView2WebMessageReceivedEventHandler,
          IID_ICoreWebView2WebMessageReceivedEventHandler> {
public:
    using Callback = std::function<void( ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs * )>;
    MsgReceivedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args ) override {
        if ( m_cb )
            m_cb( sender, args );
        return S_OK;
    }

private:
    Callback m_cb;
};

class DocTitleChangedHandler
    : public ComHandlerBase<
          ICoreWebView2DocumentTitleChangedEventHandler,
          IID_ICoreWebView2DocumentTitleChangedEventHandler> {
public:
    using Callback = std::function<void( ICoreWebView2 *, IUnknown * )>;
    DocTitleChangedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( ICoreWebView2 *sender, IUnknown *args ) override {
        if ( m_cb )
            m_cb( sender, args );
        return S_OK;
    }

private:
    Callback m_cb;
};

class WinCloseRequestedHandler
    : public ComHandlerBase<
          ICoreWebView2WindowCloseRequestedEventHandler,
          IID_ICoreWebView2WindowCloseRequestedEventHandler> {
public:
    using Callback = std::function<void( ICoreWebView2 *, IUnknown * )>;
    WinCloseRequestedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( ICoreWebView2 *sender, IUnknown *args ) override {
        if ( m_cb )
            m_cb( sender, args );
        return S_OK;
    }

private:
    Callback m_cb;
};

class ExecScriptHandler
    : public ComHandlerBase<
          ICoreWebView2ExecuteScriptCompletedHandler,
          IID_ICoreWebView2ExecuteScriptCompletedHandler> {
public:
    using Callback = std::function<void( HRESULT, LPCWSTR )>;
    ExecScriptHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( HRESULT hr, LPCWSTR result ) override {
        Callback cb = std::move( m_cb );
        m_cb = nullptr;
        if ( cb )
            cb( hr, result );
        return S_OK;
    }

private:
    Callback m_cb;
};

class WebResourceRequestedHandler
    : public ComHandlerBase<
          ICoreWebView2WebResourceRequestedEventHandler,
          IID_ICoreWebView2WebResourceRequestedEventHandler> {
public:
    using Callback = std::function<void( ICoreWebView2 *, ICoreWebView2WebResourceRequestedEventArgs * )>;
    WebResourceRequestedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( ICoreWebView2 *sender, ICoreWebView2WebResourceRequestedEventArgs *args ) override {
        if ( m_cb )
            m_cb( sender, args );
        return S_OK;
    }

private:
    Callback m_cb;
};

class NewWindowRequestedHandler
    : public ComHandlerBase<
          ICoreWebView2NewWindowRequestedEventHandler,
          IID_ICoreWebView2NewWindowRequestedEventHandler> {
public:
    using Callback = std::function<void( ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs * )>;
    NewWindowRequestedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( ICoreWebView2 *sender, ICoreWebView2NewWindowRequestedEventArgs *args ) override {
        if ( m_cb )
            m_cb( sender, args );
        return S_OK;
    }

private:
    Callback m_cb;
};

class WebResourceResponseReceivedHandler
    : public ComHandlerBase<
          ICoreWebView2WebResourceResponseReceivedEventHandler,
          IID_ICoreWebView2WebResourceResponseReceivedEventHandler> {
public:
    using Callback = std::function<void( ICoreWebView2 *, ICoreWebView2WebResourceResponseReceivedEventArgs * )>;
    WebResourceResponseReceivedHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( ICoreWebView2 *sender, ICoreWebView2WebResourceResponseReceivedEventArgs *args ) override {
        if ( m_cb )
            m_cb( sender, args );
        return S_OK;
    }

private:
    Callback m_cb;
};

class ResponseViewGetContentHandler
    : public ComHandlerBase<
          ICoreWebView2WebResourceResponseViewGetContentCompletedHandler,
          IID_ICoreWebView2WebResourceResponseViewGetContentCompletedHandler> {
public:
    using Callback = std::function<void( HRESULT, IStream * )>;
    ResponseViewGetContentHandler( Callback cb ) : m_cb( std::move( cb ) ) {
    }

    HRESULT STDMETHODCALLTYPE Invoke( HRESULT errorCode, IStream *result ) override {
        Callback cb = std::move( m_cb );
        m_cb = nullptr;
        if ( cb )
            cb( errorCode, result );
        return S_OK;
    }

private:
    Callback m_cb;
};

} // anonymous namespace

WebView::WebView() {
    std::lock_guard<std::mutex> lock( s_instancesMutex );
    s_instances.push_back( this );
}

json::value WebView::configToJson( const Config &config ) {
    return json::object{
        { "browserPath", config.browserPath },
        { "userDataDir", config.userDataDir },
        { "initialUrl", config.initialUrl },
        { "additionalBrowserArguments", config.additionalBrowserArguments },
        { "disableWebSecurity", config.disableWebSecurity },
        { "devToolsEnabled", config.devToolsEnabled },
        { "consoleEnabled", config.consoleEnabled },
        { "statusBarEnabled", config.statusBarEnabled },
        { "defaultContextMenusEnabled", config.defaultContextMenusEnabled },
        { "scriptEnabled", config.scriptEnabled },
        { "webMessageEnabled", config.webMessageEnabled },
        { "transparentBackground", config.transparentBackground },
    };
}

WebView::Config WebView::configFromJson( const json::value &value ) {
    Config cfg;
    if ( !value.is_object() )
        return cfg;

    const auto &obj = value.as_object();
    cfg.browserPath = json::jsonGetString( obj, "browserPath", cfg.browserPath );
    cfg.userDataDir = json::jsonGetString( obj, "userDataDir", cfg.userDataDir );
    cfg.initialUrl = json::jsonGetString( obj, "initialUrl", cfg.initialUrl );
    if ( obj.contains( "url" ) )
        cfg.initialUrl = json::jsonGetString( obj, "url", cfg.initialUrl );
    cfg.additionalBrowserArguments = json::jsonGetString( obj, "additionalBrowserArguments", cfg.additionalBrowserArguments );
    cfg.disableWebSecurity = json::jsonGetBool( obj, "disableWebSecurity", cfg.disableWebSecurity );
    cfg.devToolsEnabled = json::jsonGetBool( obj, "devToolsEnabled", cfg.devToolsEnabled );
    cfg.consoleEnabled = json::jsonGetBool( obj, "consoleEnabled", cfg.consoleEnabled );
    cfg.statusBarEnabled = json::jsonGetBool( obj, "statusBarEnabled", cfg.statusBarEnabled );
    cfg.defaultContextMenusEnabled = json::jsonGetBool( obj, "defaultContextMenusEnabled", cfg.defaultContextMenusEnabled );
    cfg.scriptEnabled = json::jsonGetBool( obj, "scriptEnabled", cfg.scriptEnabled );
    cfg.webMessageEnabled = json::jsonGetBool( obj, "webMessageEnabled", cfg.webMessageEnabled );
    cfg.transparentBackground = json::jsonGetBool( obj, "transparentBackground", cfg.transparentBackground );
    return cfg;
}

WebView::~WebView() {
    {
        std::lock_guard<std::mutex> lock( s_instancesMutex );
        auto it = std::find( s_instances.begin(), s_instances.end(), this );
        if ( it != s_instances.end() )
            s_instances.erase( it );
    }
    close();
}

std::wstring WebView::queryRegString( HKEY root, const wchar_t *subKey, const wchar_t *valueName ) const {
    HKEY key = nullptr;
    LONG status = RegOpenKeyExW( root, subKey, 0, KEY_READ | KEY_WOW64_32KEY, &key );
    if ( status != ERROR_SUCCESS )
        return {};

    DWORD dataSize = 0;
    status = RegQueryValueExW( key, valueName, nullptr, nullptr, nullptr, &dataSize );
    if ( status != ERROR_SUCCESS && status != ERROR_MORE_DATA ) {
        RegCloseKey( key );
        return {};
    }

    std::wstring result( dataSize / sizeof( wchar_t ), L'\0' );
    status = RegQueryValueExW( key, valueName, nullptr, nullptr,
                               reinterpret_cast<LPBYTE>( &result[0] ), &dataSize );
    RegCloseKey( key );

    if ( status != ERROR_SUCCESS )
        return {};

    while ( !result.empty() && result.back() == L'\0' )
        result.pop_back();

    return result;
}

std::wstring WebView::getFileVersionString( const std::wstring &path ) {
    DWORD size = GetFileVersionInfoSizeW( path.c_str(), nullptr );
    if ( size == 0 )
        return {};

    std::vector<BYTE> buffer( size );
    if ( !GetFileVersionInfoW( path.c_str(), 0, size, buffer.data() ) )
        return {};

    VS_FIXEDFILEINFO *info = nullptr;
    UINT infoLen = 0;
    if ( !VerQueryValueW( buffer.data(), L"\\", reinterpret_cast<LPVOID *>( &info ), &infoLen ) || !info )
        return {};

    wchar_t version[64];
    swprintf_s( version, L"%d.%d.%d.%d",
                HIWORD( info->dwFileVersionMS ), LOWORD( info->dwFileVersionMS ),
                HIWORD( info->dwFileVersionLS ), LOWORD( info->dwFileVersionLS ) );
    return version;
}

std::wstring WebView::makeClientDllPath( const std::wstring &dir ) const {
    std::wstring path = dir;
    if ( !path.empty() && path.back() != L'\\' && path.back() != L'/' )
        path += L'\\';
    path += L"EBWebView\\";
#if defined( _M_X64 ) || defined( __x86_64__ )
    path += L"x64";
#elif defined( _M_IX86 ) || defined( __i386__ )
    path += L"x86";
#elif defined( _M_ARM64 ) || defined( __aarch64__ )
    path += L"arm64";
#else
    path += L"x64";
#endif
    path += L"\\EmbeddedBrowserWebView.dll";
    return path;
}

WebView::ClientInfo WebView::findInstalledClient( bool system ) const {
    std::wstring subKey = kRegSubKey;
    subKey += kStableGuid;

    HKEY root = system ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    std::wstring ebwebview = queryRegString( root, subKey.c_str(), L"EBWebView" );
    if ( ebwebview.empty() )
        return {};

    std::wstring dllPath = makeClientDllPath( ebwebview );
    if ( GetFileAttributesW( dllPath.c_str() ) == INVALID_FILE_ATTRIBUTES )
        return {};

    return { true, std::move( dllPath ), RuntimeType::Installed };
}

WebView::ClientInfo WebView::findEmbeddedClient( const std::wstring &dir ) const {
    std::wstring dllPath = makeClientDllPath( dir );
    if ( GetFileAttributesW( dllPath.c_str() ) == INVALID_FILE_ATTRIBUTES )
        return {};

    return { true, std::move( dllPath ), RuntimeType::Embedded };
}

bool WebView::tryLoadViaWebView2Loader() {
    std::lock_guard<std::mutex> lock( s_instancesMutex );
    if ( s_sharedDll && s_sharedCreateEnvFn ) {
        m_clientDll = s_sharedDll;
        m_createEnvFn = s_sharedCreateEnvFn;
        m_createEnvInternalFn = nullptr;
        s_dllCount++;
        return true;
    }

    HMODULE dll = LoadLibraryW( L"WebView2Loader.dll" );
    if ( !dll )
        return false;

    auto fn = reinterpret_cast<CreateEnvFn>(
        GetProcAddress( dll, "CreateCoreWebView2EnvironmentWithOptions" ) );
    if ( !fn ) {
        FreeLibrary( dll );
        return false;
    }

    s_sharedDll = dll;
    s_sharedCreateEnvFn = fn;
    s_sharedCreateEnvInternalFn = nullptr;
    m_clientDll = dll;
    m_createEnvFn = fn;
    m_createEnvInternalFn = nullptr;
    s_dllCount++;
    return true;
}

bool WebView::tryLoadBuiltin() {
    std::lock_guard<std::mutex> lock( s_instancesMutex );
    if ( s_sharedDll && s_sharedCreateEnvInternalFn ) {
        m_clientDll = s_sharedDll;
        m_createEnvFn = nullptr;
        m_createEnvInternalFn = s_sharedCreateEnvInternalFn;
        m_runtimeType = s_sharedRuntimeType;
        s_dllCount++;
        return true;
    }

    ClientInfo client;

    if ( !m_config.browserPath.empty() ) {
        client = findEmbeddedClient( utils::strToWiden( m_config.browserPath ) );
    }

    if ( !client.found ) {
        client = findInstalledClient( false );
    }

    if ( !client.found ) {
        client = findInstalledClient( true );
    }

    if ( !client.found )
        return false;

    HMODULE dll = LoadLibraryW( client.dllPath.c_str() );
    if ( !dll )
        return false;

    auto fn = reinterpret_cast<CreateEnvInternalFn>(
        GetProcAddress( dll, "CreateWebViewEnvironmentWithOptionsInternal" ) );
    if ( !fn ) {
        FreeLibrary( dll );
        return false;
    }

    s_sharedDll = dll;
    s_sharedCreateEnvFn = nullptr;
    s_sharedCreateEnvInternalFn = fn;
    s_sharedRuntimeType = client.runtimeType;
    s_runtimeVersion = getFileVersionString( client.dllPath );
    m_clientDll = dll;
    m_createEnvFn = nullptr;
    m_createEnvInternalFn = fn;
    m_runtimeType = client.runtimeType;
    s_dllCount++;
    return true;
}

bool WebView::loadWebView2() {
    if ( m_createEnvFn || m_createEnvInternalFn )
        return true;

    if ( tryLoadViaWebView2Loader() )
        return true;

    if ( tryLoadBuiltin() )
        return true;

    return false;
}

bool WebView::create( HWND parentHwnd, const Config &config, CreatedCallback onCreated ) {
    if ( m_initialized )
        return false;
    if ( !parentHwnd )
        return false;

    {
        std::lock_guard<std::mutex> lock( s_instancesMutex );
        if ( s_oleCount == 0 )
            OleInitialize( nullptr );
        s_oleCount++;
        m_oleInitialized = true;
    }

    m_parentHwnd = parentHwnd;
    m_config = config;

    if ( !loadWebView2() )
        return false;

    std::wstring wBrowserPath = utils::strToWiden( config.browserPath );
    std::wstring wUserDataDir = utils::strToWiden( config.userDataDir );
    if ( wUserDataDir.empty() ) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW( nullptr, exePath, MAX_PATH );
        std::wstring exeDir( exePath );
        auto pos = exeDir.find_last_of( L"\\/" );
        if ( pos != std::wstring::npos )
            exeDir.resize( pos );
        wUserDataDir = exeDir + L"\\config";
    }
    PCWSTR browserPath = wBrowserPath.empty() ? nullptr : wBrowserPath.c_str();
    PCWSTR userDataDir = wUserDataDir.c_str();

    auto envHandler = new EnvCompletedHandler( [this, onCreated]( HRESULT hr, ICoreWebView2Environment *env ) {
        onEnvCompleted( hr, env, onCreated );
    } );

    HRESULT hr = E_FAIL;

    ICoreWebView2EnvironmentOptions *options = nullptr;
    std::wstring argsW;
    if ( m_config.disableWebSecurity ) {
        argsW = L"--disable-web-security";
    }
    if ( !m_config.additionalBrowserArguments.empty() ) {
        if ( !argsW.empty() )
            argsW += L" ";
        argsW += utils::strToWiden( m_config.additionalBrowserArguments );
    }
    EnvOptionsHandler *optionsObj = nullptr;
    if ( !argsW.empty() ) {
        optionsObj = new EnvOptionsHandler( argsW, s_runtimeVersion );
        options = optionsObj;
    }

    if ( m_createEnvFn ) {
        hr = m_createEnvFn( browserPath, userDataDir, options, envHandler );
    } else if ( m_createEnvInternalFn ) {
        int runtimeTypeInt = ( m_runtimeType == RuntimeType::Embedded ) ? 1 : 0;
        hr = m_createEnvInternalFn( true, runtimeTypeInt, userDataDir, options, envHandler );
    }

    LOG_DEBUG << "create webview environment, args: [" << std::string( m_config.additionalBrowserArguments )
              << "], hr: " << (int)hr;

    if ( optionsObj )
        optionsObj->Release();

    if ( FAILED( hr ) ) {
        delete envHandler;
        LOG_WARN << "create webview environment failed, hr: " << (int)hr;
        return false;
    }

    return true;
}

void WebView::close() {
    if ( m_webview ) {
        if ( m_navigationCompletedToken.value != 0 )
            m_webview->remove_NavigationCompleted( m_navigationCompletedToken );
        if ( m_navigationStartingToken.value != 0 )
            m_webview->remove_NavigationStarting( m_navigationStartingToken );
        if ( m_webMessageReceivedToken.value != 0 )
            m_webview->remove_WebMessageReceived( m_webMessageReceivedToken );
        if ( m_documentTitleChangedToken.value != 0 )
            m_webview->remove_DocumentTitleChanged( m_documentTitleChangedToken );
        if ( m_windowCloseRequestedToken.value != 0 )
            m_webview->remove_WindowCloseRequested( m_windowCloseRequestedToken );
        if ( m_webResourceRequestedToken.value != 0 )
            m_webview->remove_WebResourceRequested( m_webResourceRequestedToken );
        if ( m_newWindowRequestedToken.value != 0 )
            m_webview->remove_NewWindowRequested( m_newWindowRequestedToken );
        if ( m_webResourceResponseReceivedToken.value != 0 ) {
            ICoreWebView2_2 *webview2 = nullptr;
            if ( SUCCEEDED( m_webview->QueryInterface( IID_ICoreWebView2_2, (void **)&webview2 ) ) ) {
                webview2->remove_WebResourceResponseReceived( m_webResourceResponseReceivedToken );
                webview2->Release();
            }
        }

        m_webview->Release();
        m_webview = nullptr;
    }

    if ( m_controller ) {
        m_controller->Close();
        m_controller->Release();
        m_controller = nullptr;
    }

    if ( m_settings ) {
        m_settings->Release();
        m_settings = nullptr;
    }

    if ( m_environment ) {
        m_environment->Release();
        m_environment = nullptr;
    }

    m_initialized = false;
    m_parentHwnd = nullptr;

    {
        std::lock_guard<std::mutex> lock( s_instancesMutex );
        if ( m_oleInitialized ) {
            m_oleInitialized = false;
            s_oleCount--;
            if ( s_oleCount <= 0 ) {
                OleUninitialize();
                s_oleCount = 0;
            }
        }
        if ( m_clientDll ) {
            m_clientDll = nullptr;
            m_createEnvFn = nullptr;
            m_createEnvInternalFn = nullptr;
            s_dllCount--;
            if ( s_dllCount <= 0 && s_sharedDll ) {
                FreeLibrary( s_sharedDll );
                s_sharedDll = nullptr;
                s_sharedCreateEnvFn = nullptr;
                s_sharedCreateEnvInternalFn = nullptr;
                s_dllCount = 0;
            }
        }
    }
}

bool WebView::navigate( const std::string &url ) {
    if ( !m_webview )
        return false;
    std::wstring wurl = utils::strToWiden( url );
    m_webview->Navigate( wurl.c_str() );
    return true;
}

bool WebView::navigateToString( const std::string &html ) {
    if ( !m_webview )
        return false;
    std::wstring whtml = utils::strToWiden( html );
    m_webview->NavigateToString( whtml.c_str() );
    return true;
}

void WebView::executeScript( const std::string &script, ScriptResultCallback callback ) {
    if ( !m_webview )
        return;

    std::wstring wscript = utils::strToWiden( script );

    if ( callback ) {
        auto handler = new ExecScriptHandler(
            [this, cb = std::move( callback )]( HRESULT hr, LPCWSTR result ) {
                std::wstring wres = ( SUCCEEDED( hr ) && result ) ? result : L"";
                cb( utils::strToNarrow( wres ), this );
            } );
        m_webview->ExecuteScript( wscript.c_str(), handler );
    } else {
        m_webview->ExecuteScript( wscript.c_str(), nullptr );
    }
}

bool WebView::executeScript( const std::string &name, const std::string &script, ScriptResultCallback callback ) {
    std::lock_guard<std::mutex> lock( s_instancesMutex );
    for ( auto *inst : s_instances ) {
        if ( inst->m_name == name ) {
            inst->executeScript( script, callback );
            return true;
        }
    }
    return false;
}

void WebView::addScriptToExecuteOnDocumentCreated( const std::string &script, ScriptResultCallback callback ) {
    if ( !m_webview )
        return;

    std::wstring wscript = utils::strToWiden( script );

    if ( callback ) {
        auto handler = new AddScriptCompletedHandler(
            [this, cb = std::move( callback )]( HRESULT hr, LPCWSTR id ) {
                std::wstring wid = ( SUCCEEDED( hr ) && id ) ? id : L"";
                cb( utils::strToNarrow( wid ), this );
            } );
        m_webview->AddScriptToExecuteOnDocumentCreated( wscript.c_str(), handler );
    } else {
        m_webview->AddScriptToExecuteOnDocumentCreated( wscript.c_str(), nullptr );
    }
}

void WebView::postWebMessage( const json::value &json ) {
    if ( m_webview ) {
        std::string str = json::serialize( json );
        std::wstring wjson = utils::strToWiden( str );
        m_webview->PostWebMessageAsJson( wjson.c_str() );
    }
}

void WebView::resize( int x, int y, int width, int height ) {
    if ( m_controller ) {
        RECT bounds = { x, y, x + width, y + height };
        m_controller->put_Bounds( bounds );
    }
}

void WebView::setVisible( bool visible ) {
    if ( m_controller )
        m_controller->put_IsVisible( visible ? TRUE : FALSE );
}

bool WebView::isVisible() const {
    if ( !m_controller )
        return false;
    BOOL vis = FALSE;
    m_controller->get_IsVisible( &vis );
    return vis != FALSE;
}

void WebView::setZoomFactor( double factor ) {
    if ( m_controller )
        m_controller->put_ZoomFactor( factor );
}

double WebView::getZoomFactor() const {
    if ( !m_controller )
        return 1.0;
    double factor = 1.0;
    m_controller->get_ZoomFactor( &factor );
    return factor;
}

void WebView::goBack() {
    if ( m_webview )
        m_webview->GoBack();
}

void WebView::goForward() {
    if ( m_webview )
        m_webview->GoForward();
}

bool WebView::canGoBack() const {
    if ( !m_webview )
        return false;
    BOOL can = FALSE;
    m_webview->get_CanGoBack( &can );
    return can != FALSE;
}

bool WebView::canGoForward() const {
    if ( !m_webview )
        return false;
    BOOL can = FALSE;
    m_webview->get_CanGoForward( &can );
    return can != FALSE;
}

void WebView::reload() {
    if ( m_webview )
        m_webview->Reload();
}

void WebView::stop() {
    if ( m_webview )
        m_webview->Stop();
}

void WebView::openDevToolsWindow() {
    if ( m_webview )
        m_webview->OpenDevToolsWindow();
}

bool WebView::openDevToolsWindow( const std::string &name ) {
    std::lock_guard<std::mutex> lock( s_instancesMutex );
    for ( auto *inst : s_instances ) {
        if ( inst->m_name == name ) {
            inst->openDevToolsWindow();
            return true;
        }
    }
    return false;
}

std::string WebView::getSource() const {
    if ( !m_webview )
        return {};
    LPWSTR src = nullptr;
    m_webview->get_Source( &src );
    return coMemToStr( src );
}

std::string WebView::getDocumentTitle() const {
    if ( !m_webview )
        return {};
    LPWSTR title = nullptr;
    m_webview->get_DocumentTitle( &title );
    return coMemToStr( title );
}

std::string WebView::getBrowserVersion() const {
    if ( !m_environment )
        return {};
    LPWSTR ver = nullptr;
    m_environment->get_BrowserVersionString( &ver );
    return coMemToStr( ver );
}

void WebView::setName( const std::string &name ) {
    m_name = name;
}

std::string WebView::getName() const {
    return m_name;
}

void WebView::setNavigationCompletedCallback( NavigationCompletedCallback callback ) {
    m_onNavigationCompleted = std::move( callback );
}

void WebView::setNavigationStartingCallback( NavigationStartingCallback callback ) {
    m_onNavigationStarting = std::move( callback );
}

void WebView::setWebMessageReceivedCallback( WebMessageReceivedCallback callback ) {
    m_onWebMessageReceived = std::move( callback );
}

void WebView::setDocumentTitleChangedCallback( DocumentTitleChangedCallback callback ) {
    m_onDocumentTitleChanged = std::move( callback );
}

ICoreWebView2 *WebView::getCoreWebView() const {
    return m_webview;
}

ICoreWebView2Controller *WebView::getController() const {
    return m_controller;
}

ICoreWebView2Environment *WebView::getEnvironment() const {
    return m_environment;
}

HWND WebView::getParentHwnd() const {
    return m_parentHwnd;
}

HWND WebView::getRenderHwnd() const {
    if ( !m_parentHwnd )
        return nullptr;

    HWND result = nullptr;
    EnumChildWindows( m_parentHwnd, []( HWND child, LPARAM lParam ) -> BOOL {
        wchar_t className[256] = {};
        GetClassNameW( child, className, 256 );
        if ( wcscmp( className, L"Chrome_WidgetWin_0" ) == 0 ) {
            *reinterpret_cast<HWND *>( lParam ) = child;
            return FALSE;
        }
        return TRUE; }, reinterpret_cast<LPARAM>( &result ) );

    return result;
}

std::string WebView::coMemToStr( LPWSTR str ) const {
    if ( !str )
        return {};
    std::wstring wstr = str;
    CoTaskMemFree( str );
    return utils::strToNarrow( wstr );
}

bool WebView::setUserAgent( const std::string &userAgent ) {
    if ( !m_settings )
        return false;

    ICoreWebView2Settings2 *settings2 = nullptr;
    HRESULT hr = m_settings->QueryInterface( IID_ICoreWebView2Settings2, reinterpret_cast<void **>( &settings2 ) );
    if ( FAILED( hr ) || !settings2 )
        return false;

    std::wstring wua = utils::strToWiden( userAgent );
    hr = settings2->put_UserAgent( wua.c_str() );
    settings2->Release();
    return SUCCEEDED( hr );
}

std::string WebView::getUserAgent() const {
    if ( !m_settings )
        return {};

    ICoreWebView2Settings2 *settings2 = nullptr;
    HRESULT hr = m_settings->QueryInterface( IID_ICoreWebView2Settings2, reinterpret_cast<void **>( &settings2 ) );
    if ( FAILED( hr ) || !settings2 )
        return {};

    LPWSTR ua = nullptr;
    hr = settings2->get_UserAgent( &ua );
    settings2->Release();
    if ( FAILED( hr ) )
        return {};

    return coMemToStr( ua );
}

void WebView::bind( const std::string &name, NativeCallback callback ) {
    {
        // 创建完成时自动注入
        std::lock_guard<std::mutex> lock( m_bindingsMutex );
        m_bindings[name] = std::move( callback );
    }
    if ( m_webview ) {
        std::string js = generateInjectBindings( { { name, callback } } );
        // 当前立即注入
        executeScript( js );
        // 以后刷新时注入
        std::wstring wJs = utils::strToWiden( js );
        m_webview->AddScriptToExecuteOnDocumentCreated( wJs.c_str(), nullptr );
    }
}

void WebView::unbind( const std::string &name ) {
    {
        std::lock_guard<std::mutex> lock( m_bindingsMutex );
        m_bindings.erase( name );
    }

    if ( !m_webview )
        return;

    std::string js =
        "if (window.native && window.native['" + name + "']) {"
                                                        "  delete window.native['" +
        name + "'];"
               "}";

    executeScript( js );
}

std::vector<std::string> WebView::getBoundNames() const {
    std::vector<std::string> names;
    for ( auto it = m_bindings.begin(); it != m_bindings.end(); ++it ) {
        names.push_back( it->first );
    }
    return names;
}

void WebView::addApiRoute( const std::string &method, const std::string &routePath, ApiCallback callback ) {
    m_apiRoutes[method][routePath] = std::move( callback );
    ensureWebResourceRequestedHandler();
}

void WebView::removeApiRoute( const std::string &method, const std::string &routePath ) {
    auto it = m_apiRoutes.find( method );
    if ( it != m_apiRoutes.end() ) {
        it->second.erase( routePath );
        if ( it->second.empty() )
            m_apiRoutes.erase( it );
    }
}

bool WebView::addWebResourceRequestedFilter( const std::string &filterUri,
                                             COREWEBVIEW2_WEB_RESOURCE_CONTEXT resourceContext ) {
    if ( !m_webview ) {
        m_pendingFilters.emplace_back( filterUri, resourceContext );
        return true;
    }

    std::wstring wUri = utils::strToWiden( filterUri );
    HRESULT hr = m_webview->AddWebResourceRequestedFilter( wUri.c_str(), resourceContext );
    return SUCCEEDED( hr );
}

bool WebView::removeWebResourceRequestedFilter( const std::string &filterUri,
                                                COREWEBVIEW2_WEB_RESOURCE_CONTEXT resourceContext ) {
    if ( !m_webview ) {
        for ( auto it = m_pendingFilters.begin(); it != m_pendingFilters.end(); ++it ) {
            if ( it->first == filterUri && it->second == resourceContext ) {
                m_pendingFilters.erase( it );
                break;
            }
        }
        return true;
    }

    std::wstring wUri = utils::strToWiden( filterUri );
    HRESULT hr = m_webview->RemoveWebResourceRequestedFilter( wUri.c_str(), resourceContext );
    return SUCCEEDED( hr );
}

void WebView::setWebResourceRequestedCallback( WebResourceRequestedCallback callback ) {
    m_onWebResourceRequested = std::move( callback );
    ensureWebResourceRequestedHandler();
}

void WebView::setWebResourceResponseReceivedCallback( WebResourceResponseReceivedCallback callback ) {
    m_onWebResourceResponseReceived = std::move( callback );

    if ( !m_webview )
        return;

    ICoreWebView2_2 *webview2 = nullptr;
    if ( FAILED( m_webview->QueryInterface( IID_ICoreWebView2_2, (void **)&webview2 ) ) )
        return;

    if ( m_webResourceResponseReceivedToken.value != 0 ) {
        webview2->remove_WebResourceResponseReceived( m_webResourceResponseReceivedToken );
        m_webResourceResponseReceivedToken = {};
    }

    if ( !m_onWebResourceResponseReceived ) {
        webview2->Release();
        return;
    }

    auto handler = new WebResourceResponseReceivedHandler(
        [this]( ICoreWebView2 *, ICoreWebView2WebResourceResponseReceivedEventArgs *args ) {
            if ( !args )
                return;

            ICoreWebView2WebResourceRequest *request = nullptr;
            args->get_Request( &request );

            std::string url;
            std::string method;
            if ( request ) {
                LPWSTR rawUrl = nullptr;
                request->get_Uri( &rawUrl );
                url = coMemToStr( rawUrl );

                LPWSTR rawMethod = nullptr;
                request->get_Method( &rawMethod );
                method = coMemToStr( rawMethod );
                request->Release();
            }

            ICoreWebView2WebResourceResponseView *responseView = nullptr;
            args->get_Response( &responseView );
            if ( !responseView )
                return;

            int statusCode = 0;
            responseView->get_StatusCode( &statusCode );

            LPWSTR rawReason = nullptr;
            responseView->get_ReasonPhrase( &rawReason );
            std::string reasonPhrase = coMemToStr( rawReason );

            std::map<std::string, std::string> respHeadersMap;
            ICoreWebView2HttpResponseHeaders *respHeaders = nullptr;
            responseView->get_Headers( &respHeaders );
            if ( respHeaders ) {
                ICoreWebView2HttpHeadersCollectionIterator *iter = nullptr;
                respHeaders->GetIterator( &iter );
                if ( iter ) {
                    BOOL hasCurrent = FALSE;
                    iter->get_HasCurrentHeader( &hasCurrent );
                    while ( hasCurrent ) {
                        LPWSTR name = nullptr;
                        LPWSTR value = nullptr;
                        iter->GetCurrentHeader( &name, &value );
                        if ( name && value ) {
                            respHeadersMap[utils::strToNarrow( name )] = utils::strToNarrow( value );
                        }
                        if ( name )
                            CoTaskMemFree( name );
                        if ( value )
                            CoTaskMemFree( value );
                        BOOL hasNext = FALSE;
                        iter->MoveNext( &hasNext );
                        hasCurrent = hasNext;
                    }
                    iter->Release();
                }
                respHeaders->Release();
            }

            auto contentHandler = new ResponseViewGetContentHandler(
                [this, url, method, statusCode, reasonPhrase, respHeadersMap]( HRESULT errorCode, IStream *stream ) {
                    std::string responseBody;
                    if ( SUCCEEDED( errorCode ) && stream ) {
                        STATSTG stat = {};
                        if ( SUCCEEDED( stream->Stat( &stat, STATFLAG_NONAME ) ) && stat.cbSize.QuadPart > 0 ) {
                            responseBody.resize( static_cast<size_t>( stat.cbSize.QuadPart ) );
                            ULONG read = 0;
                            stream->Read( responseBody.data(), static_cast<ULONG>( responseBody.size() ), &read );
                            responseBody.resize( read );
                        }
                    }

                    if ( m_onWebResourceResponseReceived ) {
                        m_onWebResourceResponseReceived( url, method, statusCode, reasonPhrase, respHeadersMap, responseBody );
                    }
                } );
            responseView->GetContent( contentHandler );
            contentHandler->Release();

            responseView->Release();
        } );
    webview2->add_WebResourceResponseReceived( handler, &m_webResourceResponseReceivedToken );
    handler->Release();
    webview2->Release();
}

void WebView::setNewWindowRequestedCallback( NewWindowRequestedCallback callback ) {
    m_onNewWindowRequested = std::move( callback );

    if ( !m_webview )
        return;

    if ( m_newWindowRequestedToken.value != 0 ) {
        m_webview->remove_NewWindowRequested( m_newWindowRequestedToken );
        m_newWindowRequestedToken = {};
    }

    if ( !m_onNewWindowRequested )
        return;

    auto handler = new NewWindowRequestedHandler(
        [this]( ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *args ) {
            if ( !args )
                return;

            LPWSTR rawUri = nullptr;
            args->get_Uri( &rawUri );
            std::string url = coMemToStr( rawUri );

            BOOL isUserInitiated = FALSE;
            args->get_IsUserInitiated( &isUserInitiated );

            bool handled = false;
            if ( m_onNewWindowRequested ) {
                handled = m_onNewWindowRequested( url, isUserInitiated != FALSE, this );
            }

            if ( handled ) {
                args->put_Handled( TRUE );
            }
        } );
    m_webview->add_NewWindowRequested( handler, &m_newWindowRequestedToken );
    handler->Release();
}

void WebView::setConsoleMessageCallback( ConsoleMessageCallback callback ) {
    m_onConsoleMessage = std::move( callback );
}

bool WebView::setVirtualHostNameForResources( const std::string &hostName, const std::string &resourcePrefix ) {
    if ( hostName.empty() )
        return false;

    m_virtualHostName = hostName;
    m_resourcePrefix = resourcePrefix;

    std::string filterUri = kVirtualHostProtocol + hostName + "/*";
    if ( !addWebResourceRequestedFilter( filterUri, COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL ) )
        return false;

    ensureWebResourceRequestedHandler();
    return true;
}

void WebView::getVirtualHostName( std::string &hostName, std::string &resourcePrefix ) {
    hostName = m_virtualHostName;
    resourcePrefix = m_resourcePrefix;
}

std::string WebView::getResourceUrl( const std::string &path ) {
    if ( m_virtualHostName.empty() )
        return {};
    return kVirtualHostProtocol + m_virtualHostName + path;
}

void WebView::ensureWebResourceRequestedHandler() {
    if ( !m_webview )
        return;

    bool needHandler = m_onWebResourceRequested || !m_virtualHostName.empty();

    if ( !needHandler ) {
        if ( m_webResourceRequestedToken.value != 0 ) {
            m_webview->remove_WebResourceRequested( m_webResourceRequestedToken );
            m_webResourceRequestedToken = {};
        }
        return;
    }

    if ( m_webResourceRequestedToken.value != 0 )
        return;

    auto handler = new WebResourceRequestedHandler( [this]( ICoreWebView2 *obj, ICoreWebView2WebResourceRequestedEventArgs *args ) {
        onWebResourceRequested( obj, args );
    } );
    m_webview->add_WebResourceRequested( handler, &m_webResourceRequestedToken );
    handler->Release();
}

void WebView::onWebResourceRequested( ICoreWebView2 *, ICoreWebView2WebResourceRequestedEventArgs *args ) {
    if ( !args )
        return;

    ICoreWebView2WebResourceRequest *request = nullptr;
    args->get_Request( &request );
    if ( !request )
        return;

    LPWSTR rawUrl = nullptr;
    request->get_Uri( &rawUrl );
    std::string url = coMemToStr( rawUrl );

    LPWSTR rawMethod = nullptr;
    request->get_Method( &rawMethod );
    std::string method = coMemToStr( rawMethod );

    LOG_INFO << "web:" << url << method << m_name;

    ICoreWebView2HttpRequestHeaders *reqHeaders = nullptr;
    request->get_Headers( &reqHeaders );
    std::map<std::string, std::string> reqHeadersMap;
    if ( reqHeaders ) {
        ICoreWebView2HttpHeadersCollectionIterator *iter = nullptr;
        reqHeaders->GetHeaders( L"", &iter );
        if ( iter ) {
            BOOL hasCurrent = FALSE;
            iter->get_HasCurrentHeader( &hasCurrent );
            while ( hasCurrent ) {
                LPWSTR name = nullptr;
                LPWSTR value = nullptr;
                iter->GetCurrentHeader( &name, &value );
                if ( name && value ) {
                    reqHeadersMap[utils::strToNarrow( name )] = utils::strToNarrow( value );
                }
                if ( name )
                    CoTaskMemFree( name );
                if ( value )
                    CoTaskMemFree( value );
                BOOL hasNext = FALSE;
                iter->MoveNext( &hasNext );
                hasCurrent = hasNext;
            }
            iter->Release();
        }
        reqHeaders->Release();
    }

    IStream *reqContent = nullptr;
    request->get_Content( &reqContent );
    std::string requestBody;
    if ( reqContent ) {
        STATSTG stat = {};
        if ( SUCCEEDED( reqContent->Stat( &stat, STATFLAG_NONAME ) ) && stat.cbSize.QuadPart > 0 ) {
            requestBody.resize( static_cast<size_t>( stat.cbSize.QuadPart ) );
            ULONG read = 0;
            reqContent->Read( requestBody.data(), static_cast<ULONG>( requestBody.size() ), &read );
            requestBody.resize( read );
        }
        reqContent->Release();
    }

    request->Release();

    std::map<std::string, std::string> respHeadersMap;
    std::string respBody;
    int statusCode = 200;
    bool intercept = false;

    if ( !intercept && !m_virtualHostName.empty() ) {
        const std::string prefix = kVirtualHostProtocol + m_virtualHostName;
        if ( url.size() >= prefix.size() && url.compare( 0, prefix.size(), prefix ) == 0 ) {
            std::string path = url.substr( prefix.size() );
            const std::string apiStr = "/api";
            if ( path.compare( 0, apiStr.size(), apiStr ) == 0 ) {
                intercept = apiServer( method, url, path.substr( apiStr.size() ), reqHeadersMap, requestBody, respBody, respHeadersMap, statusCode );
            } else {
                intercept = staticResourceServe( path, respBody, respHeadersMap, statusCode );
            }
        }
    }

    if ( !intercept && m_onWebResourceRequested ) {
        intercept = m_onWebResourceRequested( url, method, reqHeadersMap,
                                              respHeadersMap, respBody, statusCode );
    }

    if ( intercept ) {
        ICoreWebView2WebResourceResponse *response = nullptr;

        IStream *bodyStream = nullptr;
        if ( !respBody.empty() ) {
            HGLOBAL hMem = GlobalAlloc( GMEM_FIXED, respBody.size() );
            if ( hMem ) {
                memcpy( hMem, respBody.data(), respBody.size() );
                CreateStreamOnHGlobal( hMem, TRUE, &bodyStream );
            }
        }

        std::wstring wRespHeaders;
        if ( respHeadersMap.empty() ) {
            wRespHeaders = L"Content-Type: text/plain; charset=utf-8\r\n";
        } else {
            std::string headersStr;
            for ( const auto &[k, v] : respHeadersMap ) {
                headersStr += k + ": " + v + "\r\n";
            }
            wRespHeaders = utils::strToWiden( headersStr );
        }

        m_environment->CreateWebResourceResponse(
            bodyStream, statusCode, L"OK", wRespHeaders.c_str(), &response );

        if ( bodyStream )
            bodyStream->Release();

        if ( response ) {
            args->put_Response( response );
            response->Release();
        }
    }
}

void WebView::onEnvCompleted( HRESULT hr, ICoreWebView2Environment *env, CreatedCallback onCreated ) {
    if ( FAILED( hr ) || !env ) {
        LOG_WARN << "webview environment create failed, hr: " << (int)hr;
        if ( onCreated )
            onCreated( false, this );
        return;
    }

    m_environment = env;
    m_environment->AddRef();

    auto ctrlHandler = new CtrlCompletedHandler(
        [this, onCreated]( HRESULT hr2, ICoreWebView2Controller *ctrl ) {
            if ( FAILED( hr2 ) || !ctrl ) {
                if ( onCreated )
                    onCreated( false, this );
                return;
            }

            m_controller = ctrl;
            m_controller->AddRef();

            ICoreWebView2 *wv = nullptr;
            ctrl->get_CoreWebView2( &wv );
            if ( !wv ) {
                if ( onCreated )
                    onCreated( false, this );
                return;
            }
            m_webview = wv;

            ICoreWebView2Settings *settings = nullptr;
            wv->get_Settings( &settings );
            m_settings = settings;

            const auto &cfg = m_config;
            if ( settings ) {
                settings->put_IsScriptEnabled( cfg.scriptEnabled ? TRUE : FALSE );
                settings->put_IsWebMessageEnabled( cfg.webMessageEnabled ? TRUE : FALSE );
                settings->put_AreDevToolsEnabled( cfg.devToolsEnabled ? TRUE : FALSE );
                settings->put_IsStatusBarEnabled( cfg.statusBarEnabled ? TRUE : FALSE );
                settings->put_AreDefaultContextMenusEnabled( cfg.defaultContextMenusEnabled ? TRUE : FALSE );
            }

            if ( cfg.transparentBackground ) {
                ICoreWebView2Controller2 *ctrl2 = nullptr;
                if ( SUCCEEDED( ctrl->QueryInterface( IID_ICoreWebView2Controller2, (void **)&ctrl2 ) ) ) {
                    COREWEBVIEW2_COLOR bgColor = { 0, 0, 0, 0 };
                    ctrl2->put_DefaultBackgroundColor( bgColor );
                    ctrl2->Release();
                }
            }

            m_initialized = true;
            registerEventHandlers();
            runJsInject();

            for ( const auto &[filterUri, ctx] : m_pendingFilters ) {
                std::wstring wUri = utils::strToWiden( filterUri );
                m_webview->AddWebResourceRequestedFilter( wUri.c_str(), ctx );
            }
            m_pendingFilters.clear();

            if ( !m_virtualHostName.empty() ) {
                std::string filterUri = kVirtualHostProtocol + m_virtualHostName + "/*";
                addWebResourceRequestedFilter( filterUri, COREWEBVIEW2_WEB_RESOURCE_CONTEXT_DOCUMENT );
                ensureWebResourceRequestedHandler();
            }

            if ( !cfg.initialUrl.empty() ) {
                navigate( cfg.initialUrl );
            }

            if ( onCreated )
                onCreated( true, this );
        } );

    env->CreateCoreWebView2Controller( m_parentHwnd, ctrlHandler );
}

bool WebView::handleBindCall( const json::value &msg ) {
    if ( !msg.is_object() )
        return false;

    auto &obj = msg.as_object();
    auto *typeVal = obj.if_contains( "type" );

    if ( !typeVal || !typeVal->is_string() || typeVal->as_string() != "__native-rpc" )
        return false;

    auto *messageVal = obj.if_contains( "message" );
    if ( !messageVal || !messageVal->is_object() )
        return false;

    auto &rpcObj = messageVal->as_object();
    auto *idVal = rpcObj.if_contains( "id" );
    auto *nameVal = rpcObj.if_contains( "name" );
    auto *argsVal = rpcObj.if_contains( "args" );

    std::string id = idVal && idVal->is_string() ? std::string( idVal->as_string() ) : "";
    std::string name = nameVal && nameVal->is_string() ? std::string( nameVal->as_string() ) : "";
    json::array argsArr;
    if ( argsVal && argsVal->is_array() )
        argsArr = argsVal->as_array();

    json::value result;
    std::string error;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock( m_bindingsMutex );
        auto it = m_bindings.find( name );
        if ( it != m_bindings.end() ) {
            found = true;
            try {
                result = it->second( argsArr, this );
            } catch ( const std::exception &e ) {
                error = e.what();
            } catch ( ... ) {
                error = "Unknown error";
            }
        }
    }

    if ( !found )
        error = "Function not found: " + name;

    json::object rpcResp;
    rpcResp["id"] = id;
    if ( error.empty() ) {
        rpcResp["result"] = result;
    } else {
        rpcResp["error"] = error;
    }

    json::object respObj;
    respObj["type"] = "__native-rpc-response";
    respObj["message"] = rpcResp;
    postWebMessage( respObj );
    return true;
}

bool WebView::sendIpcMessage( const std::vector<std::string> &to, const json::value &data ) {
    if ( to.empty() )
        return false;
    json::object fwdMsg;
    fwdMsg["from"] = m_name;
    fwdMsg["data"] = data;

    json::object fwd;
    fwd["type"] = "__ipc";
    fwd["message"] = fwdMsg;
    std::lock_guard<std::mutex> lock( s_instancesMutex );
    for ( auto *inst : s_instances ) {
        if ( inst == this )
            continue;
        for ( auto &target : to ) {
            if ( inst->m_name == target ) {
                inst->postWebMessage( fwd );
                return true;
            }
        }
    }
    return false;
}

bool WebView::handleIpcMessage( const json::value &msg ) {
    try {
        if ( !msg.is_object() )
            return false;
        auto &obj = msg.as_object();
        auto *typeVal = obj.if_contains( "type" );
        if ( !typeVal || !typeVal->is_string() || typeVal->as_string() != "__ipc" )
            return false;

        auto *messageVal = obj.if_contains( "message" );
        if ( !messageVal || !messageVal->is_object() )
            return false;

        auto &ipcObj = messageVal->as_object();
        std::vector<std::string> targets;
        if ( ipcObj.contains( "to" ) ) {
            auto &toVal = ipcObj.at( "to" );
            if ( toVal.is_string() ) {
                targets.push_back( std::string( toVal.as_string() ) );
            } else if ( toVal.is_array() ) {
                for ( auto &item : toVal.as_array() ) {
                    if ( item.is_string() )
                        targets.push_back( std::string( item.as_string() ) );
                }
            }
        }
        return sendIpcMessage( targets, ipcObj.contains( "data" ) ? ipcObj.at( "data" ) : json::value{} );
    } catch ( ... ) {
        return false;
    }
}

bool WebView::handleConsoleMessage( const json::value &msg ) {
    try {
        if ( !msg.is_object() )
            return false;
        auto &obj = msg.as_object();
        if ( obj.contains( "type" ) && obj.at( "type" ).is_string() &&
             obj.at( "type" ).as_string() == "__console" ) {
            auto *messageVal = obj.if_contains( "message" );
            if ( !messageVal || !messageVal->is_object() )
                return false;
            auto &consoleObj = messageVal->as_object();
            int level = 0;
            std::string message;
            if ( consoleObj.contains( "level" ) && consoleObj.at( "level" ).is_int64() )
                level = static_cast<int>( consoleObj.at( "level" ).as_int64() );
            if ( consoleObj.contains( "message" ) && consoleObj.at( "message" ).is_string() )
                message = std::string( consoleObj.at( "message" ).as_string() );

            if ( m_onConsoleMessage ) {
                m_onConsoleMessage( level, message, this );
            } else {
                switch ( level ) {
                case 0:
                    LOG_DEBUG << "JS:" << message;
                    break;
                case 1:
                    LOG_LOG << "JS:" << message;
                    break;
                case 2:
                    LOG_INFO << "JS:" << message;
                    break;
                case 3:
                    LOG_WARN << "JS:" << message;
                    break;
                case 4:
                    LOG_ERROR << "JS:" << message;
                    break;
                default:
                    LOG_DEBUG << "JS:" << level << message;
                    break;
                }
            }
        }
    } catch ( ... ) {
        return false;
    }
    return true;
}

void WebView::registerEventHandlers() {
    if ( !m_webview )
        return;

    auto navHandler = new NavCompletedHandler(
        [this]( ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args ) {
            BOOL isSuccess = FALSE;
            if ( args )
                args->get_IsSuccess( &isSuccess );

            if ( m_onNavigationCompleted && args ) {
                m_onNavigationCompleted( isSuccess != FALSE, this );
            }
        } );
    m_webview->add_NavigationCompleted( navHandler, &m_navigationCompletedToken );
    navHandler->Release();

    auto navStartingHandler = new NavStartingHandler(
        [this]( ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args ) {
            if ( !args )
                return;
            LPWSTR uri = nullptr;
            args->get_Uri( &uri );
            std::string uriStr = coMemToStr( uri );
            if ( m_onNavigationStarting && m_onNavigationStarting( uriStr, this ) )
                args->put_Cancel( TRUE );
        } );
    m_webview->add_NavigationStarting( navStartingHandler, &m_navigationStartingToken );
    navStartingHandler->Release();

    auto msgHandler = new MsgReceivedHandler(
        [this]( ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args ) {
            if ( !args )
                return;
            LPWSTR rawMsg = nullptr;
            args->get_WebMessageAsJson( &rawMsg );
            std::string msg = coMemToStr( rawMsg );
            try {
                auto jv = json::parse( msg );
                LOG_DEBUG << m_name << "消息:" << msg;
                // 处理 native-rpc 消息
                if ( handleBindCall( jv ) )
                    return;
                // 处理跨 WebView IPC 消息
                if ( handleIpcMessage( jv ) )
                    return;
                // 处理 console 拦截消息
                if ( handleConsoleMessage( jv ) )
                    return;
                // 调用回调
                if ( m_onWebMessageReceived )
                    m_onWebMessageReceived( jv, this );
            } catch ( ... ) {
            }
        } );
    m_webview->add_WebMessageReceived( msgHandler, &m_webMessageReceivedToken );
    msgHandler->Release();

    auto titleHandler = new DocTitleChangedHandler(
        [this]( ICoreWebView2 *, IUnknown * ) {
            if ( m_onDocumentTitleChanged && m_webview ) {
                LPWSTR rawTitle = nullptr;
                m_webview->get_DocumentTitle( &rawTitle );
                std::string title = coMemToStr( rawTitle );
                m_onDocumentTitleChanged( title, this );
            }
        } );
    m_webview->add_DocumentTitleChanged( titleHandler, &m_documentTitleChangedToken );
    titleHandler->Release();

    auto closeHandler = new WinCloseRequestedHandler(
        [this]( ICoreWebView2 *, IUnknown * ) {
            if ( m_controller ) {
                m_controller->Close();
            }
            PostMessage( m_parentHwnd, WM_CLOSE, 0, 0 );
        } );
    m_webview->add_WindowCloseRequested( closeHandler, &m_windowCloseRequestedToken );
    closeHandler->Release();

    if ( m_onNewWindowRequested ) {
        auto newWindowHandler = new NewWindowRequestedHandler(
            [this]( ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *args ) {
                if ( !args )
                    return;

                LPWSTR rawUri = nullptr;
                args->get_Uri( &rawUri );
                std::string url = coMemToStr( rawUri );

                BOOL isUserInitiated = FALSE;
                args->get_IsUserInitiated( &isUserInitiated );

                bool handled = false;
                if ( m_onNewWindowRequested ) {
                    handled = m_onNewWindowRequested( url, isUserInitiated != FALSE, this );
                }

                if ( handled ) {
                    args->put_Handled( TRUE );
                }
            } );
        m_webview->add_NewWindowRequested( newWindowHandler, &m_newWindowRequestedToken );
        newWindowHandler->Release();
    }
}

void WebView::runJsInject() {
    if ( !m_webview )
        return;

    std::string js;
    if ( m_config.consoleEnabled ) {
        // 注入 console 拦截脚本
        js =
            "(function() {"
            "  if (window.__consoleIntercepted) return;"
            "  window.__consoleIntercepted = true;"
            "  var origDebug = console.debug;"
            "  var origLog = console.log;"
            "  var origInfo = console.info;"
            "  var origWarn = console.warn;"
            "  var origError = console.error;"
            "  function send(level, args) {"
            "    try {"
            "      var msg = Array.prototype.slice.call(args).map(function(a) {"
            "        if (typeof a === 'object') try { return JSON.stringify(a); } catch(e) { return String(a); }"
            "        return String(a);"
            "      }).join(' ');"
            "      window.chrome.webview.postMessage({type:'__console',message:{level:level,message:msg}});"
            "    } catch(e) {}"
            "  }"
            "  console.debug = function() { send(0, arguments); origDebug.apply(console, arguments); };"
            "  console.log = function() { send(1, arguments); origLog.apply(console, arguments); };"
            "  console.info = function() { send(2, arguments); origInfo.apply(console, arguments); };"
            "  console.warn = function() { send(3, arguments); origWarn.apply(console, arguments); };"
            "  console.error = function() { send(4, arguments); origError.apply(console, arguments); };"
            "})();";
    }

    // 注入 native-rpc 桥架脚本
    {
        std::lock_guard<std::mutex> lock( m_bindingsMutex );
        js += generateInjectBindings( m_bindings );
    }
    std::wstring wJs = utils::strToWiden( js );
    m_webview->AddScriptToExecuteOnDocumentCreated( wJs.c_str(), nullptr );
}

std::string WebView::generateInjectBindings( const std::map<std::string, NativeCallback> &bindings ) {
    std::string js = R"JavaScript(
(function () {
    if (window.native && window.native._defined)
        return;
    window.native = { _defined: true, _pending: {}, _seq: 0 };
    window.native.sendIpcMessage = function (to, data) {
        window.chrome.webview.postMessage({ type: '__ipc', message: { to: to, data: data } });
    };
    window.chrome.webview.addEventListener('message', function (e) {
        var msg = (typeof e.data === 'string') ? JSON.parse(e.data) : e.data;
        if (!msg || !msg.type || !msg.message)
            return;
        if (msg.type === '__native-rpc-response') {
            if (window.native._pending[msg.message.id]) {
                var p = window.native._pending[msg.message.id];
                delete window.native._pending[msg.message.id];
                if (msg.message.error)
                    p.reject(new Error(msg.message.error));
                else
                    p.resolve(msg.message.result);
            }
        } else if (msg.type === '__ipc') {
            window.dispatchEvent(new CustomEvent('ipc-message', {
                detail: {
                    from: msg.message.from,
                    data: msg.message.data
                }
            }));
        }
    });
})();
)JavaScript";

    if ( bindings.empty() )
        return js;

    js += R"JavaScript(
(function (methods) {
    methods = methods || [];
    window.native._pending = window.native._pending || {};

    function imp(method, data) {
        var args = Array.prototype.slice.call(data);
        var id = 'rpc_' + (window.native._seq = (window.native._seq || 0) + 1);
        return new Promise(function (resolve, reject) {
            window.native._pending[id] = {
                resolve: resolve,
                reject: reject
            };
            window.chrome.webview.postMessage({
                type: '__native-rpc',
                message: {
                    id: id,
                    name: method,
                    args: args
                }
            });
        });
    }
    methods.forEach(function (method) {
        window.native[method] = function () {
            return imp(method, arguments);
        };
    });
}
)([ )JavaScript";

    bool first = true;
    for ( auto &[name, cb] : bindings ) {
        if ( first )
            first = false;
        else
            js += ", ";
        js += "'" + name + "'";
    }
    js += " ]);";
    return js;
}

bool WebView::staticResourceExists( const std::string &resName ) {
#ifdef RESOURCE_PATH
    return std::filesystem::exists( RESOURCE_PATH + resName );
#else
    return resource_exists( resName.c_str() );
#endif
}

bool WebView::getStaticResource( const std::string &resName, std::string &content, std::string &outContentType ) {
#ifdef RESOURCE_PATH
    std::string fullPath = RESOURCE_PATH + resName;
    std::ifstream file( fullPath, std::ios::binary );
    if ( !file )
        return false;
    content = std::string( std::istreambuf_iterator<char>( file ), {} );
    outContentType = utils::guessContentType( std::filesystem::path( resName ) );
    return true;
#else
    const unsigned char *data = nullptr;
    int size = resource_get( resName.c_str(), &data );
    if ( size < 0 || !data )
        return false;
    content.assign( reinterpret_cast<const char *>( data ), size );
    outContentType = utils::guessContentType( std::filesystem::path( resName ) );
    return true;
#endif
}

std::string WebView::staticResourceRedirectPage( const std::string &path ) {
    return "<html>"
           "<head><title>Redirect</title></head>"
           "<body><script>"
           "var params = window.location.search; window.location.href = '" +
           path + "' + params;" +
           "</script></body>"
           "</html>";
}

bool WebView::staticResourceServe( const std::string &path,
                                   std::string &respBody,
                                   std::map<std::string, std::string> &respHeadersMap,
                                   int &statusCode ) {

    std::string fullPath = m_resourcePrefix;
    size_t queryPos = path.find( '?' );
    if ( queryPos != std::string::npos )
        fullPath += path.substr( 0, queryPos );
    else
        fullPath += path;
    std::string contentType;
    if ( getStaticResource( fullPath, respBody, contentType ) ) {
        respHeadersMap["Content-Type"] = contentType;
        return true;
    }

    if ( path.find( '.' ) == std::string::npos ) {
        std::string base = path;
        while ( !base.empty() && base.back() == '/' )
            base.pop_back();

        if ( staticResourceExists( m_resourcePrefix + "/" + base + "/index.html" ) ) {
            respBody = staticResourceRedirectPage( base + "/index.html" );
            respHeadersMap["Content-Type"] = "text/html; charset=utf-8";
            return true;
        } else if ( staticResourceExists( m_resourcePrefix + "/" + base + ".html" ) ) {
            respBody = staticResourceRedirectPage( base + ".html" );
            respHeadersMap["Content-Type"] = "text/html; charset=utf-8";
            return true;
        }
    }

    statusCode = 404;
    if ( getStaticResource( m_resourcePrefix + "/404.html", respBody, contentType ) ) {
        respHeadersMap["Content-Type"] = contentType;
    } else {
        respHeadersMap["Content-Type"] = "text/plain; charset=utf-8";
        respBody = "404 Not Found: " + path;
    }
    return true;
}

bool WebView::apiServer( const std::string &method,
                         const std::string &url,
                         std::string apiPath,
                         const std::map<std::string, std::string> &reqHeadersMap,
                         const std::string &requestBody,
                         std::string &respBody,
                         std::map<std::string, std::string> &respHeadersMap,
                         int &statusCode ) {
    std::map<std::string, std::string> queryParams;
    size_t queryPos = apiPath.find( '?' );
    if ( queryPos != std::string::npos ) {
        std::string queryStr = apiPath.substr( queryPos + 1 );
        apiPath = apiPath.substr( 0, queryPos );
        size_t start = 0;
        while ( start < queryStr.size() ) {
            size_t ampPos = queryStr.find( '&', start );
            std::string pair = ( ampPos == std::string::npos ) ? queryStr.substr( start ) : queryStr.substr( start, ampPos - start );
            size_t eqPos = pair.find( '=' );
            if ( eqPos != std::string::npos )
                queryParams[utils::urlDecode( pair.substr( 0, eqPos ) )] = utils::urlDecode( pair.substr( eqPos + 1 ) );
            else if ( !pair.empty() )
                queryParams[utils::urlDecode( pair )] = "";
            if ( ampPos == std::string::npos )
                break;
            start = ampPos + 1;
        }
    }
    auto methodIt = m_apiRoutes.find( method );
    if ( methodIt != m_apiRoutes.end() ) {
        const auto &routes = methodIt->second;
        for ( auto it = routes.rbegin(); it != routes.rend(); ++it ) {
            if ( apiPath.compare( 0, it->first.size(), it->first ) == 0 ) {
                it->second( queryParams, reqHeadersMap, requestBody,
                            respHeadersMap, respBody, statusCode );
                if ( respHeadersMap.find( "Content-Type" ) == respHeadersMap.end() )
                    respHeadersMap["Content-Type"] = "application/json; charset=utf-8";
                return true;
            }
        }
    }
    statusCode = 404;
    return true;
}