#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <gdiplus.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <atomic>
#include <thread>
#include <algorithm>
#include <map>
#include <set>
#include <cmath>
#include "vendor/json.hpp"
#include "resource.h"
#include "tree.h"
#include "animation.h"
#include "radial.h"
#include "command-line.h"

using json = nlohmann::json;
using namespace Gdiplus;
namespace fs = std::filesystem;
constexpr wchar_t CLASS_NAME[] = L"SpiderWeb.Native.Launcher";
constexpr UINT TRAY = WM_APP+1, BIND_DOWN = WM_APP+2, BIND_UP = WM_APP+3,
    POINTER_MOVE = WM_APP+4, KEY_ACTION = WM_APP+5, RELOAD = WM_APP+6,
    DEMO = WM_APP+7, QUIT = WM_APP+8, OUTSIDE = WM_APP+9, HOOK_ERROR = WM_APP+10, QUIT_ALL = WM_APP+11, SYNC_HOOKS = WM_APP+12, ICON_READY=WM_APP+13;
constexpr double PI=3.14159265358979323846;
HWND windowHandle{}, previousWindow{};
HINSTANCE instanceHandle{};
HANDLE stopEvent{};
std::atomic<bool> visible=false, movePending=false;
std::atomic<bool> configDirty=false;
std::atomic<int> triggerKey=VK_OEM_3, triggerModifiers=2;
std::atomic<bool> keyboardTrigger=true;
#include "fullscreen.h"
std::thread hookThread, watcherThread;
std::atomic<DWORD> hookThreadId=0;
void syncHookState(){const DWORD thread=hookThreadId;if(thread)PostThreadMessageW(thread,SYNC_HOOKS,0,0);}
json configuration;
std::wstring configError;
fs::path configPath, exeDirectory;
NOTIFYICONDATAW trayData{};
UINT taskbarCreated{};
std::atomic<bool> paused=false;
bool demoMode=false, holdMode=true, bindHeld=false;
int hover=-1;
std::atomic<float> scale=1;
std::vector<std::string> navigation;
struct Item { std::string id; std::wstring name,path; int position; bool group; bool noIcon=false; };
std::vector<Item> entries;

std::wstring wide(const std::string& value) {
    if(value.empty())return {};
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    if(!count)throw std::runtime_error("Invalid UTF-8");
    std::wstring result(count,L' ');
    MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),count);
    return result;
}
std::string utf8(const std::wstring& value) {
    int count=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    std::string result(count,' ');WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),count,nullptr,nullptr);return result;
}
std::string compactName(const std::string& name,int limit=256) {
    auto value=wide(name);size_t end=0;
    for(int count=0;count<limit&&end<value.size();++count){const auto c=value[end++];if(c>=0xD800&&c<=0xDBFF&&end<value.size()&&value[end]>=0xDC00&&value[end]<=0xDFFF)++end;}
    value.resize(end);int count=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);std::string result(count,' ');
    WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),result.data(),count,nullptr,nullptr);return result;
}
std::wstring menuLabel(const std::wstring& name,int limit){const auto value=utf8(name),shortened=compactName(value,limit);return shortened==value?name:wide(compactName(value,limit-3))+L"...";}
void notify(const std::wstring& message) {
    trayData.uFlags=NIF_INFO;
    wcscpy_s(trayData.szInfoTitle,L"FoldBOcean");
    wcsncpy_s(trayData.szInfo,message.c_str(),_TRUNCATE);
    trayData.dwInfoFlags=NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY,&trayData);
}
json defaults() {
    static const auto presets=[](){auto resource=FindResourceW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDR_PRESETS),RT_RCDATA);auto data=LoadResource(GetModuleHandleW(nullptr),resource);return json::parse(static_cast<const char*>(LockResource(data)),static_cast<const char*>(LockResource(data))+SizeofResource(GetModuleHandleW(nullptr),resource));}();
    return {{"version",1},{"groups",json::array({presets["group"],{{"id","sys-personal"},{"name","Свои программы"},{"parentId","sys-prog"},{"position",7}}})},
        {"apps",presets["apps"]},{"launcher",{{"mode","hold"},{"trigger",{{"type","keyboard"},{"key",VK_OEM_3},{"modifiers",2},{"label","`"}}}}}};
}
#include "config-integrity.h"
void validate(json& data) {
    validateEntries(data);
    // Built-in commands always come from the executable, including when JSON was edited by hand.
    auto builtin=defaults();
    const std::map<std::string,std::string> legacy={{"sys-settings","sett"},{"sys-cmd","cmd"},{"sys-regedit","regedit"},{"sys-disks","disk"},{"sys-devices","Устр"},{"sys-control","управ"}};
    std::map<std::string,int> positions;for(auto key:{"groups","apps"})for(const auto& item:data[key])positions[item.at("id").get<std::string>()]=item.at("position").get<int>();
    std::map<std::string,bool> labelOptions;for(auto key:{"groups","apps"})for(const auto& item:data[key])if(item.contains("noIcon"))labelOptions[item["id"].get<std::string>()]=item["noIcon"].get<bool>();
    for(auto key:{"groups","apps"})for(auto& item:builtin[key])if(labelOptions.count(item["id"].get<std::string>()))item["noIcon"]=labelOptions.at(item["id"].get<std::string>());
    std::map<std::string,std::string> names;
    for(auto key:{"groups","apps"})for(auto& item:data[key]){const auto id=item.at("id").get<std::string>();const auto name=item.at("name").get<std::string>();names[id]=name;item["name"]=compactName(name);}
    if(names.count("sys-prog"))builtin["groups"][0]["name"]=compactName(names["sys-prog"]);
    for(auto& item:builtin["apps"]){const auto id=item["id"].get<std::string>();if(names.count(id)&&(!legacy.count(id)||names[id]!=legacy.at(id))&&names[id]!=compactName(item["name"].get<std::string>(),12))item["name"]=compactName(names[id]);}
    for(auto& item:builtin["groups"])item["name"]=compactName(item["name"].get<std::string>());
    for(auto key:{"groups","apps"})for(auto& item:builtin[key])if(positions.count(item["id"].get<std::string>()))item["position"]=positions.at(item["id"].get<std::string>());
    const auto fixed=[&](const json& item){
        if(item.value("id",std::string{})=="sys-prog")return true;
        for(const auto& app:builtin["apps"])if(app["id"]==item["id"])return true;
        return false;
    };
    for(auto key:{"groups","apps"}) {
        auto& array=data[key]; array.erase(std::remove_if(array.begin(),array.end(),fixed),array.end());
    }
    data["groups"].push_back(builtin["groups"][0]);
    for(const auto& app:builtin["apps"])data["apps"].push_back(app);
    std::map<std::string,std::set<int>> slots;
    std::map<std::string,std::string> parents;
    std::set<std::string> ids;
    int personalCount=0;
    for(const auto& g:data["groups"]) {
        std::string id=g.at("id").get<std::string>();
        if(!ids.insert(id).second)throw std::runtime_error("Duplicate ID");
        parents[id]=g.at("parentId").is_null()?"":g.at("parentId").get<std::string>();
    }
    for(const auto& [id,parent]:parents) {
        std::set<std::string> ancestry;std::string next=id;while(!next.empty()){if(!parents.count(next)||!ancestry.insert(next).second)throw std::runtime_error("Invalid group hierarchy");next=parents[next];}
        if(parent=="sys-prog"&&++personalCount>1)throw std::runtime_error("Only one personal group allowed");
    }
    for(auto key:{"groups","apps"})for(const auto& item:data[key]) {
        int pos=item.at("position").get<int>();
        auto name=wide(item.at("name").get<std::string>());
        if(name.empty()||pos<1||pos>13)throw std::runtime_error("Invalid position or name");
        std::string parent;
        if(std::string(key)=="groups")parent=parents.at(item.at("id").get<std::string>());
        else {
            if(!ids.insert(item.at("id").get<std::string>()).second)throw std::runtime_error("Duplicate ID");
            parent=item.at("groupId").get<std::string>();
            if(!parents.count(parent)||wide(item.at("path").get<std::string>()).empty())throw std::runtime_error("Invalid application");
            if(parent=="sys-prog"&&!fixed(item))throw std::runtime_error("Custom apps must be in personal group");
        }
        if(!slots[parent].insert(pos).second)throw std::runtime_error("Duplicate position");
        int depth=0;auto ancestor=parent;while(!ancestor.empty()){++depth;ancestor=parents.at(ancestor);}
        if(pos>radial::capacity(depth))throw std::runtime_error("Ring capacity exceeded");
    }
    data.erase("explorer");
    if(data.contains("layout")) {
        if(!data["layout"].is_object()||!data["layout"].contains("nodes")||!data["layout"]["nodes"].is_object())throw std::runtime_error("Invalid layout");
        auto& coordinates=data["layout"]["nodes"];
        for(auto it=coordinates.begin();it!=coordinates.end();) {
            if(tree::positionFixed(it.key())||!ids.count(it.key())){it=coordinates.erase(it);continue;}
            for(auto axis:{"x","y"}){double value=it.value().at(axis).get<double>();if(!std::isfinite(value)||std::abs(value)>2400)throw std::runtime_error("Invalid coordinates");}
            ++it;
        }
    }
    if(data.contains("layout")&&data["layout"].contains("branches")) {
        auto& branches=data["layout"]["branches"];if(!branches.is_object())throw std::runtime_error("Invalid branches");
        for(auto it=branches.begin();it!=branches.end();) {
            if(it.key()!="@root"&&!parents.count(it.key())){it=branches.erase(it);continue;}
            if(!it.value().is_object())throw std::runtime_error("Invalid branch");
            for(const auto& [key,min,max]:std::vector<std::tuple<std::string,double,double>>{{"length",0,800},{"radius",40,800},{"angle",-PI,PI}})if(it.value().contains(key)){double value=it.value()[key].get<double>();if(!std::isfinite(value)||value<min||value>max)throw std::runtime_error("Invalid branch geometry");}
            ++it;
        }
    }
    if(data.contains("layout")&&data["layout"].contains("rootLinks")){
        auto& links=data["layout"]["rootLinks"];if(!links.is_array())throw std::runtime_error("Invalid root links");
        for(const auto& pair:links)if(!pair.is_array()||pair.size()!=2||!pair[0].is_string()||!pair[1].is_string()||pair[0]==pair[1])throw std::runtime_error("Invalid root link");
        json valid=json::array();std::set<std::pair<std::string,std::string>> unique;for(const auto& pair:links){const auto a=pair[0].get<std::string>(),b=pair[1].get<std::string>();if(parents.count(a)&&parents.count(b)&&parents[a].empty()&&parents[b].empty()&&unique.insert({std::min(a,b),std::max(a,b)}).second)valid.push_back(pair);}links=std::move(valid);
    }
    if(data.contains("appearance")) {
        const auto& settings=data["appearance"];if(!settings.is_object())throw std::runtime_error("Invalid menu size");
        for(const auto& range:std::vector<std::tuple<std::string,double,double>>{{"web",50,180},{"radial",50,180},{"ringStep",0,12},{"openMs",100,1500}}){const auto& [key,min,max]=range;if(settings.contains(key)){const double value=settings[key].get<double>();if(!std::isfinite(value)||value<min||value>max)throw std::runtime_error("Invalid menu size");}}
    }
    if(data.contains("rgb")) {
        const auto& rgb=data["rgb"];if(!rgb.is_object())throw std::runtime_error("Invalid RGB settings");
        for(const auto* key:{"radial","blocks","branches"})if(rgb.contains(key)&&!rgb[key].is_boolean())throw std::runtime_error("Invalid RGB switch");
        if(rgb.contains("speed")){const double speed=rgb["speed"].get<double>();if(!std::isfinite(speed)||speed<.2||speed>3)throw std::runtime_error("Invalid RGB speed");}
    }
    if(data.contains("webStyle")) {
        const auto& style=data["webStyle"];if(!style.is_object())throw std::runtime_error("Invalid web style");
        for(const auto& setting:std::vector<std::pair<std::string,double>>{{"opacity",100},{"blur",24}})if(style.contains(setting.first)){const double value=style[setting.first].get<double>();if(!std::isfinite(value)||value<0||value>setting.second)throw std::runtime_error("Invalid web material setting");}
        if(style.contains("color")){const auto color=style["color"].get<std::string>();if(color.size()!=7||color[0]!='#'||!std::all_of(color.begin()+1,color.end(),[](unsigned char c){return std::isxdigit(c)!=0;}))throw std::runtime_error("Invalid web color");}
    }
    if(data.contains("radial")) {
        const auto& style=data["radial"];if(!style.is_object())throw std::runtime_error("Invalid ring style");
        if(style.contains("rounded")&&!style["rounded"].is_boolean())throw std::runtime_error("Invalid rounding mode");
        for(const auto& setting:std::vector<std::pair<std::string,double>>{{"roundness",18},{"blur",24}})if(style.contains(setting.first)){const double value=style[setting.first].get<double>();if(!std::isfinite(value)||value<0||value>setting.second)throw std::runtime_error("Invalid material setting");}
        const auto validColor=[](const json& value){if(!value.is_string())return false;const auto s=value.get<std::string>();return s.size()==7&&s[0]=='#'&&std::all_of(s.begin()+1,s.end(),[](unsigned char c){return std::isxdigit(c)!=0;});};
        if(style.contains("color")&&!validColor(style["color"]))throw std::runtime_error("Invalid ring color");
        if(style.contains("colors")){if(!style["colors"].is_object())throw std::runtime_error("Invalid sector colors");for(const auto& color:style["colors"])if(!validColor(color))throw std::runtime_error("Invalid sector color");}
        if(style.contains("rotation")){if(!style["rotation"].is_object())throw std::runtime_error("Invalid ring rotation");for(const auto& angle:style["rotation"]){const auto value=angle.get<double>();if(!std::isfinite(value)||std::abs(value)>PI)throw std::runtime_error("Invalid ring rotation");}}
    }
    const auto launcher=data.value("launcher",builtin["launcher"]);
    for(const auto& range:std::vector<std::tuple<std::string,double,double>>{{"hoverDelayMs",0,1500},{"closeDelayMs",0,2000},{"corridorWidth",8,120},{"branchLength",0,800}}) {
        const auto& [key,min,max]=range;if(launcher.contains(key)){double value=launcher[key].get<double>();if(!std::isfinite(value)||value<min||value>max)throw std::runtime_error("Invalid interaction setting");}
    }
    if(launcher.contains("monitor")&&(!launcher["monitor"].is_string()||launcher["monitor"].get<std::string>().empty()||launcher["monitor"].get<std::string>().size()>128))throw std::runtime_error("Invalid monitor");
    if(launcher.contains("view")&&launcher["view"]!="radial"&&launcher["view"]!="web")throw std::runtime_error("Invalid launcher view");
    const auto mode=launcher.value("mode",std::string("hold"));
    if(mode!="hold"&&mode!="toggle")throw std::runtime_error("Invalid mode");
    const auto trigger=launcher.value("trigger",builtin["launcher"]["trigger"]);
    const auto type=trigger.value("type",std::string("mouse"));
    if(type=="keyboard") {
        int key=trigger.at("key").get<int>(),mods=trigger.value("modifiers",0);
        if(key<8||key>254||key==VK_ESCAPE||(key>=VK_SHIFT&&key<=VK_MENU)||key==VK_LWIN||key==VK_RWIN||mods<0||mods>7)throw std::runtime_error("Invalid keyboard bind");
    } else if(type!="mouse"||(trigger.value("button",std::string{})!="middle"&&trigger["button"]!="x1"&&trigger["button"]!="x2"))throw std::runtime_error("Invalid mouse bind");
}
bool loadConfiguration(bool report=true) {
    configDirty=false;const auto previousError=configError;
    try {
        json next;
        if(fs::exists(configPath)) { std::ifstream file(configPath);file>>next; }
        else {next=defaults();next["apps"].push_back({{"id","personal-taskmgr"},{"name","Диспетчер задач"},{"groupId","sys-personal"},{"path","taskmgr.exe"},{"position",1}});}
        validate(next);
        const auto launcher=next.value("launcher",defaults()["launcher"]);
        const bool nextHold=launcher.value("mode",std::string("hold"))=="hold";
        const auto trigger=launcher.value("trigger",defaults()["launcher"]["trigger"]);
        bool keyboard=trigger.value("type",std::string("mouse"))=="keyboard";
        const int nextModifiers=keyboard?trigger.value("modifiers",0):0;
        const auto button=trigger.value("button",std::string("middle"));
        const int nextKey=keyboard?trigger.at("key").get<int>():button=="x1"?VK_XBUTTON1:button=="x2"?VK_XBUTTON2:VK_MBUTTON;
        configuration=std::move(next);holdMode=nextHold;triggerModifiers=nextModifiers;triggerKey=nextKey;keyboardTrigger=keyboard;refreshInvocationMonitor();syncHookState();
        configError.clear();return true;
    }catch(const std::exception&) {
        configError=L"Не удалось прочитать конфигурацию. Проверьте группы, позиции и бинд в настройках.";
        if(report&&previousError!=configError)notify(configError);
        return false;
    }
}
#include "monitors.h"
#include "icons.h"
#include "runtime.h"
#include "autostart.h"
#include "app-name.h"
void openSettings() {
    hide();
    fs::path settings=exeDirectory/L"FoldBOcean Settings.exe";
    if(!fs::exists(settings))settings=exeDirectory/L"spiderweb-settings.exe";
    if(!fs::exists(settings)){notify(L"Окно настроек не найдено. Поместите FoldBOcean Settings.exe рядом с FoldBOcean.exe.");return;}
    ShellExecuteW(nullptr,L"open",settings.c_str(),nullptr,exeDirectory.c_str(),SW_SHOWNORMAL);
}
void addTray() {
    trayData={};trayData.cbSize=sizeof(trayData);trayData.hWnd=windowHandle;trayData.uID=1;
    trayData.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;trayData.uCallbackMessage=TRAY;
    trayData.hIcon=LoadIconW(instanceHandle,MAKEINTRESOURCEW(IDI_SPIDERWEB));
    wcscpy_s(trayData.szTip,L"FoldBOcean — круговая панель");Shell_NotifyIconW(NIM_ADD,&trayData);
}
void closeSettings() {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot!=INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W process{sizeof(process)};
        if(Process32FirstW(snapshot,&process))do {
            if(_wcsicmp(process.szExeFile,L"FoldBOcean Settings.exe")!=0&&_wcsicmp(process.szExeFile,L"SpiderWeb Settings.exe")!=0&&_wcsicmp(process.szExeFile,L"spiderweb-settings.exe")!=0)continue;
            auto name=L"Local\\SpiderWeb.Settings.Exit."+std::to_wstring(process.th32ProcessID);
            HANDLE signal=OpenEventW(EVENT_MODIFY_STATE,FALSE,name.c_str());
            if(signal){SetEvent(signal);CloseHandle(signal);}
        }while(Process32NextW(snapshot,&process));
        CloseHandle(snapshot);
    }

}
void trayMenu() {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,1,L"Открыть панель (демо)");AppendMenuW(menu,MF_STRING,2,L"Настройки…");
    AppendMenuW(menu,MF_STRING|(paused?MF_CHECKED:0),3,L"Приостановить вызов");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,4,L"Выход");AppendMenuW(menu,MF_STRING,5,L"Завершить всё");
    POINT point{};GetCursorPos(&point);SetForegroundWindow(windowHandle);
    UINT command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,windowHandle,nullptr);DestroyMenu(menu);PostMessageW(windowHandle,WM_NULL,0,0);
    if(command==1)show(true);if(command==2)openSettings();if(command==3){paused=!paused;hide();}
    if(command==4)DestroyWindow(windowHandle);
    if(command==5){closeSettings();DestroyWindow(windowHandle);}
}
LRESULT dispatchWindow(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    if(message==taskbarCreated&&taskbarCreated){addTray();return 0;}
    switch(message) {
    case TRAY:if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU)trayMenu();else if(lp==WM_LBUTTONDBLCLK)openSettings();return 0;
    case DEMO:show(true);return 0;
    case QUIT:DestroyWindow(hwnd);return 0;
    case QUIT_ALL:closeSettings();DestroyWindow(hwnd);return 0;
    case HOOK_ERROR:notify(L"Не удалось подключить глобальный бинд. Перезапустите FoldBOcean.");return 0;
    case WM_DISPLAYCHANGE:refreshInvocationMonitor();return 0;
    case BIND_DOWN:
        if(paused)return 0;
        bindHeld=true;
        if(visible){if(!holdMode||demoMode)hide();}
        else show();return 0;
    case BIND_UP:
        bindHeld=false;KillTimer(hwnd,1);
        if(visible&&holdMode&&!demoMode){POINT p{};GetCursorPos(&p);int target=hit(p);if(target>=0&&!entries[target].group)selectItem(target);else hide();}return 0;
    case POINTER_MOVE:pointerMoved();return 0;
    case ICON_READY:if(applyIconResults()&&visible)render();return 0;
    case OUTSIDE:if(visible&&!demoMode){POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};if(!menuInside(worldPoint(point),3))hide();}return 0;
    case KEY_ACTION:
        if(!visible)return 0;
        if(wp==VK_ESCAPE)hide();else if(wp==VK_BACK)back();else if(wp>='1'&&wp<='7'){for(size_t i=0;i<entries.size();i++)if(scene.nodes[i].parent==(navigation.empty()?std::string{}:navigation.back())&&entries[i].position==static_cast<int>(wp-'0')){selectItem(static_cast<int>(i));break;}}return 0;
    case RELOAD:SetTimer(hwnd,2,100,nullptr);return 0;
    case WM_TIMER:
        if(wp==6){POINT p{};GetCursorPos(&p);if(hit(p)==tooltipTarget)revealTooltip();else clearTooltips();}
        if(wp==5){KillTimer(hwnd,5);if(!visible){cancelPendingIcons();iconCache.clear();}}
        if(wp==1){KillTimer(hwnd,1);POINT p{};GetCursorPos(&p);int target=hit(p);if(visible&&target>=0&&entries[target].group)enterGroup(target);}
        if(wp==3)collapseExpired();
        if(wp==4)advanceAnimation();
        if(wp==2){KillTimer(hwnd,2);auto previous=configuration;if(loadConfiguration()&&visible&&configuration!=previous)hide();}return 0;
    case WM_LBUTTONUP: {POINT p{};GetCursorPos(&p);selectItem(hit(p));return 0;}
    case WM_RBUTTONUP:back();return 0;
    case WM_NCHITTEST: {POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};return menuInside(worldPoint(point),3)?HTCLIENT:HTTRANSPARENT;}
    case WM_CLOSE:hide();return 0;
    case WM_DESTROY:
        visible=false;SetEvent(stopEvent);cancelPendingIcons();if(hookThreadId)PostThreadMessageW(hookThreadId,WM_QUIT,0,0);
        Shell_NotifyIconW(NIM_DELETE,&trayData);clearTooltips();if(tooltipWindow){DestroyWindow(tooltipWindow);tooltipWindow=nullptr;}iconCache.clear();blurredBackground.reset();renderSurface.reset();arcLabels.clear();arcRasters.clear();PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) noexcept {
    try{return dispatchWindow(hwnd,message,wp,lp);}
    catch(...){
        // Never unwind through user32. Preserve the tray process and last good JSON.
        try{hide();notify(L"Не удалось показать панель. Проверьте конфигурацию в настройках.");}
        catch(...){visible=false;ShowWindow(hwnd,SW_HIDE);for(int timer:{1,3,4})KillTimer(hwnd,timer);}
        return 0;
    }
}
bool hookHeld=false;
std::array<bool,256> swallowedKeys{};
int heldKey=0;
int passthroughKey=0;
LRESULT CALLBACK mouseHook(int code,WPARAM wp,LPARAM lp) {
    if(code==HC_ACTION) {
        auto* data=reinterpret_cast<MSLLHOOKSTRUCT*>(lp);
        bool heldMouseUp=(heldKey==VK_MBUTTON&&wp==WM_MBUTTONUP)||((heldKey==VK_XBUTTON1||heldKey==VK_XBUTTON2)&&wp==WM_XBUTTONUP&&HIWORD(data->mouseData)==(heldKey==VK_XBUTTON1?XBUTTON1:XBUTTON2));
        if(heldMouseUp&&hookHeld){hookHeld=false;PostMessageW(windowHandle,BIND_UP,0,0);syncHookState();return 1;}
        if(!keyboardTrigger) {
            int key=triggerKey;
            bool down=(key==VK_MBUTTON&&wp==WM_MBUTTONDOWN)||((key==VK_XBUTTON1||key==VK_XBUTTON2)&&wp==WM_XBUTTONDOWN&&HIWORD(data->mouseData)==(key==VK_XBUTTON1?XBUTTON1:XBUTTON2));
            if(down&&!paused){if(invocationPassThrough(fullscreenBlocksInvocation(),hookHeld))return CallNextHookEx(nullptr,code,wp,lp);if(!hookHeld){hookHeld=true;heldKey=key;PostMessageW(windowHandle,BIND_DOWN,0,0);}return 1;}
        }
        if(visible&&wp==WM_MOUSEMOVE&&!movePending.exchange(true))PostMessageW(windowHandle,POINTER_MOVE,0,0);
        if(visible&&wp==WM_LBUTTONDOWN)PostMessageW(windowHandle,OUTSIDE,0,MAKELPARAM(data->pt.x,data->pt.y));
    }
    return CallNextHookEx(nullptr,code,wp,lp);
}
LRESULT CALLBACK keyboardHook(int code,WPARAM wp,LPARAM lp) {
    if(code==HC_ACTION) {
        auto* data=reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);int key=static_cast<int>(data->vkCode);
        bool down=wp==WM_KEYDOWN||wp==WM_SYSKEYDOWN, up=wp==WM_KEYUP||wp==WM_SYSKEYUP;
        if(key==passthroughKey){if(up)passthroughKey=0;return CallNextHookEx(nullptr,code,wp,lp);}
        if(up&&hookHeld&&heldKey==key&&heldKey>VK_XBUTTON2){hookHeld=false;PostMessageW(windowHandle,BIND_UP,0,0);return 1;}
        if(up&&key>=0&&key<256&&swallowedKeys[key]){swallowedKeys[key]=false;return 1;}
        if(keyboardTrigger&&key==triggerKey) {
            int mods=((GetAsyncKeyState(VK_CONTROL)&0x8000)?1:0)|((GetAsyncKeyState(VK_MENU)&0x8000)?2:0)|((GetAsyncKeyState(VK_SHIFT)&0x8000)?4:0);
            if(down&&!paused&&(hookHeld||mods==triggerModifiers)){if(invocationPassThrough(fullscreenBlocksInvocation(),hookHeld)){passthroughKey=key;return CallNextHookEx(nullptr,code,wp,lp);}if(!hookHeld){hookHeld=true;heldKey=key;PostMessageW(windowHandle,BIND_DOWN,0,0);}return 1;}
        }
        if(visible&&GetForegroundWindow()==windowHandle&&(key==VK_ESCAPE||key==VK_BACK||(key>='1'&&key<='7'))) {
            if(down){if(!swallowedKeys[key])PostMessageW(windowHandle,KEY_ACTION,key,0);swallowedKeys[key]=true;return 1;}
        }
    }
    return CallNextHookEx(nullptr,code,wp,lp);
}
void hooks() {
    hookThreadId=GetCurrentThreadId();MSG msg{};PeekMessageW(&msg,nullptr,WM_USER,WM_USER,PM_NOREMOVE);
    HHOOK mouse{};
    HHOOK keyboard=SetWindowsHookExW(WH_KEYBOARD_LL,keyboardHook,instanceHandle,0);
    // A keyboard-bound hidden panel does not need every desktop mouse event.
    // Keep the hook for mouse binds and a held mouse button until its release.
    const auto refreshMouse=[&]{const bool needed=!keyboardTrigger||visible||(hookHeld&&heldKey<=VK_XBUTTON2);if(needed&&!mouse){mouse=SetWindowsHookExW(WH_MOUSE_LL,mouseHook,instanceHandle,0);if(!mouse)PostMessageW(windowHandle,HOOK_ERROR,0,0);}else if(!needed&&mouse){UnhookWindowsHookEx(mouse);mouse=nullptr;}};
    refreshMouse();if(!keyboard)PostMessageW(windowHandle,HOOK_ERROR,0,0);
    if(WaitForSingleObject(stopEvent,0)!=WAIT_OBJECT_0)while(GetMessageW(&msg,nullptr,0,0)>0){if(msg.message==SYNC_HOOKS)refreshMouse();else{TranslateMessage(&msg);DispatchMessageW(&msg);}}
    if(mouse)UnhookWindowsHookEx(mouse);if(keyboard)UnhookWindowsHookEx(keyboard);
    hookThreadId=0;
}
void watchConfig() {
    HANDLE changed=FindFirstChangeNotificationW(configPath.parent_path().c_str(),FALSE,FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE);
    if(changed==INVALID_HANDLE_VALUE)return;
    HANDLE waits[]{stopEvent,changed};
    while(WaitForMultipleObjects(2,waits,FALSE,INFINITE)==WAIT_OBJECT_0+1){configDirty=true;PostMessageW(windowHandle,RELOAD,0,0);if(!FindNextChangeNotification(changed))break;}
    FindCloseChangeNotification(changed);
}
#include "fullscreen-tests.h"
#include "block-label-tests.h"
int runTests(){
    json tests=json::array();bool passed=true;auto require=[](bool value){if(!value)throw std::runtime_error("Assertion failed");};
    const auto test=[&](const char* name,auto body){tests.push_back({{"test",name},{"running",true}});std::ofstream(exeDirectory/L"native-tests.json")<<tests.dump(2);try{body();tests.back()={{"test",name},{"passed",true}};}catch(const std::exception& e){passed=false;tests.back()={{"test",name},{"passed",false},{"error",e.what()}};}std::ofstream(exeDirectory/L"native-tests.json")<<tests.dump(2);};
    runFullscreenTests(test,require);
    runBlockLabelTests(test,require);
    test("shared configuration rejection fixtures",[&]{
        std::ifstream file(exeDirectory.parent_path()/L"tests"/L"config-cases.json");json cases;file>>cases;
        for(const auto& fixture:cases){auto data=fixture["config"];bool accepted=true;try{validate(data);}catch(...){accepted=false;}if(accepted!=fixture["valid"].get<bool>())throw std::runtime_error(fixture["name"].get<std::string>());}
    });
    test("CLI options require whole tokens and exact arity",[&]{
        cli::Arguments arguments;arguments.values={L"--quit-all"};require(arguments.valid()&&!arguments.is(L"--quit"));arguments.values={L"prefix--demo"};require(!arguments.valid());arguments.values={L"--app-name",L"C:\\Apps\\file --quit.exe"};require(arguments.valid()&&!arguments.is(L"--quit"));arguments.values={L"--demo",L"extra"};require(!arguments.valid());
    });
    test("icon subprocess returns alpha pixels and honors cancellation",[&]{
        const IconRequest request{"@folder",L"",true,iconGeneration.load()};const auto pixels=fetchIcon(request);require(pixels.size()==48*48*4);bool alpha=false;for(size_t i=3;i<pixels.size();i+=4)alpha|=pixels[i]>0;require(alpha);++iconGeneration;require(fetchIcon(request).empty());
    });
    test("invalid config and malformed UTF-8 preserve the last good state",[&]{
        const auto savedPath=configPath;const auto file=exeDirectory/(L"config-test-"+std::to_wstring(GetCurrentProcessId())+L".json");
        struct Cleanup{fs::path old,file;~Cleanup(){configPath=old;std::error_code error;fs::remove(file,error);}}cleanup{savedPath,file};
        configPath=file;auto good=defaults();std::ofstream(file)<<good.dump();require(loadConfiguration(false));const auto saved=configuration;const auto savedKey=triggerKey.load();
        auto bad=good;bad["apps"][0]["path"]=42;std::ofstream(file)<<bad.dump();require(!loadConfiguration(false)&&configuration==saved&&triggerKey==savedKey);
        std::ofstream(file,std::ios::binary)<<"{\"version\":1,\"name\":\""<<'\xff'<<"\"}";require(!loadConfiguration(false)&&configuration==saved);
        std::ofstream(file)<<good.dump();require(loadConfiguration(false)&&configError.empty());
    });
    test("rounded connections export current native geometry",[&]{
        tree::Frame frame;frame.points={{0,0},{220,180}};frame.edges={{{0,0},{120,0}},{{120,0},{220,180}}};const auto paths=tree::roundedConnections(frame);require(paths.size()==2&&paths[1].curved);
        json result=json::array();const auto point=[](tree::Point p){return json{{"x",p.x},{"y",p.y}};};
        for(const auto& path:paths)result.push_back({{"start",point(path.start)},{"end",point(path.end)},{"join",point(path.join)},{"c1",point(path.c1)},{"c2",point(path.c2)},{"curved",path.curved}});
        std::ofstream(exeDirectory/L"rounded-connections.json")<<result.dump(2);
    });
    test("shared layered surface preserves alpha, reuses pixels and releases GDI handles",[&]{
        const DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);LayeredSurface surface;
        for(int i=0;i<40;++i){require(surface.ensure(96+i%2,64));auto* image=surface.image();require(surface.ensure(96+i%2,64)&&surface.image()==image);{Graphics g(image);g.Clear(Color(0,0,0,0));SolidBrush brush(Color(128,80,120,200));g.FillRectangle(&brush,10,10,20,20);}Color pixel;image->GetPixel(15,15,&pixel);require(pixel.GetA()==128&&std::abs(int(pixel.GetR())-80)<=1&&std::abs(int(pixel.GetB())-200)<=1);image->GetPixel(0,0,&pixel);require(pixel.GetA()==0);surface.reset();require(!surface.image());}
        require(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==before);
    });
    test("radial reserve fits reachable rings and packed opening overshoot",[&]{
        auto config=defaults();require(radial::maximumPanelDepth(config)==2);std::string parent="sys-personal";for(int i=0;i<8;++i){const auto id="reserve-"+std::to_string(i);config["groups"].push_back({{"id",id},{"name","Test"},{"parentId",parent},{"position",1}});parent=id;}require(radial::maximumPanelDepth(config)==4);
        for(float step:{0.f,6.f,12.f})for(int count=5;count<=12;++count){const int depth=count-1;const float outer=radial::radius(depth,step)+motion::compactShift(depth,count,step)+radial::thickness(std::max(1,count-4),step)+10;require(outer<=radial::radius(4,step)+.01f);}
    });
    test("allocation-free ring hit testing matches animated geometry",[&]{
        const auto graph=radial::makeScene(defaults(),{"sys-prog"});for(const auto& sector:graph.sectors)for(float zoom:{.5f,.94f,1.f})for(float shift:{-20.f,0.f,30.f}){radial::Scene frame;frame.sectors.push_back(motion::scaledRing(sector,zoom,shift));for(int x=-250;x<=250;x+=17)for(int y=-250;y<=250;y+=19)require(radial::hitSector(sector,{float(x),float(y)},zoom,shift)==(radial::hit(frame,{float(x),float(y)})>=0));}
    });
    test("RGB label rasters keep glyph coverage at different display scales",[&]{
        struct Reset{~Reset(){animationTimerInterval=0;arcLabels.clear();arcRasters.clear();scale=1;}}reset;
        const auto graph=radial::makeScene(defaults(),{"sys-prog"});
        for(float zoom:{1.f,1.2f,1.5f}){scale=zoom;arcLabels.clear();arcRasters.clear();
            Bitmap raw(720,720,PixelFormat32bppPARGB),cached(720,720,PixelFormat32bppPARGB);
            const auto paint=[&](Bitmap& bitmap,bool useCache){Graphics g(&bitmap);g.Clear(Color(0,0,0,0));Matrix transform(zoom,0,0,zoom,360,360);g.SetTransform(&transform);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);g.SetInterpolationMode(InterpolationModeHighQualityBicubic);for(const auto& sector:graph.sectors){const auto name=wide(sector.item["name"].get<std::string>());if(useCache)drawArcLabel(g,sector,name,1);else drawArcLabelRaw(g,sector,name,1);}};
            paint(raw,false);animationTimerInterval=33;paint(cached,true);require(arcRasters.size()==graph.sectors.size());
            double expected=0,actual=0;for(int y=0;y<720;++y)for(int x=0;x<720;++x){Color a,b;raw.GetPixel(x,y,&a);cached.GetPixel(x,y,&b);expected+=a.GetA();actual+=b.GetA();}require(expected>0&&std::abs(actual-expected)/expected<.08);
        }
    });
    test("root folders have direct links without an intermediate center",[&]{for(int count=1;count<=5;++count){auto config=defaults();for(int i=1;i<count;++i)config["groups"].push_back({{"id","root-"+std::to_string(i)},{"name","Root"},{"parentId",nullptr},{"position",i+1}});validate(config);const std::set<std::string> closed;const auto graph=tree::makeScene(config,&closed);require(graph.nodes.size()==static_cast<size_t>(count));require(graph.edges.size()==static_cast<size_t>(count*(count-1)/2));for(const auto& edge:graph.edges){require(std::any_of(graph.nodes.begin(),graph.nodes.end(),[&](const auto& n){return tree::samePoint(n.point,edge.a);}));require(std::any_of(graph.nodes.begin(),graph.nodes.end(),[&](const auto& n){return tree::samePoint(n.point,edge.b);}));}}});
    test("custom root links are editable and repair after root deletion",[&]{auto config=defaults();for(int i=1;i<=3;++i)config["groups"].push_back({{"id","root-"+std::to_string(i)},{"name","Root"},{"parentId",nullptr},{"position",i+1}});config["layout"]={{"nodes",json::object()},{"branches",json::object()},{"rootLinks",json::array({json::array({"sys-prog","root-3"}),json::array({"root-3","root-1"}),json::array({"root-1","root-2"})})}};validate(config);const auto links=tree::rootLinks(config);require(links.size()==3&&links[0].second=="root-3");const std::set<std::string> closed;const auto graph=tree::makeScene(config,&closed);json nodes=json::array(),edges=json::array();for(const auto& n:graph.nodes)nodes.push_back({{"id",n.item["id"]},{"x",n.point.x},{"y",n.point.y}});for(const auto& edge:graph.edges)edges.push_back({{"a",{{"x",edge.a.x},{"y",edge.a.y}}},{"b",{{"x",edge.b.x},{"y",edge.b.y}}}});std::ofstream(exeDirectory/L"root-links-geometry.json")<<json({{"config",config},{"nodes",nodes},{"edges",edges}}).dump(2);auto& groups=config["groups"];groups.erase(std::remove_if(groups.begin(),groups.end(),[](const auto& g){return g["id"]=="root-1";}),groups.end());validate(config);const auto repaired=tree::rootLinks(config);require(repaired.size()==2);for(const auto& pair:repaired)require(pair.first!="root-1"&&pair.second!="root-1");});
    test("web view renders, navigates and switches back without changing commands",[&]{configuration=defaults();configuration["launcher"]["view"]="web";validate(configuration);navigation.clear();expanded.clear();reveals.clear();updateEntries();require(webView()&&entries.size()==1);enterGroup(0);require(entries.size()==8&&navigation.back()=="sys-prog");reveals.clear();worldMinX=worldMinY=-650;scale=1;canvasWidth=canvasHeight=1300;Bitmap bitmap(1300,1300,PixelFormat32bppPARGB);{Graphics g(&bitmap);paintScene(g);}Color pixel;bitmap.GetPixel(static_cast<int>(scene.nodes[0].point.x-worldMinX+40),static_cast<int>(scene.nodes[0].point.y-worldMinY),&pixel);require(pixel.GetA()>240);back();require(closingBranch("sys-prog")&&entries.size()==8);finishCollapseGroup("sys-prog");require(entries.size()==1&&navigation.empty());configuration["launcher"]["view"]="radial";updateEntries();require(!webView()&&rings.panels.size()==1);});
    test("opening motion settles and only animates new descendants",[&]{auto config=defaults();const auto graph=tree::makeScene(config);const tree::Reveal reveal{"sys-prog",graph.nodes[0].point,1000,1};const auto start=tree::animationFrame(graph,{reveal},1000),middle=tree::animationFrame(graph,{reveal},1225),end=tree::animationFrame(graph,{reveal},1450);require(tree::samePoint(start.points[0],graph.nodes[0].point)&&start.nodeAlpha[0]==1);bool child=false;for(size_t i=1;i<graph.nodes.size();++i)if(tree::contains(graph.nodes[i].chain,"sys-prog")){child=true;require(start.nodeAlpha[i]==0&&!start.interactive[i]);require(middle.nodeAlpha[i]>0&&middle.nodeAlpha[i]<1);require(end.nodeAlpha[i]==1&&end.interactive[i]&&tree::samePoint(end.points[i],graph.nodes[i].point));}require(child);const auto ring=radial::makeScene(config,{"sys-prog","sys-personal"}).sectors.back();const auto early=motion::ringFrame(ring,motion::progress(1000,1000)),late=motion::ringFrame(ring,motion::progress(1000,1450));require(early.radius<ring.radius&&early.inner<ring.inner&&late.radius==ring.radius&&late.inner==ring.inner);require(motion::progress(1000,1000)==0&&motion::progress(1000,1450)==1&&motion::progress(1000,999)==0);});
    test("old rings sink at five and reverse continuously at six",[&]{require(!motion::sunk(0,6)&&!motion::sunk(1,4)&&motion::sunk(1,5)&&!motion::sunk(2,5)&&motion::sunk(2,6)&&!motion::sunk(3,6));motion::RingTransition state;ULONGLONG started=0;require(motion::sample(state,started,450).alpha==1);require(motion::retarget(state,started,1000,true,.6f)&&state.duration==motion::SINK_MS);const auto initial=motion::sample(state,started,1000),middle=motion::sample(state,started,1200);require(initial.alpha==1&&middle.alpha>.4f&&middle.alpha<.6f&&middle.zoom<1);require(motion::retarget(state,started,1200,false,.6f)&&state.duration==motion::RETURN_MS);const auto reversed=motion::sample(state,started,1200);require(reversed.alpha==middle.alpha&&reversed.zoom==middle.zoom);const auto restored=motion::sample(state,started,1550);require(restored.alpha==1&&restored.zoom==1);require(!motion::retarget(state,started,1600,false,.6f));});
    test("web closing fades before removal and resumes from current opacity",[&]{auto config=defaults();const auto graph=tree::makeScene(config);const tree::Reveal closing{"sys-prog",graph.nodes[0].point,1000,1,true,1};const auto beginning=tree::animationFrame(graph,{closing},1000),middle=tree::animationFrame(graph,{closing},1225),end=tree::animationFrame(graph,{closing},1450);require(beginning.nodeAlpha[0]==1&&middle.nodeAlpha[0]==1);for(size_t i=1;i<graph.nodes.size();++i)if(tree::contains(graph.nodes[i].chain,"sys-prog")){require(beginning.nodeAlpha[i]==1&&!beginning.interactive[i]);require(middle.nodeAlpha[i]>.4f&&middle.nodeAlpha[i]<.6f&&!middle.interactive[i]);require(end.nodeAlpha[i]==0);}const tree::Reveal resume{"sys-prog",graph.nodes[0].point,1225,1,false,tree::revealProgress(closing,1225)};require(tree::revealProgress(resume,1225)==tree::revealProgress(closing,1225)&&tree::revealProgress(resume,1675)==1);});
    test("opening duration is configurable without changing sink timing",[&]{configuration=defaults();configuration["appearance"]={{"openMs",1000}};validate(configuration);require(openingDuration()==1000);const tree::Reveal reveal{"group",{},1000,1,false,0,openingDuration()};require(tree::revealProgress(reveal,1500)==.5f&&tree::revealProgress(reveal,1450)<1&&tree::revealProgress(reveal,2000)==1);motion::RingTransition transition;transition.duration=openingDuration();require(motion::sample(transition,1000,1500).alpha==.5f);ULONGLONG started=1000;motion::retarget(transition,started,2000,true,.6f);require(transition.duration==motion::SINK_MS);configuration["appearance"]["openMs"]=99;bool rejected=false;try{validate(configuration);}catch(...){rejected=true;}require(rejected);});
    test("packed rings fill old slots and keep original thickness",[&]{for(float step:{0.f,6.f,12.f})for(size_t count=5;count<=12;++count){float outer=radial::radius(0,step);for(int d=1;d<static_cast<int>(count);++d){if(motion::sunk(d,count))continue;const auto shift=motion::compactShift(d,count,step);const float inner=radial::innerRadius(d,step)+shift,next=radial::radius(d,step)+shift;require(std::abs(inner-outer-10)<.001&&std::abs(next-inner-radial::thickness(d,step))<.001);outer=next;}}motion::RingTransition transition;ULONGLONG started=0;motion::retarget(transition,started,1000,true,.6f);motion::retarget(transition,started,1700,false,.6f,-66.4f);require(transition.duration==motion::RETURN_MS);require(motion::parentKeep(2,true)==3&&motion::parentKeep(2,false)==2);});
    test("parent hover hit testing follows compacted rings",[&]{configuration=defaults();std::string parent="sys-personal";for(int i=1;i<=3;++i){const auto id="nav-"+std::to_string(i);configuration["groups"].push_back({{"id",id},{"name","Group"},{"parentId",parent},{"position",1}});parent=id;}validate(configuration);navigation={"sys-prog","sys-personal","nav-1","nav-2"};expanded=std::set<std::string>(navigation.begin(),navigation.end());reveals.clear();updateEntries();for(auto& pair:ringOpened)pair.second=0;bool checked=false;for(const auto& sector:rings.sectors)if(sector.depth==2&&sector.item["id"]=="nav-1"){const auto frame=motion::scaledRing(sector,1,motion::compactShift(sector.depth,rings.panels.size()));require(hitRing(frame.label)>=0&&ringKeepAt(frame.label)==3);checked=true;}require(checked);require(insideRingMenu({0,200})&&!insideRingMenu({0,400}));navigation.clear();expanded.clear();updateEntries();});
    test("autostart registry roundtrip preserves quoted Unicode paths",[&]{const auto key=L"Software\\FoldBOcean.StartupTest."+std::to_wstring(GetCurrentProcessId());struct Cleanup{std::wstring key;~Cleanup(){RegDeleteKeyW(HKEY_CURRENT_USER,key.c_str());}}cleanup{key};require(!startup::read(key.c_str()));const std::wstring command=L"\"C:\\Программы\\FoldBOcean\\FoldBOcean.exe\" --background";startup::write(command,key.c_str());require(startup::read(key.c_str()).value()==command);startup::write(std::nullopt,key.c_str());require(!startup::read(key.c_str()));startup::write(std::nullopt,key.c_str());require(startup::command().find(L"\" --background")!=std::wstring::npos);});
    test("full application names come from version resources with filename fallback",[&]{wchar_t system[32768]{};require(GetSystemDirectoryW(system,32768)>0);const auto metadata=appname::read((fs::path(system)/L"cmd.exe").wstring());require(metadata["source"]!="filename"&&!metadata["name"].get<std::string>().empty());const auto fallback=appname::read(L"C:\\Missing\\Полное название программы с пробелами.exe");require(fallback["name"]=="Полное название программы с пробелами"&&fallback["source"]=="filename");auto config=defaults();config["apps"].push_back({{"id","full-name"},{"name","Полное название программы с пробелами"},{"groupId","sys-personal"},{"path","test.exe"},{"position",1}});validate(config);require(std::any_of(config["apps"].begin(),config["apps"].end(),[](const auto& app){return app["id"]=="full-name"&&app["name"]=="Полное название программы с пробелами";}));require(wide(compactName("😀😀😀😀😀😀😀😀😀😀😀😀😀",12)).size()==24);});
    test("menu ellipsis counts dots inside the character limit",[&]{require(menuLabel(L"123456789012",12)==L"123456789012");require(menuLabel(L"1234567890123",12)==L"123456789...");require(menuLabel(L"ABCDEFGHIJKLM",8)==L"ABCDE...");require(menuLabel(L"ABCDEFGHIJKLM",10)==L"ABCDEFG...");require(menuLabel(L"😀😀😀😀😀😀😀😀😀😀😀😀😀",12).size()==21);});
    test("menu scaling and linear layer decrement",[&]{auto config=defaults();config["appearance"]={{"web",130},{"radial",140},{"ringStep",10}};validate(config);require(std::abs(radial::thickness(1,10)-54)<.001&&std::abs(radial::thickness(2,10)-48)<.001&&radial::thickness(2,10)==radial::thickness(4,10));const auto graph=radial::makeScene(config,{"sys-prog","sys-personal"});require(graph.panels.size()==3);for(size_t i=0;i<graph.panels.size();++i)require(std::abs(graph.panels[i].radius-graph.panels[i].inner-radial::thickness(static_cast<int>(i),10))<.001);config["appearance"]["web"]=181;bool rejected=false;try{validate(config);}catch(...){rejected=true;}require(rejected);});
    test("RGB colors animate independently and validate settings",[&]{const auto first=rgbColorAt("sys-prog",0,1,128),later=rgbColorAt("sys-prog",1000,1,128),other=rgbColorAt("sys-cmd",0,1,128);require(first.GetValue()!=later.GetValue()&&first.GetValue()!=other.GetValue()&&first.GetA()==128);require(later.GetValue()==rgbColorAt("sys-prog",500,2,128).GetValue());configuration=defaults();configuration["rgb"]={{"radial",true},{"blocks",false},{"branches",false},{"speed",1.0}};validate(configuration);require(rgbActive());configuration["launcher"]["view"]="web";require(!rgbActive());configuration["rgb"]["branches"]=true;require(rgbActive());configuration["rgb"]["speed"]=0;bool rejected=false;try{validate(configuration);}catch(...){rejected=true;}require(rejected);});
    test("web material opacity, tint and blur validation",[&]{configuration=defaults();configuration["launcher"]["view"]="web";configuration["webStyle"]={{"color","#ff0000"},{"opacity",50},{"blur",0}};validate(configuration);navigation.clear();expanded.clear();reveals.clear();blurredBackground.reset();updateEntries();reveals.clear();worldMinX=worldMinY=-650;scale=1;canvasWidth=canvasHeight=1300;const auto sample=[&](){Bitmap bitmap(1300,1300,PixelFormat32bppPARGB);{Graphics g(&bitmap);paintScene(g);}Color pixel;bitmap.GetPixel(static_cast<int>(scene.nodes[0].point.x-worldMinX+45),static_cast<int>(scene.nodes[0].point.y-worldMinY+10),&pixel);return pixel;};auto half=sample();require(half.GetA()>=125&&half.GetA()<=130&&half.GetR()>half.GetG()+40);configuration["webStyle"]["opacity"]=0;require(sample().GetA()==0);configuration["webStyle"]["opacity"]=100;require(sample().GetA()==255);configuration["webStyle"]["blur"]=25;bool rejected=false;try{validate(configuration);}catch(...){rejected=true;}require(rejected);});
    test("rotated radial icons are clipped and follow the label tangent",[&]{configuration=defaults();configuration["groups"].push_back({{"id","one"},{"name","One"},{"position",2},{"parentId",nullptr}});configuration["groups"].push_back({{"id","two"},{"name","Two"},{"position",3},{"parentId",nullptr}});validate(configuration);navigation.clear();expanded.clear();reveals.clear();updateEntries();for(auto& pair:ringOpened)pair.second=0;auto marker=std::make_unique<Bitmap>(48,48,PixelFormat32bppPARGB);{Graphics g(marker.get());g.Clear(Color(255,255,0,0));SolidBrush green(Color(255,0,255,0));g.FillRectangle(&green,0,24,48,24);}iconCache["@folder"]={L"",std::move(marker)};worldMinX=worldMinY=-360;scale=1;canvasWidth=canvasHeight=720;Bitmap bitmap(720,720,PixelFormat32bppPARGB);{Graphics g(&bitmap);paintScene(g);}for(const auto& sector:rings.sectors){const float r=radial::iconRadius(sector);const double angle=sector.angle+(std::sin(sector.angle)>0.1?-PI/2:PI/2);const int x=static_cast<int>(std::round(std::cos(sector.angle)*r-4*std::sin(angle)-worldMinX)),y=static_cast<int>(std::round(std::sin(sector.angle)*r+4*std::cos(angle)-worldMinY));Color color;bitmap.GetPixel(x,y,&color);require(color.GetG()>color.GetR()+100);}for(int y=0;y<720;y++)for(int x=0;x<720;x++){Color color;bitmap.GetPixel(x,y,&color);if(color.GetG()>220&&color.GetR()<20&&color.GetB()<20)require(radial::hit(rings,{x+worldMinX,y+worldMinY})>=0);}iconCache.clear();});
    test("web layout supports thirteen siblings, deep groups and direct group links",[&]{auto config=defaults();for(int i=1;i<=13;i++)config["apps"].push_back({{"id","web-"+std::to_string(i)},{"name","App"},{"path","app.exe"},{"groupId","sys-personal"},{"position",i}});validate(config);const auto graph=tree::makeScene(config);std::set<std::pair<int,int>> points;tree::Point parent{},child{};for(const auto& node:graph.nodes){if(node.parent=="sys-personal")points.insert({static_cast<int>(node.point.x*100),static_cast<int>(node.point.y*100)});if(node.item["id"]=="sys-prog")parent=node.point;if(node.item["id"]=="sys-personal")child=node.point;}require(points.size()==13);bool direct=false;for(const auto& edge:graph.edges)if(tree::samePoint(edge.a,parent)&&tree::samePoint(edge.b,child))direct=true;require(direct);json nodes=json::array();for(const auto& node:graph.nodes)nodes.push_back({{"id",node.item["id"]},{"x",node.point.x},{"y",node.point.y}});std::ofstream(exeDirectory/L"web-geometry.json")<<json({{"config",config},{"nodes",nodes}}).dump(2);config["apps"]=json::array();std::string previous="sys-personal";for(int i=0;i<5;++i){const auto id="deep-"+std::to_string(i);config["groups"].push_back({{"id",id},{"name","Deep"},{"position",1},{"parentId",previous}});previous=id;}validate(config);const auto deep=tree::makeScene(config);require(std::any_of(deep.nodes.begin(),deep.nodes.end(),[&](const auto& n){return n.item["id"]==previous;}));});
    test("readable label budgets and ring capacities",[&]{require(radial::capacity(0)==5&&radial::capacity(1)==7&&radial::capacity(2)==13&&radial::capacity(5)==13);require(radial::labelLimit(0)==8&&radial::labelLimit(1)==12&&radial::labelLimit(2)==10);require(compactName("1234567890123")=="1234567890123"&&compactName("1234567890123",12)=="123456789012");auto config=defaults();config["apps"].push_back({{"id","thirteen"},{"name","TwelveChars"},{"path","app.exe"},{"groupId","sys-personal"},{"position",13}});validate(config);config["apps"].back()["position"]=14;bool rejected=false;try{validate(config);}catch(...){rejected=true;}require(rejected);});
    test("rounding and high resolution alpha icons",[&]{configuration=defaults();auto graph=radial::makeScene(configuration,{"sys-prog"});const auto sector=graph.sectors[1];configuration["radial"]={{"rounded",true},{"roundness",8},{"blur",16}};GraphicsPath rounded;sectorShape(rounded,sector);configuration["radial"]["rounded"]=false;GraphicsPath square;sectorShape(square,sector);require(rounded.GetPointCount()>square.GetPointCount());auto icon=loadIconImage(L"",true);require(icon&&icon->GetWidth()==48&&icon->GetHeight()==48);int clear=0,visiblePixels=0;for(int y=0;y<48;y++)for(int x=0;x<48;x++){Color pixel;icon->GetPixel(x,y,&pixel);if(pixel.GetA())++visiblePixels;else ++clear;}require(clear>0&&visiblePixels>0);});
    test("equal circular sectors, no placeholder positions, empty center",[&]{for(int n=1;n<=7;++n){auto config=defaults();config["groups"]=json::array();config["apps"]=json::array();for(int i=0;i<n;++i)config["groups"].push_back({{"id",std::to_string(i)},{"name","Group"},{"parentId",nullptr},{"position",i+1}});const auto graph=radial::makeScene(config);require(graph.sectors.size()==static_cast<size_t>(n));for(const auto& sector:graph.sectors)require(std::abs(sector.sweep-2*PI/n)<0.00001);require(radial::hit(graph,{0,0})==-1);}});
    test("layers stay visible, concentric and shrink only through layer three",[&]{auto config=defaults();config["groups"].push_back({{"id","deep"},{"name","Deep"},{"parentId","sys-personal"},{"position",1}});config["groups"].push_back({{"id","deeper"},{"name","Deeper"},{"parentId","deep"},{"position",1}});validate(config);const auto graph=radial::makeScene(config,{"sys-prog","sys-personal","deep","deeper"});require(graph.panels.size()==5);for(size_t i=0;i<graph.panels.size();++i){require(graph.panels[i].center.x==0&&graph.panels[i].center.y==0);if(i)require(graph.panels[i].inner>graph.panels[i-1].radius);}require(radial::thickness(0)>radial::thickness(1)&&radial::thickness(1)>radial::thickness(2));require(radial::thickness(2)==radial::thickness(4));});
    test("system command paths protected, order and colors editable",[&]{auto config=defaults();config["apps"][0]["path"]="changed.exe";config["apps"][0]["position"]=2;config["apps"][1]["position"]=1;config["radial"]={{"color","#70818b"},{"colors",{{"sys-settings","#ab5678"}}}};validate(config);bool found=false;for(const auto& app:config["apps"])if(app["id"]=="sys-settings"){require(app["path"]=="ms-settings:"&&app["position"]==2);found=true;}require(found);const auto graph=radial::makeScene(config,{"sys-prog"});for(const auto& sector:graph.sectors)if(sector.item["id"]=="sys-settings")require(sector.color=="#ab5678");});
    test("circular hit testing retains old layer targets",[&]{auto config=defaults();const auto graph=radial::makeScene(config,{"sys-prog"});for(size_t i=0;i<graph.sectors.size();++i)require(radial::hit(graph,graph.sectors[i].label)==static_cast<int>(i));require(radial::hit(graph,{1000,1000})==-1);});
    test("nested group cycles rejected",[&]{auto config=defaults();config["groups"][1]["parentId"]="sys-personal";bool rejected=false;try{validate(config);}catch(...){rejected=true;}require(rejected);});
    test("monitor selection defaults to primary",[&]{configuration=defaults();require(launcherMonitor().dwFlags&MONITORINFOF_PRIMARY);configuration["launcher"]["monitor"]="disconnected-screen";require(launcherMonitor().dwFlags&MONITORINFOF_PRIMARY);});
    test("native annulus material leaves the center transparent",[&]{configuration=defaults();validate(configuration);navigation.clear();ringOpened.clear();updateEntries();for(auto& pair:ringOpened)pair.second=0;worldMinX=worldMinY=-360;canvasWidth=canvasHeight=720;scale=1;Bitmap bitmap(720,720,PixelFormat32bppPARGB);{Graphics g(&bitmap);paintScene(g);}Color center;bitmap.GetPixel(360,360,&center);require(center.GetA()==0);Color sector;bitmap.GetPixel(360,240,&sector);require(sector.GetA()>0);const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};require(bitmap.Save((exeDirectory/L"radial-native.png").c_str(),&png,nullptr)==Ok);});
    test("ring navigation keeps parent rings and back removes only the newest",[&]{configuration=defaults();validate(configuration);navigation.clear();updateEntries();require(rings.panels.size()==1);enterGroup(0);require(rings.panels.size()==2&&navigation.back()=="sys-prog");back();require(rings.panels.size()==1&&navigation.empty());});
    test("application launch uses Windows Shell",[&]{HANDLE event=CreateEventW(nullptr,TRUE,FALSE,L"Local\\SpiderWeb.Test.Launched");ResetEvent(event);entries={{"fixture",L"Fixture",(exeDirectory/L"SpiderWeb.TestFixture.exe").wstring(),1,false}};iconCache["fixture"]={entries[0].path,nullptr,0,0,true,0};selectItem(0);const auto result=WaitForSingleObject(event,5000);CloseHandle(event);require(result==WAIT_OBJECT_0);});
    auto config=defaults();config["groups"].push_back({{"id","work"},{"name","Work"},{"parentId",nullptr},{"position",2}});config["radial"]={{"color","#70818b"},{"rotation",{{"@root",0.5}}},{"colors",{{"sys-cmd","#6688aa"}}}};validate(config);const std::vector<std::string> path{"sys-prog","sys-personal"};const auto graph=radial::makeScene(config,path);json sectors=json::array();for(const auto& s:graph.sectors)sectors.push_back({{"id",s.item["id"]},{"parent",s.parent},{"depth",s.depth},{"x",s.label.x},{"y",s.label.y},{"inner",s.inner},{"radius",s.radius},{"start",s.start},{"sweep",s.sweep},{"color",s.color}});std::ofstream(exeDirectory/L"radial-geometry.json")<<json({{"config",config},{"path",path},{"sectors",sectors}}).dump(2);
    std::ofstream(exeDirectory/L"native-tests.json")<<tests.dump(2);return passed?0:1;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int) {
    (void)command;const cli::Arguments arguments;if(!arguments.valid())return 2;
    if(arguments.is(L"--app-name"))return appname::execute();
    if(arguments.is(L"--autostart-status")||arguments.is(L"--autostart-on")||arguments.is(L"--autostart-off"))return startup::execute(arguments.values.front().c_str());
    instanceHandle=instance;SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if(arguments.is(L"--icon-pixels"))return exportIconPixels(arguments.values.at(1));
    if(arguments.is(L"--app-status"))return exportAppStatus(arguments.values.at(1));
    if(arguments.is(L"--export-icons"))return exportIcons();if(arguments.is(L"--export-monitors"))return exportMonitors();
    bool demo=arguments.is(L"--demo"),quit=arguments.is(L"--quit");
    bool quitAll=arguments.is(L"--quit-all"),background=arguments.is(L"--background");
    bool reload=arguments.is(L"--reload");
    HWND existing=FindWindowW(CLASS_NAME,nullptr);
    if(existing){if(!background)PostMessageW(existing,quitAll?QUIT_ALL:quit?QUIT:reload?RELOAD:DEMO,0,0);return 0;}
    if(quitAll)closeSettings();
    if(quit||quitAll||reload)return 0;
    HANDLE singleton=CreateMutexW(nullptr,TRUE,L"Local\\SpiderWeb.Native.Singleton");
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(singleton);return 0;}
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    ULONG_PTR gdiplus{};GdiplusStartupInput startup;GdiplusStartup(&gdiplus,&startup,nullptr);
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);exeDirectory=fs::path(exe).parent_path();
    if(arguments.is(L"--self-test")){int result=runTests();iconCache.clear();blurredBackground.reset();GdiplusShutdown(gdiplus);CoUninitialize();ReleaseMutex(singleton);CloseHandle(singleton);return result;}
    PWSTR roaming{};SHGetKnownFolderPath(FOLDERID_RoamingAppData,0,nullptr,&roaming);configPath=fs::path(roaming)/L"com.spiderweb.settings"/L"config.json";CoTaskMemFree(roaming);
    std::error_code error;fs::create_directories(configPath.parent_path(),error);
    stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=CLASS_NAME;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(IDI_SPIDERWEB));RegisterClassW(&wc);
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    windowHandle=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_TOOLWINDOW,CLASS_NAME,L"FoldBOcean",WS_POPUP,0,0,720,720,nullptr,nullptr,instance,nullptr);
    if(!windowHandle)return 1;
    // Consume STARTUPINFO's initial visibility before any requested demo show.
    ShowWindow(windowHandle,SW_HIDE);
    SetWindowPos(windowHandle,nullptr,0,0,0,0,SWP_HIDEWINDOW|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowDisplayAffinity(windowHandle,WDA_NONE);addTray();configuration=defaults();loadConfiguration();
    hookThread=std::thread(hooks);watcherThread=std::thread(watchConfig);
    if(demo)PostMessageW(windowHandle,DEMO,0,0);
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    stopIconWorker();if(hookThread.joinable())hookThread.join();if(watcherThread.joinable())watcherThread.join();
    CloseHandle(stopEvent);ReleaseMutex(singleton);CloseHandle(singleton);GdiplusShutdown(gdiplus);CoUninitialize();return 0;
}
