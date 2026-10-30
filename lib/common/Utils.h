#ifndef UTILS_H
#define UTILS_H

#include <filesystem>

#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>
#include <boost/process/v2/start_dir.hpp>
#include <boost/process/v2/environment.hpp>

#if defined( _WIN32 )
#include <boost/process/v2/windows/creation_flags.hpp>
#include <boost/process/v2/windows/show_window.hpp>
#endif

namespace json {
using namespace boost::json;
/**
 * @brief 从 JSON 对象中读取整数字段
 * @param obj JSON 对象
 * @param key 字段名
 * @param defaultVal 字段缺失或类型不符时的默认值
 * @return 字段值
 */
inline int64_t jsonGetInt( const object &obj, const char *key, int64_t defaultVal = 0 ) {
    if ( !obj.contains( key ) )
        return defaultVal;
    const value &val = obj.at( key );
    if ( val.is_int64() )
        return val.as_int64();
    if ( val.is_uint64() )
        return static_cast<int64_t>( val.as_uint64() );
    if ( val.is_double() )
        return static_cast<int64_t>( val.as_double() );
    return defaultVal;
}

/**
 * @brief 从 JSON 对象中读取布尔字段
 * @param obj JSON 对象
 * @param key 字段名
 * @param defaultVal 字段缺失或类型不符时的默认值
 * @return 字段值
 */
inline bool jsonGetBool( const object &obj, const char *key, bool defaultVal = false ) {
    if ( !obj.contains( key ) )
        return defaultVal;
    const value &val = obj.at( key );
    if ( val.is_bool() )
        return val.as_bool();
    if ( val.is_int64() )
        return val.as_int64() != 0;
    if ( val.is_uint64() )
        return val.as_uint64() != 0;
    return defaultVal;
}

/**
 * @brief 从 JSON 对象中读取字符串字段
 * @param obj JSON 对象
 * @param key 字段名
 * @param defaultVal 字段缺失或类型不符时的默认值
 * @return 字段值
 */
inline std::string jsonGetString( const object &obj, const char *key, const std::string &defaultVal = {} ) {
    if ( !obj.contains( key ) )
        return defaultVal;
    const value &val = obj.at( key );
    if ( val.is_string() )
        return std::string( val.as_string() );
    return defaultVal;
}

} // namespace json

namespace bio {
using context = boost::asio::io_context;
using pipe_read = boost::asio::readable_pipe;
using pipe_write = boost::asio::writable_pipe;
using timer = boost::asio::steady_timer;
using error_code = boost::system::error_code;
} // namespace bio

namespace bp {
using process = boost::process::v2::process;
using process_env = boost::process::v2::process_environment;
using process_stdio = boost::process::v2::process_stdio;
using process_startdir = boost::process::v2::process_start_dir;
} // namespace bp

namespace utils {
/**
 * @brief 将 UTF-8 字符串转换为宽字符串
 * @param str UTF-8 字符串
 * @return 宽字符串
 */
std::wstring strToWiden( const std::string &str );

/**
 * @brief 将宽字符串转换为 UTF-8 字符串
 * @param wstr 宽字符串
 * @return UTF-8 字符串
 */
std::string strToNarrow( const std::wstring &wstr );

std::string urlDecode( const std::string &str );

/**
 * @brief 猜测文件类型的内容类型
 * @param path 文件路径
 * @return 内容类型
 */
std::string guessContentType( const std::filesystem::path &path );

/**
 * @brief 读取资源文件内容
 * @param path 资源文件路径
 * @return 资源文件内容
 */
std::string readResource( const std::string &path );

/**
 * @brief 从文件加载图标
 * @param filePath 图标文件路径
 * @param width 图标宽度，默认 0 表示使用文件宽度
 * @param height 图标高度，默认 0 表示使用文件高度
 * @return 图标句柄
 */
HICON iconFromFile( const std::string &filePath, int width = 0, int height = 0 );
HICON iconFromMemory( const BYTE *data, size_t size );
HICON iconFromResource( const std::string &path );
std::pair<std::string, std::string> iconExeToIconData( const std::string &exePath, int size = 32 );

/**
 * @brief 从 JSON 配置创建裁剪区域
 *
 * 支持的形状类型及配置格式：
 *
 * 矩形:
 *   { "type": "rect", "x": 0, "y": 0, "w": 100, "h": 100 }
 *
 * 圆角矩形:
 *   { "type": "roundRect", "x": 0, "y": 0, "w": 100, "h": 100, "rx": 10, "ry": 10 }
 *
 * 椭圆:
 *   { "type": "ellipse", "x": 0, "y": 0, "w": 100, "h": 100 }
 *
 * 多边形:
 *   { "type": "polygon", "points": [[x1,y1], [x2,y2], [x3,y3], ...] }
 *
 * 组合（将多个子区域合并为一个区域）:
 *   { "type": "combine", "op": "union", "regions": [ {...}, {...} ] }
 *   op 可选值: "union"(并集), "intersect"(交集), "diff"(差集), "xor"(异或)
 *
 * 多个顶层区域自动取并集。
 *
 * @param rgnList 区域配置列表
 * @return HRGN 区域句柄，失败返回 nullptr
 */
HRGN createRgn( const json::array &rgnList );

/**
 * @brief 打开文件选择对话框
 *
 * 基于 Win32 GetOpenFileNameW，支持单选和多选。
 * 多选时返回所有选中文件路径，单选时返回 0 或 1 个元素。
 *
 * @param owner 父窗口句柄，模态时禁用该窗口
 * @param title 对话框标题
 * @param defaultDir 默认打开目录
 * @param defaultFilename 默认文件名
 * @param multiSelect 是否允许多选
 * @param filters 文件类型过滤器，每项 {显示名, 规格}，如 {"Image Files", "*.png;*.jpg"}，为空则默认 "All Files (*.*)"
 * @return 选中文件路径列表（UTF-8），取消或失败返回空
 */
std::vector<std::string> openFileDialog(
    HWND owner = nullptr,
    const std::string &title = "选择文件",
    const std::string &defaultDir = ".",
    const std::string &defaultFilename = "",
    bool multiSelect = false,
    const std::vector<std::pair<std::string, std::string>> &filters = {} );

/**
 * @brief 打开保存文件对话框
 *
 * 基于 Win32 GetSaveFileNameW，覆盖已有文件时自动弹出确认提示。
 *
 * @param owner 父窗口句柄，模态时禁用该窗口
 * @param title 对话框标题
 * @param defaultDir 默认打开目录
 * @param defaultFilename 默认文件名
 * @param filters 文件类型过滤器，每项 {显示名, 规格}，如 {"Text Files", "*.txt"}，为空则默认 "All Files (*.*)"
 * @return 保存文件路径（UTF-8），取消或失败返回空字符串
 */
std::string saveFileDialog(
    HWND owner = nullptr,
    const std::string &title = "保存文件",
    const std::string &defaultDir = ".",
    const std::string &defaultFilename = "",
    const std::vector<std::pair<std::string, std::string>> &filters = {} );

/**
 * @brief 打开文件夹选择对话框
 *
 * 基于 Vista+ IFileDialog (FOS_PICKFOLDERS)，支持自定义标题和默认目录。
 *
 * @param owner 父窗口句柄，模态时禁用该窗口
 * @param title 对话框标题
 * @param defaultDir 默认打开目录
 * @return 选中文件夹路径（UTF-8），取消或失败返回空字符串
 */
std::string selectFolderDialog(
    HWND owner = nullptr,
    const std::string &title = "选择文件夹",
    const std::string &defaultDir = "." );

} // namespace utils
#endif