#pragma once
#include <cwctype>
namespace appname {
inline std::wstring clean(std::wstring value){for(auto& c:value)if(c<L' ')c=L' ';const auto first=value.find_first_not_of(L" \t\r\n"),last=value.find_last_not_of(L" \t\r\n");return first==std::wstring::npos?L"":wide(compactName(utf8(value.substr(first,last-first+1)),256));}
inline fs::path resolve(std::wstring value){
    const auto first=value.find_first_not_of(L" \t\r\n"),last=value.find_last_not_of(L" \t\r\n");value=first==std::wstring::npos?L"":value.substr(first,last-first+1);if(value.size()>1&&value.front()==L'"'&&value.back()==L'"')value=value.substr(1,value.size()-2);
    wchar_t expandedPath[32768]{};const auto expandedSize=ExpandEnvironmentStringsW(value.c_str(),expandedPath,32768);if(expandedSize&&expandedSize<=32768)value=expandedPath;
    wchar_t found[32768]{};const auto foundLength=SearchPathW(nullptr,value.c_str(),nullptr,32768,found,nullptr);if(foundLength>0&&foundLength<32768)value=found;
    fs::path path(value);auto extension=path.extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    if(extension==L".lnk"){IShellLinkW* link=nullptr;if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)))){IPersistFile* file=nullptr;if(SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))){if(SUCCEEDED(file->Load(value.c_str(),STGM_READ))){wchar_t target[32768]{};if(SUCCEEDED(link->GetPath(target,32768,nullptr,SLGP_RAWPATH))&&target[0])path=target;}file->Release();}link->Release();}}
    return path;
}
inline json read(const std::wstring& original){
    const auto path=resolve(original);DWORD ignored=0;const DWORD size=GetFileVersionInfoSizeW(path.c_str(),&ignored);
    if(size&&size<=16*1024*1024){std::vector<BYTE> data(size);if(GetFileVersionInfoW(path.c_str(),0,size,data.data())){
        struct Translation{WORD language,codepage;};Translation* translations=nullptr;UINT bytes=0;std::vector<Translation> candidates;
        if(VerQueryValueW(data.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void**>(&translations),&bytes))for(UINT i=0;i<bytes/sizeof(Translation);++i)candidates.push_back(translations[i]);
        std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return (a.language==GetUserDefaultUILanguage())>(b.language==GetUserDefaultUILanguage());});candidates.push_back({0x0409,0x04b0});candidates.push_back({0x0409,0x04e4});
        for(const auto* key:{L"FileDescription",L"ProductName"})for(const auto& locale:candidates){wchar_t query[128]{};swprintf_s(query,L"\\StringFileInfo\\%04x%04x\\%ls",locale.language,locale.codepage,key);wchar_t* value=nullptr;UINT length=0;if(VerQueryValueW(data.data(),query,reinterpret_cast<void**>(&value),&length)&&value&&length){const auto name=clean(std::wstring(value,length-(value[length-1]==0?1:0)));if(!name.empty())return {{"name",utf8(name)},{"source",key==std::wstring(L"FileDescription")?"description":"product"}};}}
    }}
    auto fallback=clean(path.stem().wstring());if(fallback.empty())fallback=L"Приложение";return {{"name",utf8(fallback)},{"source","filename"}};
}
inline int execute(){
    int count=0;auto arguments=CommandLineToArgvW(GetCommandLineW(),&count);json result;int code=0;const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    try{bool found=false;for(int i=1;i+1<count;++i)if(wcscmp(arguments[i],L"--app-name")==0){result=read(arguments[i+1]);found=true;break;}if(!found)throw std::runtime_error("Missing application path");}catch(const std::exception& e){result={{"error",e.what()}};code=1;}
    if(arguments)LocalFree(arguments);if(SUCCEEDED(initialized))CoUninitialize();const auto output=result.dump();DWORD written=0;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),output.data(),static_cast<DWORD>(output.size()),&written,nullptr);return code;
}
}
