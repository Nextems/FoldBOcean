#pragma once
namespace cli {
struct Arguments {
    std::vector<std::wstring> values;
    Arguments(){int count=0;auto raw=CommandLineToArgvW(GetCommandLineW(),&count);if(!raw)throw std::runtime_error("Cannot parse command line");try{for(int i=1;i<count;++i)values.emplace_back(raw[i]);}catch(...){LocalFree(raw);throw;}LocalFree(raw);}
    bool is(const wchar_t* option)const{return !values.empty()&&values.front()==option;}
    bool valid()const{
        if(values.empty())return true;
        if(is(L"--app-name")||is(L"--icon-pixels")||is(L"--app-status"))return values.size()==2;
        if(values.size()!=1)return false;
        for(const auto* option:{L"--background",L"--demo",L"--quit",L"--quit-all",L"--reload",L"--self-test",L"--export-icons",L"--export-monitors",L"--autostart-status",L"--autostart-on",L"--autostart-off"})if(is(option))return true;
        return false;
    }
};
}
