#pragma once

#include "webview/WebviewWidget.h"
#include "webview/EventLoop.h"

class Dialog : public WebviewWidget {
public:
    Dialog() = default;
    ~Dialog() = default;

    int exec( Widget *parent );
    void exit( int code );

protected:
    LRESULT onClose() override;

private:
    EventLoop m_eventLoop;
};