set(WEBVIEW_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/webview/EventLoop.cpp
    ${CMAKE_CURRENT_LIST_DIR}/webview/Widget.cpp
    ${CMAKE_CURRENT_LIST_DIR}/webview/WebView.cpp
    ${CMAKE_CURRENT_LIST_DIR}/webview/WebviewWidget.cpp
    ${CMAKE_CURRENT_LIST_DIR}/webview/WebviewWindow.cpp
    ${CMAKE_CURRENT_LIST_DIR}/webview/Dialog.cpp
    ${CMAKE_CURRENT_LIST_DIR}/webview/PopupWebview.cpp
    ${CMAKE_CURRENT_LIST_DIR}/webview/OverlayWebview.cpp
    ${CMAKE_CURRENT_LIST_DIR}/webview/TrayIcon.cpp
    ${CMAKE_CURRENT_LIST_DIR}/common/Utils.cpp
)

set(WEBVIEW_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}
)

set(WEBVIEW_LIBRARIES
    Boost::asio
    Boost::process
    Boost::filesystem
    Boost::regex
    Boost::json

    # webview2
    advapi32 ole32 oleaut32 shell32 shlwapi user32
    version uuid dwmapi comdlg32
)

set(WEBVIEW_COMPILE_DEFINITIONS
    -DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    -DWIN32_LEAN_AND_MEAN
)

set(RESOURCE_NAME_PREIFX ${PROJECT_NAME})
set(RESOURCE_PATH ${CMAKE_CURRENT_LIST_DIR}/../static)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../resource)
list(APPEND WEBVIEW_LIBRARIES
    ${RESOURCE_NAME_PREIFX}_resource
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND WEBVIEW_COMPILE_DEFINITIONS
        -DRESOURCE_PATH="${RESOURCE_PATH}"
    )
endif()

if(ADD_HTTP_SERVER)
    list(APPEND WEBVIEW_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/http/HttpClient.cpp
        ${CMAKE_CURRENT_LIST_DIR}/http/HttpServer.cpp
    )

    list(APPEND WEBVIEW_LIBRARIES
        Boost::beast
        mswsock
    )

    find_package(OpenSSL REQUIRED)

    if(OPENSSL_FOUND)
        list(APPEND WEBVIEW_COMPILE_DEFINITIONS
            -DHTTP_ENABLE_SSL
        )
        list(APPEND WEBVIEW_LIBRARIES
            OpenSSL::SSL
            OpenSSL::Crypto
        )
    endif()
endif()

macro(add_webview_to_target name)
    target_sources(${name} PRIVATE ${WEBVIEW_SOURCES})
    target_include_directories(${name} PRIVATE ${WEBVIEW_INCLUDE_DIRS})
    target_link_libraries(${name} PRIVATE ${WEBVIEW_LIBRARIES})
    target_compile_definitions(${name} PRIVATE ${WEBVIEW_COMPILE_DEFINITIONS})
endmacro()
