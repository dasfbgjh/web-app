cmake_minimum_required(VERSION 3.10)

# 注意：版本文件不能命名为 VERSION。Windows 文件系统大小写不敏感，
# 且本目录会作为头文件搜索路径加入下游目标，命名为 VERSION 会与 C++
# 标准头 <version>（Boost 等库会包含它）冲突，导致编译失败。
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/quickjs_version.txt QUICKJS_VERSION)
string(STRIP "${QUICKJS_VERSION}" QUICKJS_VERSION)

set(QUICKJS_SOURCES
    quickjs.c
    dtoa.c
    libregexp.c
    libunicode.c
    cutils.c
    quickjs-libc.c
)

add_library(quickjs STATIC ${QUICKJS_SOURCES})

target_include_directories(quickjs SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(quickjs PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION="${QUICKJS_VERSION}"
)

if(WIN32)
    target_compile_definitions(quickjs PRIVATE __USE_MINGW_ANSI_STDIO)
endif()

if(MSVC)
    target_compile_definitions(quickjs PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(quickjs PRIVATE /W0)
else()
    target_compile_options(quickjs PRIVATE
        -w
        -fwrapv
    )
endif()

find_package(Threads REQUIRED)
target_link_libraries(quickjs PUBLIC Threads::Threads)

if(UNIX)
    target_link_libraries(quickjs PUBLIC m)
endif()