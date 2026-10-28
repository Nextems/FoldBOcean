#pragma once
#include <optional>
namespace startup {
constexpr wchar_t RUN_KEY[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t VALUE_NAME[]=L"FoldBOcean";
struct Key {HKEY handle=nullptr;~Key(){if(handle)RegCloseKey(handle);}};
inline void check(LSTATUS result){if(result!=ERROR_SUCCESS)throw std::runtime_error("Windows registry error: "+std::to_string(result));}
inline std::optional<std::wstring> read(const wchar_t* key=RUN_KEY){
    DWORD bytes=0;auto result=RegGetValueW(HKEY_CURRENT_USER,key,VALUE_NAME,RRF_RT_REG_SZ,nullptr,nullptr,&bytes);
    if(result==ERROR_FILE_NOT_FOUND||result==ERROR_PATH_NOT_FOUND)return std::nullopt;check(result);
    std::vector<wchar_t> buffer(bytes/sizeof(wchar_t)+1,0);check(RegGetValueW(HKEY_CURRENT_USER,key,VALUE_NAME,RRF_RT_REG_SZ,nullptr,buffer.data(),&bytes));return std::wstring(buffer.data());
}
inline void write(const std::optional<std::wstring>& command,const wchar_t* key=RUN_KEY){
    Key opened;
    if(command){check(RegCreateKeyExW(HKEY_CURRENT_USER,key,0,nullptr,0,KEY_SET_VALUE,nullptr,&opened.handle,nullptr));check(RegSetValueExW(opened.handle,VALUE_NAME,0,REG_SZ,reinterpret_cast<const BYTE*>(command->c_str()),static_cast<DWORD>((command->size()+1)*sizeof(wchar_t))));}
    else {const auto result=RegOpenKeyExW(HKEY_CURRENT_USER,key,0,KEY_SET_VALUE,&opened.handle);if(result==ERROR_FILE_NOT_FOUND||result==ERROR_PATH_NOT_FOUND)return;check(result);const auto deleted=RegDeleteValueW(opened.handle,VALUE_NAME);if(deleted!=ERROR_FILE_NOT_FOUND)check(deleted);}
}
inline std::wstring command(){
    wchar_t buffer[32768]{};const auto length=GetModuleFileNameW(nullptr,buffer,32768);if(!length||length>=32768)throw std::runtime_error("Cannot resolve launcher path");
    auto path=fs::path(buffer);const auto canonical=path.parent_path()/L"FoldBOcean.exe";if(fs::is_regular_file(canonical))path=canonical;
    return L"\""+path.wstring()+L"\" --background";
}
inline json status(){const auto expected=command();const auto actual=read();return {{"enabled",actual&&_wcsicmp(actual->c_str(),expected.c_str())==0},{"command",utf8(expected)}};}
inline int execute(const wchar_t* argument){
    json result;int code=0;
    try {if(wcscmp(argument,L"--autostart-on")==0)write(command());else if(wcscmp(argument,L"--autostart-off")==0)write(std::nullopt);result=status();}
    catch(const std::exception& error){result={{"error",error.what()}};code=1;}
    const auto output=result.dump();DWORD written=0;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),output.data(),static_cast<DWORD>(output.size()),&written,nullptr);return code;
}
}
