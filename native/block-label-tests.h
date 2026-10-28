#pragma once
#include <cwctype>
template<class Test,class Require>void runBlockLabelTests(Test test,Require require){
    test("availability resolves commands and detects deleted files and broken shortcuts",[&]{
        wchar_t system[32768]{};require(GetSystemDirectoryW(system,32768)>0);const auto command=(fs::path(system)/L"cmd.exe").wstring();
        require(programState(command)==ProgramState::available&&programState(L"cmd.exe")==ProgramState::available&&programState(L"cmd")==ProgramState::available&&programState(L"ms-settings:")==ProgramState::available);
        const auto file=exeDirectory/(L"availability-test-"+std::to_wstring(GetCurrentProcessId())+L".cmd");const auto shortcut=file.wstring()+L".lnk";
        struct Cleanup {fs::path file,link;~Cleanup(){std::error_code error;fs::remove(file,error);fs::remove(link,error);}}cleanup{file,shortcut};
        require(programState(file.wstring())==ProgramState::missing);std::ofstream(file)<<"test";require(programState(file.wstring())==ProgramState::available);fs::remove(file);require(programState(file.wstring())==ProgramState::missing);
        IShellLinkW* link{};require(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))));
        IPersistFile* storage{};require(SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&storage))));link->SetPath(file.c_str());require(SUCCEEDED(storage->Save(shortcut.c_str(),TRUE)));require(programState(shortcut)==ProgramState::missing);
        link->SetPath(command.c_str());require(SUCCEEDED(storage->Save(shortcut.c_str(),TRUE)));storage->Release();link->Release();require(programState(shortcut)==ProgramState::available);
        ProgramState state=ProgramState::unknown;const IconRequest request{"probe",file.wstring(),false,iconGeneration.load(),false};require(fetchIcon(request,&state).empty()&&state==ProgramState::missing);
        std::ofstream(file)<<"test";state=ProgramState::unknown;require(fetchIcon(request,&state).empty()&&state==ProgramState::available);
        configuration=defaults();configuration["apps"].push_back({{"id","probe"},{"name","Probe"},{"groupId","sys-personal"},{"position",1},{"path",utf8(file.wstring())},{"noIcon",true}});
        {std::lock_guard<std::mutex> lock(iconMutex);iconResults.push_back({request,{},ProgramState::missing});}require(applyIconResults()&&iconCache["probe"].missing);
        {std::lock_guard<std::mutex> lock(iconMutex);iconResults.push_back({request,{},ProgramState::available});}require(applyIconResults()&&!iconCache["probe"].missing);iconCache.clear();
    });
    test("full missing messages and no-icon labels render in both modes without ellipsis",[&]{
        configuration=defaults();configuration["groups"][0]["noIcon"]=true;configuration["apps"][0]["noIcon"]=true;validate(configuration);
        require(std::any_of(configuration["groups"].begin(),configuration["groups"].end(),[](const auto& group){return group["id"]=="sys-prog"&&group.value("noIcon",false);}));
        configuration["apps"].push_back({{"id","missing-label"},{"name","Очень длинное полное название потерянной программы"},{"groupId","sys-personal"},{"path","C:\\FoldBOcean.Missing.Test\\absent.exe"},{"position",1}});
        configuration["apps"].push_back({{"id","text-label"},{"name","ДлинноеНазваниеПрограммыБезЗначка"},{"groupId","sys-personal"},{"path","cmd.exe"},{"position",2},{"noIcon",true}});
        navigation={"sys-prog","sys-personal"};expanded={"sys-prog","sys-personal"};configuration["launcher"]["view"]="radial";blurredBackground.reset();updateEntries();for(auto& pair:ringOpened)pair.second=0;
        worldMinX=worldMinY=-360;canvasWidth=canvasHeight=720;scale=1;Bitmap circle(720,720,PixelFormat32bppPARGB);{Graphics graphics(&circle);paintScene(graphics);}
        const auto message=missingProgramMessage(L"Очень длинное полное название потерянной программы");require(iconCache["missing-label"].missing&&!arcBlockLayouts.empty());
        bool checked=false;for(const auto& [key,layout]:arcBlockLayouts)if(std::get<0>(key)==message){std::wstring joined;for(const auto& line:layout.lines)joined+=line;auto trim=[](std::wstring value){value.erase(std::remove_if(value.begin(),value.end(),[](wchar_t c){return std::iswspace(c)!=0;}),value.end());return value;};require(trim(joined)==trim(message));checked=true;}require(checked);
        const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};require(circle.Save((exeDirectory/L"missing-circle-native.png").c_str(),&png,nullptr)==Ok);
        configuration["launcher"]["view"]="web";updateEntries();reveals.clear();float minX=0,minY=0,maxX=0,maxY=0;for(const auto& node:scene.nodes){minX=std::min(minX,node.point.x-tree::HALF_WIDTH);minY=std::min(minY,node.point.y-tree::HALF_HEIGHT);maxX=std::max(maxX,node.point.x+tree::HALF_WIDTH);maxY=std::max(maxY,node.point.y+tree::HALF_HEIGHT);}worldMinX=minX-12;worldMinY=minY-12;canvasWidth=static_cast<int>(std::ceil(maxX-minX+24));canvasHeight=static_cast<int>(std::ceil(maxY-minY+24));scale=1;Bitmap web(canvasWidth,canvasHeight,PixelFormat32bppPARGB);{Graphics graphics(&web);paintScene(graphics);}
        require(!wrappedFontSizes.empty());require(web.Save((exeDirectory/L"missing-web-native.png").c_str(),&png,nullptr)==Ok);
        navigation.clear();expanded.clear();iconCache.clear();configuration=defaults();updateEntries();
    });
}
