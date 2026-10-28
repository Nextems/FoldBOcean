#include <windows.h>
#include <cwchar>
MONITORINFO fixtureMonitor{sizeof(fixtureMonitor)};
LRESULT CALLBACK fixtureWindow(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_APP+1){
        const bool overlay=wp==3,fullscreen=wp==0||overlay;
        ShowWindow(window,SW_RESTORE);SetWindowLongPtrW(window,GWL_STYLE,fullscreen&&!overlay?WS_POPUP:WS_OVERLAPPEDWINDOW);
        SetWindowLongPtrW(window,GWL_EXSTYLE,WS_EX_TOPMOST|(overlay?WS_EX_TRANSPARENT|WS_EX_LAYERED:0));
        RECT rect=fixtureMonitor.rcMonitor;if(overlay){rect.left=GetSystemMetrics(SM_XVIRTUALSCREEN);rect.top=GetSystemMetrics(SM_YVIRTUALSCREEN);rect.right=rect.left+GetSystemMetrics(SM_CXVIRTUALSCREEN);rect.bottom=rect.top+GetSystemMetrics(SM_CYVIRTUALSCREEN);SetLayeredWindowAttributes(window,0,0,LWA_ALPHA);}
        const int width=fullscreen?rect.right-rect.left:500,height=fullscreen?rect.bottom-rect.top:320;
        SetWindowPos(window,HWND_TOPMOST,rect.left,rect.top,width,height,SWP_FRAMECHANGED|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        if(wp==2)ShowWindow(window,SW_MAXIMIZE);
        return 0;
    }
    if(message==WM_APP+2){fixtureMonitor.cbSize=sizeof(fixtureMonitor);GetMonitorInfoW(reinterpret_cast<HMONITOR>(lp),&fixtureMonitor);return SendMessageW(window,WM_APP+1,wp,0);}
    if(message==WM_TIMER||message==WM_CLOSE){DestroyWindow(window);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    unsigned long long monitor=0;
    if(swscanf_s(command,L"--fullscreen-fixture %llu",&monitor)==1){
        GetMonitorInfoW(reinterpret_cast<HMONITOR>(monitor),&fixtureMonitor);
        WNDCLASSW type{};type.hInstance=instance;type.lpfnWndProc=fixtureWindow;type.lpszClassName=L"FoldBOcean.FullscreenFixture";type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&type);
        auto window=CreateWindowExW(WS_EX_TOPMOST,type.lpszClassName,L"FoldBOcean fullscreen test",WS_POPUP,0,0,500,320,nullptr,nullptr,instance,nullptr);
        if(!window)return 2;SendMessageW(window,WM_APP+1,0,0);SetTimer(window,1,30000,nullptr);
        MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}return 0;
    }
    HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,L"Local\\SpiderWeb.Test.Launched");
    if(event){SetEvent(event);CloseHandle(event);return 0;}
    return 1;
}
