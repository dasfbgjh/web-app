#pragma once

#include "webview/WebviewWidget.h"
#include "webview/WebviewWindow.h"
#include "webview/Widget.h"
#include "common/Logger.hpp"
#include "common/Utils.h"
#include "webview/EventLoop.h"
#include "webview/TrayIcon.h"
#include "webview/Dialog.h"

class TestWindow : public WebviewWindow {
private:
    const std::string contentName = "content";

public:
    TestWindow() : WebviewWindow() {
        WebviewWindow::Config cfg;
        cfg.window.title = "WebviewWindow Frame Demo";
        cfg.window.width = 1400;
        cfg.window.height = 1000;
        cfg.window.center = true;
        cfg.window.resizable = true;
        cfg.window.noBorder = true;
        cfg.window.transparentBackground = true;
        create( cfg );
    }

    static void run() {
        EventLoop loop;
        TestWindow w;
        loop.exec();
    }

protected:
    void onMainWebviewCreateComplete( bool success, WebviewWidget *mainWebview ) {
        WebviewWindow::onMainWebviewCreateComplete( success, mainWebview );
        if ( !success ) {
            LOG_WARN << "main webview create fail !";
            return;
        }
        mainWebview->setVirtualHostNameForResources( "test.app", "/html/app" );
        std::string url = mainWebview->getResourceUrl( "/window/macos.html" );
        url += "?contentName=" + contentName;
        mainWebview->navigate( url );
    }

    void onSubWebviewCreate( WebviewWidget::Config &config, WebviewWidget *subWebview ) {
        WebviewWindow::onSubWebviewCreate( config, subWebview );
        config.window.transparentBackground = true;
    }

    void onSubWebviewCreateComplete( bool success, WebviewWidget *subWebview ) {
        WebviewWindow::onSubWebviewCreateComplete( success, subWebview );
        if ( !success ) {
            LOG_WARN << "sub webview create fail !";
            return;
        }
        if ( subWebview->getName() == contentName ) {
            subWebview->navigate( subWebview->getResourceUrl( "/index.html" ) );
        }
    }
};

void test1() {
    EventLoop loop;

    WebviewWidget w;
    WebviewWidget::Config cfg;
    cfg.window.title = "WebviewWindow Frame Demo";
    cfg.window.width = 1400;
    cfg.window.height = 1000;
    cfg.window.center = true;
    cfg.window.resizable = true;
    cfg.window.noBorder = true;
    cfg.window.transparentBackground = true;

    w.create( cfg, []( bool success, WebviewWidget *ptr ) {
        if ( !success ) {
            LOG_WARN << "sub webview create fail !";
            return;
        }

        ptr->setVirtualHostNameForResources( "test.app", "/html/app" );
        ptr->navigate( ptr->getResourceUrl( "/window/macos.html" ) );
    } );

    loop.exec();
}
