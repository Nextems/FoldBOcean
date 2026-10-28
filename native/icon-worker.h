#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>

struct IconRequest {std::string key;std::wstring path;bool folder;unsigned generation;bool withIcon=true;};
struct IconResult {IconRequest request;std::vector<BYTE> pixels;ProgramState state=ProgramState::unknown;};
std::mutex iconMutex;
std::condition_variable iconCondition;
std::deque<IconRequest> iconRequests;
std::vector<IconResult> iconResults;
std::set<std::pair<std::string,std::wstring>> pendingIcons;
std::atomic<unsigned> iconGeneration=0;
bool iconWorkerStopping=false;
std::thread iconWorkerThread;
struct OwnedHandle {
    HANDLE value{};
    ~OwnedHandle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
std::wstring quotedArgument(const std::wstring& value){
    std::wstring out=L"\"";size_t slashes=0;
    for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}out.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;out+=c;}
    out.append(slashes*2,L'\\');out+=L'"';return out;
}
std::vector<BYTE> fetchIcon(const IconRequest& request,ProgramState* state=nullptr){
    wchar_t executable[32768]{};const auto length=GetModuleFileNameW(nullptr,executable,32768);if(!length||length>=32768)return {};
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};OwnedHandle read,write;
    if(!CreatePipe(&read.value,&write.value,&security,0))return {};
    SetHandleInformation(read.value,HANDLE_FLAG_INHERIT,0);
    OwnedHandle job;job.value=CreateJobObjectW(nullptr,nullptr);if(!job.value)return {};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))return {};
    OwnedHandle input;input.value=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=write.value;startup.hStdError=input.value;startup.hStdInput=input.value;
    auto command=quotedArgument(executable)+(request.withIcon?L" --icon-pixels ":L" --app-status ")+quotedArgument(request.folder?L"@folder":request.path);
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(executable,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&startup,&process))return {};
    OwnedHandle child,thread;child.value=process.hProcess;thread.value=process.hThread;
    if(!AssignProcessToJobObject(job.value,child.value)){TerminateProcess(child.value,1);WaitForSingleObject(child.value,1000);return {};}
    CloseHandle(write.value);write.value=nullptr;ResumeThread(thread.value);
    const auto deadline=GetTickCount64()+2500;std::vector<BYTE> result;result.reserve(48*48*4);
    for(;;){
        if(iconGeneration!=request.generation||GetTickCount64()>deadline||WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0)return {};
        DWORD available=0;if(!PeekNamedPipe(read.value,nullptr,0,nullptr,&available,nullptr))break;
        if(available){BYTE bytes[4096];DWORD received=0;if(!ReadFile(read.value,bytes,std::min<DWORD>(available,sizeof(bytes)),&received,nullptr)||!received)break;result.insert(result.end(),bytes,bytes+received);if(result.size()>48*48*4)return {};continue;}
        if(WaitForSingleObject(child.value,20)==WAIT_OBJECT_0){DWORD remaining=0;if(!PeekNamedPipe(read.value,nullptr,0,nullptr,&remaining,nullptr)||!remaining)break;}
    }
    WaitForSingleObject(child.value,50);DWORD code=STILL_ACTIVE;GetExitCodeProcess(child.value,&code);
    if(state)*state=code==3?ProgramState::missing:(code==0||code==2||result.size()==48*48*4)?ProgramState::available:ProgramState::unknown;
    return result.size()==48*48*4?result:std::vector<BYTE>{};
}
void requestIcon(const std::string& key,const std::wstring& path,bool folder,bool withIcon=true){
    std::lock_guard<std::mutex> lock(iconMutex);
    if(iconWorkerStopping||!pendingIcons.emplace(key,path).second)return;
    iconRequests.push_back({key,path,folder,iconGeneration.load(),withIcon});
    if(!iconWorkerThread.joinable())iconWorkerThread=std::thread([]{
        for(;;){
            IconRequest request;
            {std::unique_lock<std::mutex> lock(iconMutex);iconCondition.wait(lock,[]{return iconWorkerStopping||!iconRequests.empty();});if(iconWorkerStopping)return;request=std::move(iconRequests.front());iconRequests.pop_front();}
            ProgramState state=ProgramState::unknown;std::vector<BYTE> pixels;try{pixels=fetchIcon(request,&state);}catch(...){}
            {std::lock_guard<std::mutex> lock(iconMutex);if(request.generation!=iconGeneration)continue;iconResults.push_back({request,std::move(pixels),state});}
            PostMessageW(windowHandle,ICON_READY,0,0);
        }
    });
    iconCondition.notify_one();
}
void cancelPendingIcons(){
    std::lock_guard<std::mutex> lock(iconMutex);++iconGeneration;iconRequests.clear();iconResults.clear();pendingIcons.clear();
}
void stopIconWorker(){
    {std::lock_guard<std::mutex> lock(iconMutex);iconWorkerStopping=true;++iconGeneration;iconRequests.clear();iconResults.clear();pendingIcons.clear();}
    iconCondition.notify_one();if(iconWorkerThread.joinable())iconWorkerThread.join();
}
