#include "EventLoop.h"

std::vector<EventLoop *> EventLoop::s_loopStack;

bool EventLoop::s_appQuitting = false;
int EventLoop::s_appExitCode = 0;

int EventLoop::s_windowCount = 0;

DWORD EventLoop::s_eventThreadId = 0;
std::mutex EventLoop::s_eventMutex;
std::vector<std::function<void()>> EventLoop::s_eventQueue;

HHOOK EventLoop::s_dragHook = nullptr;
std::unordered_map<int, std::vector<EventLoop::DragMonitorEntry>> EventLoop::s_dragMonitors;

std::unordered_map<UINT_PTR, EventLoop::TimerCallback> EventLoop::s_timers;
std::mutex EventLoop::s_timersMutex;

EventLoop::~EventLoop() {
    if ( m_running )
        exit();
}

int EventLoop::exec( bool quitOnLastWindow ) {
    bool isOutermost = s_loopStack.empty();

    if ( isOutermost ) {
        s_appQuitting = false;
        s_appExitCode = 0;
    }

    m_quit = false;
    m_exitCode = 0;
    m_running = true;
    m_quitOnLastWindow = quitOnLastWindow;
    s_eventThreadId = GetCurrentThreadId();

    s_loopStack.push_back( this );

    MSG msg = {};
    while ( !m_quit && !s_appQuitting ) {
        BOOL ret = GetMessageW( &msg, nullptr, 0, 0 );
        if ( ret == -1 ) {
            break;
        }
        if ( ret == 0 ) {
            s_appExitCode = static_cast<int>( msg.wParam );
            s_appQuitting = true;
            break;
        }

        if ( msg.message == s_wmWakeUp ) {
            continue;
        }

        if ( msg.message == s_wmPostEvent ) {
            std::vector<std::function<void()>> tasks;
            {
                std::lock_guard<std::mutex> lock( s_eventMutex );
                tasks = std::move( s_eventQueue );
                s_eventQueue.clear();
            }
            for ( auto &task : tasks )
                task();
            continue;
        }

        if ( msg.message == WM_TIMER && msg.hwnd == nullptr ) {
            globalTimerProc( nullptr, WM_TIMER, static_cast<UINT_PTR>( msg.wParam ), static_cast<DWORD>( msg.lParam ) );
            continue;
        }

        if ( m_messageHandler && m_messageHandler( msg ) ) {
            continue;
        }

        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }

    s_loopStack.pop_back();
    m_running = false;

    if ( s_appQuitting )
        m_exitCode = s_appExitCode;

    if ( isOutermost ) {
        if ( s_dragHook ) {
            UnhookWindowsHookEx( s_dragHook );
            s_dragHook = nullptr;
        }
        s_dragMonitors.clear();

        {
            std::lock_guard<std::mutex> lock( s_eventMutex );
            s_eventQueue.clear();
        }
    }

    return m_exitCode;
}

void EventLoop::exit( int exitCode ) {
    m_exitCode = exitCode;
    m_quit = true;
    wakeUp();
}

void EventLoop::quit( int exitCode ) {
    s_appExitCode = exitCode;
    s_appQuitting = true;
    wakeUp();
}

bool EventLoop::isRunning() const {
    return m_running;
}

void EventLoop::setMessageHandler( std::function<bool( MSG &msg )> handler ) {
    m_messageHandler = std::move( handler );
}

EventLoop *EventLoop::current() {
    if ( s_loopStack.empty() )
        return nullptr;
    return s_loopStack.back();
}

void EventLoop::wakeUp() {
    if ( s_eventThreadId != 0 )
        PostThreadMessageW( s_eventThreadId, s_wmWakeUp, 0, 0 );
}

void EventLoop::postEvent( std::function<void()> handler ) {
    if ( !handler )
        return;
    {
        std::lock_guard<std::mutex> lock( s_eventMutex );
        s_eventQueue.push_back( std::move( handler ) );
    }
    PostThreadMessageW( s_eventThreadId, s_wmPostEvent, 0, 0 );
}

UINT_PTR EventLoop::startTimer( UINT intervalMs, TimerCallback callback ) {
    if ( !callback )
        return 0;

    UINT_PTR id = SetTimer( nullptr, 0, intervalMs, globalTimerProc );
    if ( !id )
        return 0;

    std::lock_guard<std::mutex> lock( s_timersMutex );
    s_timers[id] = std::move( callback );
    return id;
}

void EventLoop::stopTimer( UINT_PTR timerId ) {
    std::lock_guard<std::mutex> lock( s_timersMutex );
    auto it = s_timers.find( timerId );
    if ( it != s_timers.end() ) {
        KillTimer( nullptr, timerId );
        s_timers.erase( it );
    }
}

void CALLBACK EventLoop::globalTimerProc( HWND hwnd, UINT msg, UINT_PTR id, DWORD time ) {
    TimerCallback cb;
    {
        std::lock_guard<std::mutex> lock( s_timersMutex );
        auto it = s_timers.find( id );
        if ( it == s_timers.end() )
            return;
        cb = it->second;
    }
    bool cont = cb( id );
    if ( !cont ) {
        std::lock_guard<std::mutex> lock( s_timersMutex );
        auto it = s_timers.find( id );
        if ( it != s_timers.end() ) {
            KillTimer( nullptr, id );
            s_timers.erase( it );
        }
    }
}

bool EventLoop::startDragMonitor( int button, DragMonitorCallback callback, UINT minIntervalMs ) {
    if ( button < 1 || button > 3 || !callback )
        return false;

    int vk = button == 1   ? VK_LBUTTON
             : button == 2 ? VK_RBUTTON
                           : VK_MBUTTON;

    if ( !( GetAsyncKeyState( vk ) & 0x8000 ) )
        return false;

    if ( !s_dragHook ) {
        s_dragHook = SetWindowsHookExW( WH_MOUSE_LL, dragMonitorHookProc,
                                        GetModuleHandleW( nullptr ), 0 );
        if ( !s_dragHook )
            return false;
    }

    DragMonitorEntry entry;
    entry.count = 0;
    GetCursorPos( &entry.prev );
    entry.lastTime = GetTickCount();
    entry.minIntervalMs = minIntervalMs;
    entry.callback = std::move( callback );

    s_dragMonitors[button].push_back( std::move( entry ) );
    return true;
}

void EventLoop::stopDragMonitor( int button ) {
    if ( button == 0 ) {
        s_dragMonitors.clear();
    } else {
        s_dragMonitors.erase( button );
    }
    if ( s_dragMonitors.empty() && s_dragHook ) {
        HHOOK hook = s_dragHook;
        s_dragHook = nullptr;
        postEvent( [hook]() { UnhookWindowsHookEx( hook ); } );
    }
}

LRESULT CALLBACK EventLoop::dragMonitorHookProc( int nCode, WPARAM wParam, LPARAM lParam ) {
    if ( nCode == HC_ACTION && !s_dragMonitors.empty() ) {
        auto *ms = reinterpret_cast<MSLLHOOKSTRUCT *>( lParam );

        if ( wParam == WM_MOUSEMOVE ) {
            DWORD now = GetTickCount();
            for ( auto &[btn, entries] : s_dragMonitors ) {
                for ( auto &e : entries ) {
                    int dx = ms->pt.x - e.prev.x;
                    int dy = ms->pt.y - e.prev.y;
                    if ( dx != 0 || dy != 0 ) {
                        if ( e.minIntervalMs == 0 || now - e.lastTime >= e.minIntervalMs ) {
                            ++e.count;
                            e.callback( e.count, dx, dy );
                            e.prev = ms->pt;
                            e.lastTime = now;
                        }
                    }
                }
            }
        }

        int upButton = 0;
        if ( wParam == WM_LBUTTONUP )
            upButton = 1;
        else if ( wParam == WM_RBUTTONUP )
            upButton = 2;
        else if ( wParam == WM_MBUTTONUP )
            upButton = 3;

        if ( upButton != 0 ) {
            auto it = s_dragMonitors.find( upButton );
            if ( it != s_dragMonitors.end() ) {
                auto entries = std::move( it->second );
                s_dragMonitors.erase( it );
                for ( auto &e : entries ) {
                    int dx = ms->pt.x - e.prev.x;
                    int dy = ms->pt.y - e.prev.y;
                    if ( dx != 0 || dy != 0 ) {
                        ++e.count;
                        e.callback( e.count, dx, dy );
                    }
                    e.callback( 0, ms->pt.x, ms->pt.y );
                }
            }
            if ( s_dragMonitors.empty() && s_dragHook ) {
                HHOOK hook = s_dragHook;
                s_dragHook = nullptr;
                postEvent( [hook]() { UnhookWindowsHookEx( hook ); } );
            }
        }
    }
    return CallNextHookEx( s_dragHook, nCode, wParam, lParam );
}

void EventLoop::onWindowVisibleChanged( bool visible ) {
    if ( visible ) {
        s_windowCount++;
    } else {
        s_windowCount--;
        if ( s_windowCount <= 0 ) {
            EventLoop *loop = current();
            if ( loop && loop->m_quitOnLastWindow )
                loop->exit();
        }
    }
}