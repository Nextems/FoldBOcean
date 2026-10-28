#pragma once
#include <dwmapi.h>

// Updated on the UI thread, read by the hook thread without touching JSON.
std::atomic<HMONITOR> invocationMonitor{};
void refreshInvocationMonitor() noexcept;
bool coversDisplay(RECT window,RECT display,bool maximized,LONG_PTR style){
    constexpr LONG tolerance=2;
    const bool covers=window.left<=display.left+tolerance&&window.top<=display.top+tolerance&&
        window.right>=display.right-tolerance&&window.bottom>=display.bottom-tolerance;
    return covers&&(!maximized||(style&WS_CAPTION)==0);
}
bool invocationPassThrough(bool fullscreen,bool held){return fullscreen&&!held;}
bool ignoredFullscreenOverlay(LONG_PTR style){return (style&(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT))!=0;}
struct FullscreenQuery {HMONITOR monitor;RECT bounds;bool blocked=false;};
BOOL CALLBACK inspectFullscreenWindow(HWND candidate,LPARAM context){
    auto& query=*reinterpret_cast<FullscreenQuery*>(context);
    if(!IsWindowVisible(candidate)||IsIconic(candidate))return TRUE;
    DWORD process=0;GetWindowThreadProcessId(candidate,&process);
    if(process==GetCurrentProcessId())return TRUE;
    const auto extended=GetWindowLongPtrW(candidate,GWL_EXSTYLE);
    if(ignoredFullscreenOverlay(extended))return TRUE;
    wchar_t name[64]{};GetClassNameW(candidate,name,64);
    for(const auto* ignored:{L"Progman",L"WorkerW",L"Shell_TrayWnd",L"Shell_SecondaryTrayWnd"})if(wcscmp(name,ignored)==0)return TRUE;
    DWORD cloaked=0;if(SUCCEEDED(DwmGetWindowAttribute(candidate,DWMWA_CLOAKED,&cloaked,sizeof(cloaked)))&&cloaked)return TRUE;
    RECT bounds{};if(!GetWindowRect(candidate,&bounds))return TRUE;
    RECT frame{};if(SUCCEEDED(DwmGetWindowAttribute(candidate,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame))))bounds=frame;
    const auto style=GetWindowLongPtrW(candidate,GWL_STYLE);
    const bool fullscreen=coversDisplay(bounds,query.bounds,IsZoomed(candidate)!=FALSE,style);
    if(MonitorFromWindow(candidate,MONITOR_DEFAULTTONULL)!=query.monitor&&!fullscreen)return TRUE;
    // EnumWindows walks front to back: an ordinary window covering a background
    // fullscreen application means that display is currently usable normally.
    query.blocked=fullscreen;return FALSE;
}
bool fullscreenBlocksInvocation() noexcept {
    auto monitor=invocationMonitor.load();if(!monitor)monitor=MonitorFromWindow(nullptr,MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{sizeof(info)};if(!GetMonitorInfoW(monitor,&info))return false;
    FullscreenQuery query{monitor,info.rcMonitor};EnumWindows(inspectFullscreenWindow,reinterpret_cast<LPARAM>(&query));return query.blocked;
}
