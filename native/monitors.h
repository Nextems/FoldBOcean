#pragma once
struct Display {HMONITOR handle;MONITORINFOEXW info;};
BOOL CALLBACK collectDisplay(HMONITOR monitor,HDC,LPRECT,LPARAM context) {
    MONITORINFOEXW info{};info.cbSize=sizeof(info);
    if(GetMonitorInfoW(monitor,&info))reinterpret_cast<std::vector<Display>*>(context)->push_back({monitor,info});
    return TRUE;
}
std::vector<Display> displays() {
    std::vector<Display> result;EnumDisplayMonitors(nullptr,nullptr,collectDisplay,reinterpret_cast<LPARAM>(&result));return result;
}
MONITORINFOEXW launcherMonitor() {
    const auto id=configuration.contains("launcher")?configuration["launcher"].value("monitor",std::string("primary")):"primary";
    const auto current=displays();
    for(const auto& display:current)if(utf8(display.info.szDevice)==id)return display.info;
    for(const auto& display:current)if(display.info.dwFlags&MONITORINFOF_PRIMARY)return display.info;
    MONITORINFOEXW info{};info.cbSize=sizeof(info);GetMonitorInfoW(MonitorFromWindow(nullptr,MONITOR_DEFAULTTOPRIMARY),&info);return info;
}
RECT centeredBounds(RECT work,int width,int height) {
    const LONG x=work.left+(work.right-work.left-width)/2,y=work.top+(work.bottom-work.top-height)/2;
    return {x,y,x+width,y+height};
}
void refreshInvocationMonitor() noexcept {
    try{auto info=launcherMonitor();invocationMonitor=MonitorFromRect(&info.rcMonitor,MONITOR_DEFAULTTOPRIMARY);}
    catch(...){invocationMonitor=MonitorFromWindow(nullptr,MONITOR_DEFAULTTOPRIMARY);}
}
int exportMonitors() {
    json result=json::array();int number=0;
    for(const auto& display:displays()){
        const auto& info=display.info;const auto& rect=info.rcMonitor;
        result.push_back({{"id",utf8(info.szDevice)},{"number",++number},{"width",rect.right-rect.left},{"height",rect.bottom-rect.top},{"primary",bool(info.dwFlags&MONITORINFOF_PRIMARY)}});
    }
    const auto output=result.dump();DWORD written{};return WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),output.data(),static_cast<DWORD>(output.size()),&written,nullptr)&&written==output.size()?0:1;
}
