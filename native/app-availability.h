#pragma once
enum class ProgramState {unknown,available,missing};
std::wstring expandedProgramPath(const std::wstring& source){
    const DWORD length=ExpandEnvironmentStringsW(source.c_str(),nullptr,0);if(!length||length>32768)return source;
    std::wstring result(length,L'\0');if(!ExpandEnvironmentStringsW(source.c_str(),result.data(),length))return source;result.resize(length-1);return result;
}
ProgramState programState(const std::wstring& source,bool shortcut=true){
    auto path=expandedProgramPath(source);
    const auto colon=path.find(L':');if(colon!=std::wstring::npos&&colon>1)return ProgramState::available; // Shell/URI targets are not files.
    if(path.find_first_of(L"\\/")==std::wstring::npos){
        wchar_t found[32768]{};if(SearchPathW(nullptr,path.c_str(),nullptr,32768,found,nullptr)||SearchPathW(nullptr,path.c_str(),L".exe",32768,found,nullptr))path=found;
        else {
            for(auto root:{HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE}){
                wchar_t value[32768]{};DWORD size=sizeof(value);const auto key=L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"+path;
                if(RegGetValueW(root,key.c_str(),nullptr,RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ,nullptr,value,&size)==ERROR_SUCCESS){path=expandedProgramPath(value);if(path.size()>1&&path.front()==L'"'&&path.back()==L'"')path=path.substr(1,path.size()-2);break;}
            }
        }
    }
    const DWORD attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES){const DWORD error=GetLastError();return error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND?ProgramState::missing:ProgramState::unknown;}
    if(shortcut&&_wcsicmp(fs::path(path).extension().c_str(),L".lnk")==0){
        IShellLinkW* link{};if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link)))){
            IPersistFile* file{};std::wstring target;
            if(SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))){if(SUCCEEDED(file->Load(path.c_str(),STGM_READ))){wchar_t value[32768]{};if(SUCCEEDED(link->GetPath(value,32768,nullptr,SLGP_RAWPATH)))target=value;}file->Release();}
            link->Release();if(!target.empty())return programState(target,false);
        }
    }
    return ProgramState::available;
}
