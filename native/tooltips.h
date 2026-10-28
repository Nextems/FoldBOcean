#pragma once
// Tracking tooltips use the same animated hit test as clicks, including sectors.
HWND tooltipWindow{};
int tooltipTarget=-1;
std::wstring tooltipText;
TOOLINFOW trackingTool(){TOOLINFOW tool{sizeof(tool)};tool.hwnd=windowHandle;tool.uId=1;tool.uFlags=TTF_TRACK|TTF_ABSOLUTE|TTF_TRANSPARENT;return tool;}
void clearTooltips(){
    if(windowHandle)KillTimer(windowHandle,6);
    if(tooltipWindow){auto tool=trackingTool();SendMessageW(tooltipWindow,TTM_TRACKACTIVATE,FALSE,reinterpret_cast<LPARAM>(&tool));}
    tooltipTarget=-1;tooltipText.clear();
}
void syncTooltips(){
    if(!windowHandle||tooltipWindow)return;
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
    tooltipWindow=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,windowHandle,nullptr,instanceHandle,nullptr);
    if(!tooltipWindow)return;
    auto tool=trackingTool();tool.lpszText=const_cast<wchar_t*>(L"");SendMessageW(tooltipWindow,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tool));
    SendMessageW(tooltipWindow,TTM_SETMAXTIPWIDTH,0,320);
}
void trackTooltip(int target,POINT point){
    if(target!=tooltipTarget){clearTooltips();tooltipTarget=target;if(target>=0&&windowHandle)SetTimer(windowHandle,6,500,nullptr);}
    if(tooltipWindow)SendMessageW(tooltipWindow,TTM_TRACKPOSITION,0,MAKELPARAM(point.x+14,point.y+20));
}
void revealTooltip(){
    if(windowHandle)KillTimer(windowHandle,6);
    if(!visible||!tooltipWindow||tooltipTarget<0||static_cast<size_t>(tooltipTarget)>=entries.size())return;
    tooltipText=entries[tooltipTarget].name;auto tool=trackingTool();tool.lpszText=tooltipText.data();
    SendMessageW(tooltipWindow,TTM_UPDATETIPTEXTW,0,reinterpret_cast<LPARAM>(&tool));
    SendMessageW(tooltipWindow,TTM_TRACKACTIVATE,TRUE,reinterpret_cast<LPARAM>(&tool));
}
