#pragma once
template<class Test,class Require>void runFullscreenTests(Test test,Require require){
    test("fullscreen geometry excludes maximized windows and accepts spanning displays",[&]{
        const RECT monitor{1920,0,3840,1080};
        require(coversDisplay({1920,0,3840,1080},monitor,false,WS_POPUP));
        require(coversDisplay({0,0,3840,1080},monitor,false,WS_POPUP));
        require(!coversDisplay({1920,0,3840,1040},monitor,true,WS_OVERLAPPEDWINDOW));
        require(!coversDisplay(monitor,monitor,true,WS_OVERLAPPEDWINDOW));
        require(coversDisplay(monitor,monitor,true,WS_POPUP));
        require(!coversDisplay({0,0,1920,1080},monitor,false,WS_POPUP));
        require(invocationPassThrough(true,false)&&!invocationPassThrough(false,false)&&!invocationPassThrough(true,true));
        require(ignoredFullscreenOverlay(WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_TOPMOST));
        require(!ignoredFullscreenOverlay(WS_EX_APPWINDOW|WS_EX_TOPMOST));
    });
    test("fullscreen guard follows the chosen monitor and ordinary windows remain usable",[&]{
        const auto all=displays();require(!all.empty());const auto saved=invocationMonitor.load();const auto target=all.front().handle;invocationMonitor=target;
        struct Child {
            HANDLE process{},thread{};HMONITOR saved;
            ~Child(){invocationMonitor=saved;if(process){TerminateProcess(process,0);WaitForSingleObject(process,1000);CloseHandle(process);}if(thread)CloseHandle(thread);}
        } child{nullptr,nullptr,saved};
        const auto exe=exeDirectory/L"SpiderWeb.TestFixture.exe";auto command=quotedArgument(exe.wstring())+L" --fullscreen-fixture "+std::to_wstring(reinterpret_cast<uintptr_t>(target));
        STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
        require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)!=FALSE);child.process=process.hProcess;child.thread=process.hThread;
        HWND window{};for(int attempt=0;attempt<100&&!window;++attempt){window=FindWindowW(L"FoldBOcean.FullscreenFixture",nullptr);if(!window)Sleep(20);}require(window!=nullptr);
        const auto expect=[&](bool blocked){for(int attempt=0;attempt<50;++attempt){DwmFlush();if(fullscreenBlocksInvocation()==blocked)return;Sleep(20);}require(false);};
        expect(true);SendMessageW(window,WM_APP+1,3,0);expect(false);
        SendMessageW(window,WM_APP+1,0,0);expect(true);
        SendMessageW(window,WM_APP+1,1,0);expect(false);
        SendMessageW(window,WM_APP+1,2,0);expect(false);
        SendMessageW(window,WM_APP+1,0,0);expect(true);
        if(all.size()>1){SendMessageW(window,WM_APP+2,0,reinterpret_cast<LPARAM>(all[1].handle));expect(false);invocationMonitor=all[1].handle;expect(true);}
        PostMessageW(window,WM_CLOSE,0,0);require(WaitForSingleObject(child.process,2000)==WAIT_OBJECT_0);
    });
}
