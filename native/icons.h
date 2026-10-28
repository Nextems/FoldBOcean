#pragma once
#include <memory>
#include <commoncontrols.h>
#include "app-availability.h"
#include "icon-worker.h"

struct CachedIcon {std::wstring source;std::unique_ptr<Gdiplus::Bitmap> image;ULONGLONG loaded=0,used=0;bool missing=false;ULONGLONG checked=0;};
std::map<std::string,CachedIcon> iconCache;
std::unique_ptr<Gdiplus::Bitmap> loadIconImage(const std::wstring& input,bool folder=false) {
    HICON icon{};int sourceSize=48;
    if(folder){SHSTOCKICONINFO info{sizeof(info)};if(SUCCEEDED(SHGetStockIconInfo(SIID_FOLDER,SHGSI_ICON|SHGSI_LARGEICON,&info)))icon=info.hIcon;}
    else {
        std::wstring path=input;wchar_t expandedPath[32768]{};if(path==L"ms-settings:")path=L"%SystemRoot%\\ImmersiveControlPanel\\SystemSettings.exe";ExpandEnvironmentStringsW(path.c_str(),expandedPath,32768);path=expandedPath;
        wchar_t resolved[32768]{};if(path.find(L':')==std::wstring::npos&&SearchPathW(nullptr,path.c_str(),nullptr,32768,resolved,nullptr))path=resolved;
        UINT iconId{};if(PrivateExtractIconsW(path.c_str(),0,256,256,&icon,&iconId,1,LR_DEFAULTCOLOR)>0&&icon)sourceSize=256;
        SHFILEINFOW info{};std::error_code error;const UINT flags=SHGFI_SYSICONINDEX|(fs::exists(path,error)?0:SHGFI_USEFILEATTRIBUTES);
        if(!icon&&SHGetFileInfoW(path.c_str(),FILE_ATTRIBUTE_NORMAL,&info,sizeof(info),flags)){
            IImageList* images{};if(SUCCEEDED(SHGetImageList(SHIL_EXTRALARGE,__uuidof(IImageList),reinterpret_cast<void**>(&images)))){images->GetIcon(info.iIcon,ILD_TRANSPARENT,&icon);images->Release();}
        }
        if(!icon){SHGetFileInfoW(path.c_str(),FILE_ATTRIBUTE_NORMAL,&info,sizeof(info),SHGFI_ICON|SHGFI_LARGEICON|(fs::exists(path,error)?0:SHGFI_USEFILEATTRIBUTES));icon=info.hIcon;}
    }
    if(!icon)return {};
    // Draw onto black and white DIBs to reconstruct premultiplied alpha correctly,
    // including legacy masked icons. Bitmap(HICON) can lose their alpha channel.
    const int size=sourceSize;BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=size;info.bmiHeader.biHeight=-size;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);void *blackBits{},*whiteBits{};HBITMAP black=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&blackBits,nullptr,0),white=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&whiteBits,nullptr,0);
    if(!black||!white){if(black)DeleteObject(black);if(white)DeleteObject(white);DeleteDC(dc);DestroyIcon(icon);return {};}
    auto* pixels=static_cast<DWORD*>(blackBits);auto* light=static_cast<DWORD*>(whiteBits);std::fill(pixels,pixels+size*size,0);std::fill(light,light+size*size,0x00ffffff);
    auto old=SelectObject(dc,black);DrawIconEx(dc,0,0,icon,size,size,0,nullptr,DI_NORMAL);SelectObject(dc,white);DrawIconEx(dc,0,0,icon,size,size,0,nullptr,DI_NORMAL);SelectObject(dc,old);GdiFlush();
    for(int i=0;i<size*size;++i){int difference=0;for(int shift:{0,8,16})difference=std::max(difference,int((light[i]>>shift)&255)-int((pixels[i]>>shift)&255));const DWORD alpha=static_cast<DWORD>(std::clamp(255-difference,0,255));pixels[i]=(pixels[i]&0x00ffffff)|(alpha<<24);}
    auto result=std::make_unique<Gdiplus::Bitmap>(size,size,PixelFormat32bppPARGB);Gdiplus::BitmapData data{};Gdiplus::Rect rect(0,0,size,size);
    if(result->LockBits(&rect,Gdiplus::ImageLockModeWrite,PixelFormat32bppPARGB,&data)==Gdiplus::Ok){for(int row=0;row<size;++row)memcpy(static_cast<BYTE*>(data.Scan0)+row*data.Stride,pixels+row*size,size*4);result->UnlockBits(&data);}else result.reset();
    DeleteObject(black);DeleteObject(white);DeleteDC(dc);DestroyIcon(icon);
    if(result&&size!=48){auto resized=std::make_unique<Gdiplus::Bitmap>(48,48,PixelFormat32bppPARGB);{Gdiplus::Graphics g(resized.get());g.Clear(Gdiplus::Color(0,0,0,0));g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);Gdiplus::ImageAttributes attributes;attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);g.DrawImage(result.get(),Gdiplus::RectF(0,0,48,48),0,0,static_cast<Gdiplus::REAL>(size),static_cast<Gdiplus::REAL>(size),Gdiplus::UnitPixel,&attributes);}result=std::move(resized);}
    return result;
}
void syncIcons() {
    std::set<std::string> current;const auto now=GetTickCount64();
    for(const auto& item:entries) {
        if(item.group&&item.noIcon)continue;
        const auto key=item.group?"@folder":item.id;current.insert(key);
        auto found=iconCache.find(key);
        if(found==iconCache.end()||found->second.source!=item.path){
            iconCache.erase(key);
            if(!windowHandle){const auto state=item.group?ProgramState::available:programState(item.path);iconCache[key]={item.path,item.noIcon||state==ProgramState::missing?nullptr:loadIconImage(item.path,item.group),now,now,state==ProgramState::missing,now};}
            else requestIcon(key,item.path,item.group,!item.noIcon);
        }else{found->second.used=now;if(windowHandle){if(!item.noIcon&&now-found->second.loaded>60000)requestIcon(key,item.path,item.group);else if(!item.group&&now-found->second.checked>1000)requestIcon(key,item.path,false,false);}}
    }
    while(iconCache.size()>32){auto oldest=iconCache.end();for(auto it=iconCache.begin();it!=iconCache.end();++it)if(!current.count(it->first)&&(oldest==iconCache.end()||it->second.used<oldest->second.used))oldest=it;if(oldest==iconCache.end())break;iconCache.erase(oldest);}
}
bool applyIconResults(){
    std::vector<IconResult> results;{std::lock_guard<std::mutex> lock(iconMutex);results.swap(iconResults);for(const auto& result:results)pendingIcons.erase({result.request.key,result.request.path});}
    bool changed=false;
    for(auto& result:results){
        const auto& request=result.request;if(request.generation!=iconGeneration)continue;
        if(!request.folder&&!std::any_of(configuration["apps"].begin(),configuration["apps"].end(),[&](const auto& app){return app["id"]==request.key&&wide(app["path"].get<std::string>())==request.path;}))continue;
        std::unique_ptr<Bitmap> image;
        if(result.pixels.size()==48*48*4){image=std::make_unique<Bitmap>(48,48,PixelFormat32bppPARGB);BitmapData data{};Rect rect(0,0,48,48);if(image->LockBits(&rect,ImageLockModeWrite,PixelFormat32bppPARGB,&data)==Ok){for(int row=0;row<48;++row)memcpy(static_cast<BYTE*>(data.Scan0)+row*data.Stride,result.pixels.data()+row*48*4,48*4);image->UnlockBits(&data);}else image.reset();}
        const auto now=GetTickCount64();auto& cached=iconCache[request.key];const bool wasMissing=cached.missing;cached.source=request.path;cached.used=cached.checked=now;
        if(result.state!=ProgramState::unknown)cached.missing=result.state==ProgramState::missing;
        if(request.withIcon){cached.image=std::move(image);cached.loaded=now;}
        if(cached.missing)cached.image.reset();
        else if(!request.withIcon&&result.state==ProgramState::available&&(wasMissing||!cached.image)){
            const auto app=std::find_if(configuration["apps"].begin(),configuration["apps"].end(),[&](const auto& a){return a["id"]==request.key;});
            if(app!=configuration["apps"].end()&&!app->value("noIcon",false))requestIcon(request.key,request.path,false);
        }
        changed=true;
    }
    return changed;
}
struct IconRuntime {
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);ULONG_PTR token{};
    IconRuntime(){Gdiplus::GdiplusStartupInput startup;if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok)throw std::runtime_error("GDI+ startup failed");}
    ~IconRuntime(){if(token)Gdiplus::GdiplusShutdown(token);if(SUCCEEDED(com))CoUninitialize();}
};
int exportIconPixels(const std::wstring& source){
    try{IconRuntime runtime;if(source!=L"@folder"&&programState(source)==ProgramState::missing)return 3;auto image=loadIconImage(source,source==L"@folder");if(!image)return 2;BitmapData data{};Rect rect(0,0,48,48);if(image->LockBits(&rect,ImageLockModeRead,PixelFormat32bppPARGB,&data)!=Ok)return 1;std::vector<BYTE> pixels(48*48*4);for(int row=0;row<48;++row)memcpy(pixels.data()+row*48*4,static_cast<BYTE*>(data.Scan0)+row*data.Stride,48*4);image->UnlockBits(&data);DWORD written=0;return WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),pixels.data(),static_cast<DWORD>(pixels.size()),&written,nullptr)&&written==pixels.size()?0:1;}
    catch(...){return 1;}
}
int exportAppStatus(const std::wstring& source){try{IconRuntime runtime;const auto state=programState(source);return state==ProgramState::missing?3:state==ProgramState::available?0:4;}catch(...){return 4;}}
std::string base64(const unsigned char* data,size_t size) {
    const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string result;
    for(size_t i=0;i<size;i+=3){const unsigned value=(unsigned(data[i])<<16)|(i+1<size?unsigned(data[i+1])<<8:0)|(i+2<size?data[i+2]:0);result+=alphabet[(value>>18)&63];result+=alphabet[(value>>12)&63];result+=i+1<size?alphabet[(value>>6)&63]:'=';result+=i+2<size?alphabet[value&63]:'=';}
    return result;
}
std::string iconDataUrl(Gdiplus::Bitmap& bitmap) {
    IStream* stream{};if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&stream)))return {};
    const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    std::string result;
    if(bitmap.Save(stream,&png,nullptr)==Gdiplus::Ok){STATSTG stat{};HGLOBAL memory{};if(SUCCEEDED(stream->Stat(&stat,STATFLAG_NONAME))&&SUCCEEDED(GetHGlobalFromStream(stream,&memory))){auto* bytes=static_cast<const unsigned char*>(GlobalLock(memory));if(bytes){result="data:image/png;base64,"+base64(bytes,static_cast<size_t>(stat.cbSize.QuadPart));GlobalUnlock(memory);}}}
    stream->Release();return result;
}
int exportIcons() {
    try {
    IconRuntime runtime;
    PWSTR roaming{};if(FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData,0,nullptr,&roaming)))return 1;configPath=fs::path(roaming)/L"com.spiderweb.settings"/L"config.json";CoTaskMemFree(roaming);
    configuration=defaults();if(!loadConfiguration(false))return 1;json result=json::object();
    if(auto folder=loadIconImage(L"",true))result["@folder"]=iconDataUrl(*folder);
    result["@missing"]=json::object();
    for(const auto& app:configuration["apps"]){const auto path=wide(app["path"].get<std::string>());const auto id=app["id"].get<std::string>();if(programState(path)==ProgramState::missing){result["@missing"][id]=true;continue;}if(!app.value("noIcon",false))if(auto icon=loadIconImage(path))result[id]=iconDataUrl(*icon);}
    const std::string output=result.dump();DWORD written{};const BOOL success=WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),output.data(),static_cast<DWORD>(output.size()),&written,nullptr);
    return success&&written==output.size()?0:1;
    }catch(...){return 1;}
}
