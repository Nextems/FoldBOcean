using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public sealed class LauncherCounters : IDisposable {
    [DllImport("pdh.dll", CharSet=CharSet.Unicode)] static extern uint PdhOpenQuery(string source, IntPtr user, out IntPtr query);
    [DllImport("pdh.dll", CharSet=CharSet.Unicode)] static extern uint PdhAddEnglishCounter(IntPtr query,string path,IntPtr user,out IntPtr counter);
    [DllImport("pdh.dll")] static extern uint PdhCollectQueryData(IntPtr query);
    [DllImport("pdh.dll")] static extern uint PdhGetFormattedCounterValue(IntPtr counter,uint format,out uint type,out Value value);
    [DllImport("pdh.dll",CharSet=CharSet.Unicode)] static extern uint PdhGetFormattedCounterArray(IntPtr counter,uint format,ref uint bytes,ref uint count,IntPtr items);
    [DllImport("pdh.dll")] static extern uint PdhCloseQuery(IntPtr query);
    [StructLayout(LayoutKind.Explicit,Size=16)] struct Value { [FieldOffset(0)] public uint Status; [FieldOffset(8)] public double Number; }
    [StructLayout(LayoutKind.Sequential)] struct Item { public IntPtr Name; public Value Value; }
    IntPtr query; readonly Dictionary<string,IntPtr> counters=new Dictionary<string,IntPtr>();
    public LauncherCounters() {
        Check(PdhOpenQuery(null,IntPtr.Zero,out query));
        Add("interruptsPerSec",@"\Processor(_Total)\Interrupts/sec");
        Add("interruptPercent",@"\Processor(_Total)\% Interrupt Time");
        Add("dpcPercent",@"\Processor(_Total)\% DPC Time");
        Add("systemCpuPercent",@"\Processor(_Total)\% Processor Time");
        Add("systemContextSwitchesPerSec",@"\System\Context Switches/sec");
        Add("launcherContextSwitchesPerSec",@"\Thread(FoldBOcean/*)\Context Switches/sec");
        Collect();
    }
    void Check(uint error) { if(error!=0)throw new Exception("PDH: 0x"+error.ToString("X")); }
    void Add(string name,string path){IntPtr counter;Check(PdhAddEnglishCounter(query,path,IntPtr.Zero,out counter));counters.Add(name,counter);}
    public void Collect(){Check(PdhCollectQueryData(query));}
    public Dictionary<string,double?> Read(){
        var result=new Dictionary<string,double?>();
        foreach(var pair in counters){
            if(pair.Key=="launcherContextSwitchesPerSec"){
                uint bytes=0,count=0;PdhGetFormattedCounterArray(pair.Value,0x200,ref bytes,ref count,IntPtr.Zero);
                if(bytes==0){result[pair.Key]=null;continue;}
                var memory=Marshal.AllocHGlobal((int)bytes);
                try {var status=PdhGetFormattedCounterArray(pair.Value,0x200,ref bytes,ref count,memory);double sum=0;bool valid=false;
                    if(status==0)for(int i=0;i<count;i++){var item=(Item)Marshal.PtrToStructure(IntPtr.Add(memory,i*Marshal.SizeOf(typeof(Item))),typeof(Item));var name=Marshal.PtrToStringUni(item.Name);if(name.StartsWith("FoldBOcean/",StringComparison.OrdinalIgnoreCase)&&item.Value.Status<=1){sum+=item.Value.Number;valid=true;}}
                    result[pair.Key]=valid?(double?)sum:null;
                }finally{Marshal.FreeHGlobal(memory);}
            }else{uint type;Value value;var status=PdhGetFormattedCounterValue(pair.Value,0x200,out type,out value);result[pair.Key]=status==0&&value.Status<=1?(double?)value.Number:null;}
        }return result;
    }
    public void Dispose(){if(query!=IntPtr.Zero){PdhCloseQuery(query);query=IntPtr.Zero;}}
}
public static class LauncherProbe {
    static volatile bool sweeping;static System.Threading.Thread sweep;
    [StructLayout(LayoutKind.Sequential)] struct MouseInput {public int X,Y;public uint Data,Flags,Time;public IntPtr Extra;}
    [StructLayout(LayoutKind.Sequential)] struct KeyboardInput {public ushort Key,Scan;public uint Flags,Time;public IntPtr Extra;}
    [StructLayout(LayoutKind.Explicit,Size=40)] struct Input {[FieldOffset(0)] public uint Type;[FieldOffset(8)] public MouseInput Mouse;[FieldOffset(8)] public KeyboardInput Keyboard;}
    [DllImport("user32.dll")] static extern uint SendInput(uint count,Input[] inputs,int size);
    public static void Key(ushort key,bool up){var input=new Input[1];input[0].Type=1;input[0].Keyboard.Key=key;input[0].Keyboard.Flags=up?2u:0u;if(SendInput(1,input,40)!=1)throw new Exception("Keyboard SendInput failed");}
    public static void MiddleButton(bool up){var input=new Input[1];input[0].Mouse.Flags=up?0x40u:0x20u;if(SendInput(1,input,40)!=1)throw new Exception("Mouse SendInput failed");}
    public static void BeginSweep(bool injected){Point origin;GetCursorPos(out origin);sweeping=true;sweep=new System.Threading.Thread(()=>{int i=0;var input=new Input[1];input[0].Mouse.Flags=1;while(sweeping){if(injected){input[0].Mouse.X=(i++%2==0)?1:-1;SendInput(1,input,Marshal.SizeOf(typeof(Input)));}else SetCursorPos(origin.X+(i++%80),origin.Y);System.Threading.Thread.Sleep(10);}});sweep.IsBackground=true;sweep.Start();}
    public static void EndSweep(){sweeping=false;if(sweep!=null)sweep.Join();sweep=null;}
    delegate bool WindowCallback(IntPtr window,IntPtr param);
    [DllImport("user32.dll")] static extern bool EnumWindows(WindowCallback callback,IntPtr param);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window,System.Text.StringBuilder name,int size);
    public static int ToolCount(int pid){int count=-1;EnumWindows((window,param)=>{uint owner;GetWindowThreadProcessId(window,out owner);if(owner==pid){var name=new System.Text.StringBuilder(128);GetClassName(window,name,128);if(name.ToString()=="tooltips_class32"){IntPtr value;SendMessageTimeout(window,1037,IntPtr.Zero,IntPtr.Zero,2,1000,out value);count=value.ToInt32();return false;}}return true;},IntPtr.Zero);return count;}
    [StructLayout(LayoutKind.Sequential)] public struct Rect {public int Left,Top,Right,Bottom;}
    [StructLayout(LayoutKind.Sequential)] public struct Point {public int X,Y;}
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr window,uint message,IntPtr w,IntPtr l,uint flags,uint timeout,out IntPtr result);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect rect);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
    [DllImport("user32.dll")] public static extern uint GetGuiResources(IntPtr process,uint flags);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr context);
}
