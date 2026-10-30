#pragma once

#include <string>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <Windows.h>

class EventLoop {
public:
    using TimerCallback = std::function<bool( UINT_PTR id )>;

    using DragMonitorCallback = std::function<void( int count, int x, int y )>;

    EventLoop() = default;

    ~EventLoop();

    EventLoop( const EventLoop & ) = delete;
    EventLoop &operator=( const EventLoop & ) = delete;
    EventLoop( EventLoop && ) = delete;
    EventLoop &operator=( EventLoop && ) = delete;

    /**
     * @brief 运行事件循环，阻塞直到 exit() 或 quit() 被调用
     *
     * 支持嵌套：可在已运行的事件循环中创建新实例并调用 exec()，
     * 形成嵌套消息循环。exit() 仅退出当前实例，quit() 退出所有循环。
     *
     * @param quitOnLastWindow 当最后一个顶级窗口关闭时是否自动退出，默认 true
     * @return 退出码
     */
    int exec( bool quitOnLastWindow = true );

    /**
     * @brief 退出此事件循环实例
     *
     * 仅退出当前实例的 exec()，不影响外层嵌套的事件循环。
     * 如需退出整个应用，请使用 quit()。
     *
     * @param exitCode 退出码，默认 0
     */
    void exit( int exitCode = 0 );

    /**
     * @brief 退出整个应用（所有事件循环）
     *
     * 设置全局退出标志，所有嵌套的事件循环都将退出。
     *
     * @param exitCode 退出码，默认 0
     */
    static void quit( int exitCode = 0 );

    /**
     * @brief 当前事件循环是否正在运行
     */
    bool isRunning() const;

    /**
     * @brief 设置消息处理函数，用于过滤或修改事件循环中的消息
     * @param handler 消息处理函数，返回 true 则跳过该消息，false 则继续处理
     */
    void setMessageHandler( std::function<bool( MSG &msg )> handler );

    /**
     * @brief 获取当前正在运行的事件循环（最内层）
     * @return 当前事件循环指针，无运行中的循环时返回 nullptr
     */
    static EventLoop *current();

    /**
     * @brief 向事件循环投递一个待执行的任务
     *
     * 线程安全。handler 将在 exec() 消息循环中被调用。
     *
     * @param handler 待执行的任务
     */
    static void postEvent( std::function<void()> handler );

    /**
     * @brief 开始定时器
     * @param intervalMs 定时器间隔（毫秒）
     * @param callback 定时器回调, 返回 true 继续定时器，false 停止定时器
     * @return 定时器 ID
     */
    static UINT_PTR startTimer( UINT intervalMs, TimerCallback callback );

    /**
     * @brief 停止定时器
     * @param timerId 定时器 ID
     */
    static void stopTimer( UINT_PTR timerId );

    /**
     * @brief 开始监控鼠标拖拽（基于消息循环，非阻塞）
     *
     * 在 exec() 消息循环中通过 WH_MOUSE_LL 钩子驱动回调。
     * 指定按钮当前未按下返回 false，否则返回 true 并在每次鼠标移动时调用回调，直到按钮松开自动移除。
     * 同一按钮可注册多个回调，按钮松开时全部自动移除并收到 count=0 的结束通知。
     *
     * @param button 监控的鼠标按钮，1=左键，2=右键，3=中键
     * @param callback 拖拽回调
     * @param minIntervalMs 最小更新时间间隔（毫秒），0=每次移动都回调。
     *   大于 0 时，距离上次回调时长小于此值则跳过，变化量累积到下次回调时一起发出。
     * @return true 表示开始成功
     */
    static bool startDragMonitor( int button, DragMonitorCallback callback, UINT minIntervalMs = 0 );

    /**
     * @brief 移除指定按钮的所有拖拽监控回调
     * @param button 鼠标按钮，0=全部，1=左键，2=右键，3=中键
     */
    static void stopDragMonitor( int button = 0 );

private:
    static void wakeUp();

    static void onWindowVisibleChanged( bool visible );
    friend class Widget;

    bool m_quit = false;
    int m_exitCode = 0;
    bool m_running = false;
    bool m_quitOnLastWindow = true;

    std::function<bool( MSG &msg )> m_messageHandler = nullptr;

    static std::vector<EventLoop *> s_loopStack;

    static bool s_appQuitting;
    static int s_appExitCode;

    static int s_windowCount;

    static constexpr UINT s_wmPostEvent = WM_USER + 0x100;
    static constexpr UINT s_wmWakeUp = WM_USER + 0x101;

    static DWORD s_eventThreadId;
    static std::mutex s_eventMutex;
    static std::vector<std::function<void()>> s_eventQueue;

    struct DragMonitorEntry {
        int count = 0;
        POINT prev = {};
        DWORD lastTime = 0;
        UINT minIntervalMs = 0;
        DragMonitorCallback callback;
    };
    static HHOOK s_dragHook;
    static std::unordered_map<int, std::vector<DragMonitorEntry>> s_dragMonitors;

    static std::unordered_map<UINT_PTR, TimerCallback> s_timers;
    static std::mutex s_timersMutex;

    static void CALLBACK globalTimerProc( HWND hwnd, UINT msg, UINT_PTR id, DWORD time );
    static LRESULT CALLBACK dragMonitorHookProc( int nCode, WPARAM wParam, LPARAM lParam );
};