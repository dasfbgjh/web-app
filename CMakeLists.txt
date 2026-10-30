cmake_minimum_required(VERSION 3.10)
project(web-app
    VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

find_package(Boost REQUIRED CONFIG COMPONENTS
    asio
    process
    filesystem
    regex
    json
)

include(${CMAKE_CURRENT_SOURCE_DIR}/appInfo/AppInfo.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/lib/webview.cmake)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(WIN_GUI WIN32)
endif()

# =====================================================
# 测试
add_executable(test ${WIN_GUI}
    ${APPINFO_SRC}
    src/main.cpp
)
target_include_directories(test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_options(test PRIVATE
    -static-libstdc++ -static-libgcc -static
)
add_webview_to_target(test)
