#include "Utils.h"
#include "Logger.hpp"
#include "resource.h"
#include <map>
#include <mutex>
#include <objidl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <vector>

namespace utils {
std::wstring strToWiden( const std::string &str ) {
    if ( str.empty() )
        return {};
    int len = MultiByteToWideChar( CP_UTF8, 0, str.c_str(), -1, nullptr, 0 );
    if ( len <= 0 )
        return {};
    std::wstring wstr( static_cast<size_t>( len ) - 1, L'\0' );
    MultiByteToWideChar( CP_UTF8, 0, str.c_str(), -1, &wstr[0], len );
    return wstr;
}

std::string strToNarrow( const std::wstring &wstr ) {
    if ( wstr.empty() )
        return {};
    int len = WideCharToMultiByte( CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr );
    if ( len <= 0 )
        return {};
    std::string str( static_cast<size_t>( len ) - 1, '\0' );
    WideCharToMultiByte( CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, nullptr, nullptr );
    return str;
}

std::string urlDecode( const std::string &str ) {
    std::string result;
    result.reserve( str.size() );
    for ( size_t i = 0; i < str.size(); ++i ) {
        if ( str[i] == '%' && i + 2 < str.size() ) {
            auto hex = []( char c ) -> int {
                if ( c >= '0' && c <= '9' )
                    return c - '0';
                if ( c >= 'A' && c <= 'F' )
                    return c - 'A' + 10;
                if ( c >= 'a' && c <= 'f' )
                    return c - 'a' + 10;
                return -1;
            };
            int hi = hex( str[i + 1] );
            int lo = hex( str[i + 2] );
            if ( hi >= 0 && lo >= 0 ) {
                result += static_cast<char>( hi * 16 + lo );
                i += 2;
                continue;
            }
        }
        if ( str[i] == '+' ) {
            result += ' ';
            continue;
        }
        result += str[i];
    }
    return result;
}

std::string guessContentType( const std::filesystem::path &path ) {
    std::string ext = path.extension().string();
    std::transform( ext.begin(), ext.end(), ext.begin(), ::tolower );

    if ( ext == ".html" || ext == ".htm" )
        return "text/html; charset=utf-8";
    if ( ext == ".css" )
        return "text/css; charset=utf-8";
    if ( ext == ".js" || ext == ".mjs" )
        return "application/javascript; charset=utf-8";
    if ( ext == ".json" )
        return "application/json; charset=utf-8";
    if ( ext == ".txt" || ext == ".log" )
        return "text/plain; charset=utf-8";
    if ( ext == ".csv" )
        return "text/csv; charset=utf-8";
    if ( ext == ".xml" )
        return "application/xml; charset=utf-8";
    if ( ext == ".pdf" )
        return "application/pdf";
    if ( ext == ".zip" )
        return "application/zip";
    if ( ext == ".wasm" )
        return "application/wasm";

    if ( ext == ".svg" )
        return "image/svg+xml";
    if ( ext == ".png" )
        return "image/png";
    if ( ext == ".jpg" || ext == ".jpeg" )
        return "image/jpeg";
    if ( ext == ".gif" )
        return "image/gif";
    if ( ext == ".webp" )
        return "image/webp";
    if ( ext == ".ico" )
        return "image/x-icon";
    if ( ext == ".bmp" )
        return "image/bmp";

    if ( ext == ".woff" )
        return "font/woff";
    if ( ext == ".woff2" )
        return "font/woff2";
    if ( ext == ".ttf" )
        return "font/ttf";
    if ( ext == ".otf" )
        return "font/otf";

    if ( ext == ".mp4" )
        return "video/mp4";
    if ( ext == ".webm" )
        return "video/webm";

    if ( ext == ".mp3" )
        return "audio/mpeg";
    if ( ext == ".wav" )
        return "audio/wav";
    if ( ext == ".ogg" )
        return "audio/ogg";
    if ( ext == ".flac" )
        return "audio/flac";
    if ( ext == ".m4a" || ext == ".aac" )
        return "audio/mp4";
    if ( ext == ".opus" )
        return "audio/ogg";
    if ( ext == ".wma" )
        return "audio/x-ms-wma";
    return "application/octet-stream";
}

std::string readResource( const std::string &path ) {
#ifdef RESOURCE_PATH
    std::string fullPath = RESOURCE_PATH + path;
    std::ifstream file( fullPath, std::ios::binary );
    if ( !file )
        return {};
    return std::string( std::istreambuf_iterator<char>( file ), {} );
#else
    int index = resource_index( path.c_str() );
    if ( index <= 0 )
        return {};
    const unsigned char *data = resource_data( index );
    int size = resource_size( index );
    return std::string( data, data + size );
#endif
}

HICON iconFromFile( const std::string &filePath, int width, int height ) {
    std::wstring wPath = utils::strToWiden( filePath );
    return (HICON)LoadImageW(
        nullptr, wPath.c_str(), IMAGE_ICON,
        width, height,
        LR_LOADFROMFILE | ( ( width == 0 && height == 0 ) ? LR_DEFAULTSIZE : 0 ) );
}

HICON iconFromMemory( const BYTE *data, size_t size ) {
    if ( !data || size < 6 )
        return nullptr;

    WORD reserved = *(WORD *)( data );
    WORD type = *(WORD *)( data + 2 );
    WORD count = *(WORD *)( data + 4 );

    if ( reserved != 0 || type != 1 || count == 0 )
        return nullptr;

    if ( size < 6 + (size_t)count * 16 )
        return nullptr;

    DWORD dataOffset = *(DWORD *)( data + 6 + 12 );
    DWORD dataSize = *(DWORD *)( data + 6 + 8 );

    if ( dataOffset == 0 || dataSize == 0 )
        return nullptr;
    if ( (size_t)dataOffset + dataSize > size )
        return nullptr;

    return CreateIconFromResource( (PBYTE)( data + dataOffset ), dataSize, TRUE, 0x00030000 );
}

HICON iconFromResource( const std::string &path ) {
    int index = resource_index( path.c_str() );
    if ( index < 0 )
        return nullptr;
    const unsigned char *data = resource_data( index );
    int size = resource_size( index );
    return iconFromMemory( data, size );
}

namespace exeIcon {
std::string dibToIcoBytes( const BYTE *dibData, DWORD dibSize, int width, int height, WORD bitCount ) {
    DWORD imageOffset = 6 + 16;
    std::string ico;
    ico.reserve( imageOffset + dibSize );
    ico.push_back( 0 );
    ico.push_back( 0 );
    ico.push_back( 1 );
    ico.push_back( 0 );
    ico.push_back( 1 );
    ico.push_back( 0 );
    ico.push_back( static_cast<char>( width >= 256 ? 0 : width ) );
    ico.push_back( static_cast<char>( height >= 256 ? 0 : height ) );
    ico.push_back( 0 );
    ico.push_back( 0 );
    ico.push_back( 1 );
    ico.push_back( 0 );
    ico.append( reinterpret_cast<char *>( &bitCount ), 2 );
    ico.append( reinterpret_cast<char *>( &dibSize ), 4 );
    ico.append( reinterpret_cast<char *>( &imageOffset ), 4 );
    ico.append( reinterpret_cast<const char *>( dibData ), dibSize );
    return ico;
}

std::pair<std::string, std::string> hiconToIcoData( HICON hIcon ) {
    ICONINFO ii;
    if ( !GetIconInfo( hIcon, &ii ) )
        return {};

    BITMAP bmColor;
    GetObject( ii.hbmColor, sizeof( bmColor ), &bmColor );
    int width = bmColor.bmWidth;
    int height = bmColor.bmHeight;

    HDC hdc = GetDC( nullptr );

    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof( BITMAPINFOHEADER );
    bih.biWidth = width;
    bih.biHeight = height;
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    std::vector<BYTE> colorBits( width * height * 4 );
    GetDIBits( hdc, ii.hbmColor, 0, height, colorBits.data(), reinterpret_cast<BITMAPINFO *>( &bih ), DIB_RGB_COLORS );

    int maskRowBytes = ( ( width + 31 ) / 32 ) * 4;
    std::vector<BYTE> maskBits( maskRowBytes * height, 0 );

    ReleaseDC( nullptr, hdc );
    DeleteObject( ii.hbmColor );
    DeleteObject( ii.hbmMask );

    BITMAPINFOHEADER icoBih = {};
    icoBih.biSize = sizeof( BITMAPINFOHEADER );
    icoBih.biWidth = width;
    icoBih.biHeight = height * 2;
    icoBih.biPlanes = 1;
    icoBih.biBitCount = 32;
    icoBih.biCompression = BI_RGB;
    icoBih.biSizeImage = static_cast<DWORD>( colorBits.size() + maskBits.size() );

    std::string dibData;
    dibData.reserve( sizeof( BITMAPINFOHEADER ) + colorBits.size() + maskBits.size() );
    dibData.append( reinterpret_cast<char *>( &icoBih ), sizeof( BITMAPINFOHEADER ) );
    dibData.append( reinterpret_cast<char *>( colorBits.data() ), colorBits.size() );
    dibData.append( reinterpret_cast<char *>( maskBits.data() ), maskBits.size() );

    std::string ico = dibToIcoBytes( reinterpret_cast<const BYTE *>( dibData.data() ),
                                     static_cast<DWORD>( dibData.size() ), width, height, 32 );
    return { ico, "image/x-icon" };
}

struct GRPICONDIR {
    WORD idReserved;
    WORD idType;
    WORD nCount;
};

struct GRPICONDIRENTRY {
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;
    BYTE bReserved;
    WORD wPlanes;
    WORD wBitCount;
    DWORD dwBytesInRes;
    WORD nID;
};

std::pair<std::string, std::string> extractIconFromResource( const std::string &exePath, int targetSize ) {
    std::wstring wPath = strToWiden( exePath );
    HMODULE hExe = LoadLibraryExW( wPath.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE );
    if ( !hExe )
        hExe = LoadLibraryExW( wPath.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE );
    if ( !hExe )
        return {};

    std::pair<std::string, std::string> result;

    auto findGroupIcon = []( HMODULE hMod ) -> HRSRC {
        HRSRC hRes = nullptr;
        struct Data {
            HMODULE mod;
            HRSRC *out;
        } data = { hMod, &hRes };
        EnumResourceNamesW( hMod, (LPCWSTR)RT_GROUP_ICON, []( HMODULE, LPCWSTR, LPWSTR lpName, LONG_PTR lParam ) -> BOOL {
                auto *d = reinterpret_cast<Data *>( lParam );
                *( d->out ) = FindResourceW( d->mod, lpName, (LPCWSTR)RT_GROUP_ICON );
                return FALSE; }, reinterpret_cast<LONG_PTR>( &data ) );
        return hRes;
    };

    auto processIconData = []( const BYTE *pData, DWORD dataSize, int width, int height, WORD bitCount ) -> std::pair<std::string, std::string> {
        if ( dataSize >= 4 && pData[0] == 0x89 && pData[1] == 0x50 && pData[2] == 0x4E && pData[3] == 0x47 )
            return { std::string( reinterpret_cast<const char *>( pData ), dataSize ), "image/png" };
        return { dibToIcoBytes( pData, dataSize, width, height, bitCount ), "image/x-icon" };
    };

    HRSRC hGrpRes = findGroupIcon( hExe );
    if ( hGrpRes ) {
        HGLOBAL hGrpData = LoadResource( hExe, hGrpRes );
        if ( hGrpData ) {
            auto *pGrpDir = reinterpret_cast<GRPICONDIR *>( LockResource( hGrpData ) );
            if ( pGrpDir && pGrpDir->nCount > 0 ) {
                auto *pEntries = reinterpret_cast<GRPICONDIRENTRY *>(
                    reinterpret_cast<BYTE *>( pGrpDir ) + sizeof( GRPICONDIR ) );

                int bestIdx = 0;
                int bestDiff = 9999;
                int bestBitCount = 0;
                for ( int i = 0; i < pGrpDir->nCount; i++ ) {
                    int w = pEntries[i].bWidth == 0 ? 256 : pEntries[i].bWidth;
                    int diff = abs( w - targetSize );
                    int bc = pEntries[i].wBitCount;
                    if ( diff < bestDiff || ( diff == bestDiff && bc > bestBitCount ) ) {
                        bestDiff = diff;
                        bestBitCount = bc;
                        bestIdx = i;
                    }
                }

                HRSRC hIconRes = FindResourceW( hExe,
                                                MAKEINTRESOURCEW( pEntries[bestIdx].nID ), (LPCWSTR)RT_ICON );
                if ( hIconRes ) {
                    HGLOBAL hIconData = LoadResource( hExe, hIconRes );
                    if ( hIconData ) {
                        DWORD dataSize = SizeofResource( hExe, hIconRes );
                        auto *pData = reinterpret_cast<BYTE *>( LockResource( hIconData ) );
                        if ( pData && dataSize > 0 ) {
                            int w = pEntries[bestIdx].bWidth == 0 ? 256 : pEntries[bestIdx].bWidth;
                            int h = pEntries[bestIdx].bHeight == 0 ? 256 : pEntries[bestIdx].bHeight;
                            result = processIconData( pData, dataSize, w, h, pEntries[bestIdx].wBitCount );
                        }
                        FreeResource( hIconData );
                    }
                }
            }
            FreeResource( hGrpData );
        }
    }

    if ( result.first.empty() ) {
        HRSRC hIconRes = nullptr;
        struct Data2 {
            HMODULE mod;
            HRSRC *out;
        } data2 = { hExe, &hIconRes };
        EnumResourceNamesW( hExe, (LPCWSTR)RT_ICON, []( HMODULE, LPCWSTR, LPWSTR lpName, LONG_PTR lParam ) -> BOOL {
                auto *d = reinterpret_cast<Data2 *>( lParam );
                if ( !*( d->out ) )
                    *( d->out ) = FindResourceW( d->mod, lpName, (LPCWSTR)RT_ICON );
                return TRUE; }, reinterpret_cast<LONG_PTR>( &data2 ) );

        if ( hIconRes ) {
            HGLOBAL hIconData = LoadResource( hExe, hIconRes );
            if ( hIconData ) {
                DWORD dataSize = SizeofResource( hExe, hIconRes );
                auto *pData = reinterpret_cast<BYTE *>( LockResource( hIconData ) );
                if ( pData && dataSize > 0 ) {
                    if ( dataSize >= 4 && pData[0] == 0x89 && pData[1] == 0x50 && pData[2] == 0x4E && pData[3] == 0x47 )
                        result = { std::string( reinterpret_cast<const char *>( pData ), dataSize ), "image/png" };
                    else
                        result = { dibToIcoBytes( pData, dataSize, 0, 0, 0 ), "image/x-icon" };
                }
                FreeResource( hIconData );
            }
        }
    }

    FreeLibrary( hExe );
    return result;
}
}; // namespace exeIcon

std::pair<std::string, std::string> iconExeToIconData( const std::string &exePath, int size ) {
    if ( exePath.empty() )
        return {};

    std::wstring wPath = strToWiden( exePath );
    HICON hIcon = ExtractIconW( nullptr, wPath.c_str(), 0 );
    if ( hIcon && hIcon != (HICON)1 ) {
        auto result = exeIcon::hiconToIcoData( hIcon );
        DestroyIcon( hIcon );
        if ( !result.first.empty() )
            return result;
    }

    return exeIcon::extractIconFromResource( exePath, size );
}

HRGN createRgn( const json::array &rgnList ) {
    auto getInt = []( const json::object &obj, const char *key, int defaultVal = 0 ) -> int {
        if ( obj.contains( key ) && obj.at( key ).is_int64() )
            return static_cast<int>( obj.at( key ).as_int64() );
        return defaultVal;
    };

    std::function<HRGN( const json::object & )> createOne;
    createOne = [&createOne, &getInt]( const json::object &obj ) -> HRGN {
        if ( !obj.contains( "type" ) || !obj.at( "type" ).is_string() )
            return nullptr;

        std::string type = std::string( obj.at( "type" ).as_string() );

        if ( type == "rect" ) {
            int x = getInt( obj, "x" );
            int y = getInt( obj, "y" );
            int w = getInt( obj, "w" );
            int h = getInt( obj, "h" );
            return CreateRectRgn( x, y, x + w, y + h );
        }

        if ( type == "roundRect" ) {
            int x = getInt( obj, "x" );
            int y = getInt( obj, "y" );
            int w = getInt( obj, "w" );
            int h = getInt( obj, "h" );
            int rx = getInt( obj, "rx" );
            int ry = getInt( obj, "ry" );
            return CreateRoundRectRgn( x, y, x + w, y + h, rx * 2, ry * 2 );
        }

        if ( type == "ellipse" ) {
            int x = getInt( obj, "x" );
            int y = getInt( obj, "y" );
            int w = getInt( obj, "w" );
            int h = getInt( obj, "h" );
            return CreateEllipticRgn( x, y, x + w, y + h );
        }

        if ( type == "polygon" ) {
            if ( !obj.contains( "points" ) || !obj.at( "points" ).is_array() )
                return nullptr;
            auto &pts = obj.at( "points" ).as_array();
            if ( pts.size() < 3 )
                return nullptr;
            std::vector<POINT> points( pts.size() );
            for ( size_t i = 0; i < pts.size(); ++i ) {
                if ( !pts[i].is_array() )
                    return nullptr;
                auto &p = pts[i].as_array();
                if ( p.size() < 2 || !p[0].is_int64() || !p[1].is_int64() )
                    return nullptr;
                points[i] = { static_cast<LONG>( p[0].as_int64() ),
                              static_cast<LONG>( p[1].as_int64() ) };
            }
            return CreatePolygonRgn( points.data(), static_cast<int>( points.size() ), ALTERNATE );
        }

        if ( type == "combine" ) {
            if ( !obj.contains( "regions" ) || !obj.at( "regions" ).is_array() )
                return nullptr;
            auto &subs = obj.at( "regions" ).as_array();
            if ( subs.empty() )
                return nullptr;

            std::string op = obj.contains( "op" ) && obj.at( "op" ).is_string()
                                 ? std::string( obj.at( "op" ).as_string() )
                                 : "union";
            int combineMode = RGN_OR;
            if ( op == "intersect" )
                combineMode = RGN_AND;
            else if ( op == "diff" )
                combineMode = RGN_DIFF;
            else if ( op == "xor" )
                combineMode = RGN_XOR;

            HRGN result = createOne( subs[0].as_object() );
            for ( size_t i = 1; i < subs.size(); ++i ) {
                if ( !subs[i].is_object() )
                    continue;
                HRGN sub = createOne( subs[i].as_object() );
                if ( sub ) {
                    CombineRgn( result, result, sub, combineMode );
                    DeleteObject( sub );
                }
            }
            return result;
        }

        return nullptr;
    };

    if ( rgnList.empty() )
        return nullptr;

    HRGN result = createOne( rgnList[0].as_object() );
    if ( !result )
        return nullptr;

    for ( size_t i = 1; i < rgnList.size(); ++i ) {
        if ( !rgnList[i].is_object() )
            continue;
        HRGN sub = createOne( rgnList[i].as_object() );
        if ( sub ) {
            CombineRgn( result, result, sub, RGN_OR );
            DeleteObject( sub );
        }
    }

    return result;
}

static std::wstring buildFilterString( const std::vector<std::pair<std::string, std::string>> &filters ) {
    std::wstring result;
    for ( const auto &f : filters ) {
        result += strToWiden( f.first );
        result.push_back( L'\0' );
        result += strToWiden( f.second );
        result.push_back( L'\0' );
    }
    if ( result.empty() ) {
        result += L"All Files";
        result.push_back( L'\0' );
        result += L"*.*";
        result.push_back( L'\0' );
    }
    result.push_back( L'\0' );
    return result;
}

std::vector<std::string> openFileDialog(
    HWND owner,
    const std::string &title,
    const std::string &defaultDir,
    const std::string &defaultFilename,
    bool multiSelect,
    const std::vector<std::pair<std::string, std::string>> &filters ) {
    std::vector<std::string> result;

    OPENFILENAMEW ofn = {};
    std::wstring fileBuf;
    if ( multiSelect )
        fileBuf.resize( 32768, L'\0' );
    else
        fileBuf.resize( MAX_PATH, L'\0' );

    std::wstring wTitle = strToWiden( title );
    std::wstring wDir = strToWiden( defaultDir );
    std::wstring wDefFilename = strToWiden( defaultFilename );
    std::wstring wFilter = buildFilterString( filters );

    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = wFilter.c_str();
    ofn.lpstrFile = fileBuf.data();
    ofn.nMaxFile = static_cast<DWORD>( fileBuf.size() );
    ofn.lpstrInitialDir = wDir.empty() ? nullptr : wDir.c_str();
    ofn.lpstrTitle = wTitle.empty() ? nullptr : wTitle.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if ( multiSelect )
        ofn.Flags |= OFN_ALLOWMULTISELECT;
    if ( !wDefFilename.empty() ) {
        wcsncpy_s( fileBuf.data(), fileBuf.size(), wDefFilename.c_str(), _TRUNCATE );
    }

    if ( !GetOpenFileNameW( &ofn ) )
        return result;

    if ( multiSelect ) {
        wchar_t *p = fileBuf.data();
        std::wstring dir = p;
        p += dir.size() + 1;

        if ( *p == L'\0' ) {
            result.push_back( strToNarrow( dir ) );
        } else {
            while ( *p != L'\0' ) {
                std::wstring fullPath = dir + L"\\" + p;
                result.push_back( strToNarrow( fullPath ) );
                p += wcslen( p ) + 1;
            }
        }
    } else {
        result.push_back( strToNarrow( fileBuf.data() ) );
    }

    return result;
}

std::string saveFileDialog(
    HWND owner,
    const std::string &title,
    const std::string &defaultDir,
    const std::string &defaultFilename,
    const std::vector<std::pair<std::string, std::string>> &filters ) {
    OPENFILENAMEW ofn = {};
    std::wstring fileBuf( MAX_PATH, L'\0' );

    std::wstring wTitle = strToWiden( title );
    std::wstring wDir = strToWiden( defaultDir );
    std::wstring wDefFilename = strToWiden( defaultFilename );
    std::wstring wFilter = buildFilterString( filters );

    ofn.lStructSize = sizeof( ofn );
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = wFilter.c_str();
    ofn.lpstrFile = fileBuf.data();
    ofn.nMaxFile = static_cast<DWORD>( fileBuf.size() );
    ofn.lpstrInitialDir = wDir.empty() ? nullptr : wDir.c_str();
    ofn.lpstrTitle = wTitle.empty() ? nullptr : wTitle.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if ( !wDefFilename.empty() ) {
        wcsncpy_s( fileBuf.data(), fileBuf.size(), wDefFilename.c_str(), _TRUNCATE );
    }

    if ( !GetSaveFileNameW( &ofn ) )
        return {};

    return strToNarrow( fileBuf.data() );
}

std::string selectFolderDialog(
    HWND owner,
    const std::string &title,
    const std::string &defaultDir ) {
    std::wstring wTitle = strToWiden( title );
    std::wstring wDir = strToWiden( defaultDir );

    IFileDialog *pDialog = nullptr;
    HRESULT hr = CoCreateInstance( CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_IFileDialog, reinterpret_cast<void **>( &pDialog ) );
    if ( FAILED( hr ) )
        return {};

    pDialog->SetOptions( FOS_PICKFOLDERS | FOS_PATHMUSTEXIST );

    if ( !wTitle.empty() )
        pDialog->SetTitle( wTitle.c_str() );

    if ( !wDir.empty() ) {
        IShellItem *pItem = nullptr;
        hr = SHCreateItemFromParsingName( wDir.c_str(), nullptr, IID_IShellItem,
                                          reinterpret_cast<void **>( &pItem ) );
        if ( SUCCEEDED( hr ) ) {
            pDialog->SetDefaultFolder( pItem );
            pItem->Release();
        }
    }

    hr = pDialog->Show( owner );
    if ( FAILED( hr ) ) {
        pDialog->Release();
        return {};
    }

    IShellItem *pResult = nullptr;
    hr = pDialog->GetResult( &pResult );
    pDialog->Release();
    if ( FAILED( hr ) )
        return {};

    LPWSTR pPath = nullptr;
    hr = pResult->GetDisplayName( SIGDN_FILESYSPATH, &pPath );
    pResult->Release();
    if ( FAILED( hr ) )
        return {};

    std::string result = strToNarrow( pPath );
    CoTaskMemFree( pPath );
    return result;
}

} // namespace utils