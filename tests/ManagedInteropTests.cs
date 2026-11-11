using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Globalization;
using EthhhSDK;
using EthhhSDK.Managed;
using EthhhSDK.Modding;

internal static class ManagedInteropTests
{
    [DllImport("EthhhSDK.dll", CallingConvention=CallingConvention.Cdecl)] private static extern void Test_Reset();
    [DllImport("EthhhSDK.dll", CallingConvention=CallingConvention.Cdecl)] private static extern void Test_Set(int index,float value);
    [DllImport("EthhhSDK.dll", CallingConvention=CallingConvention.Cdecl)] private static extern float Test_Get(int index);
    [DllImport("EthhhSDK.dll", CallingConvention=CallingConvention.Cdecl)] private static extern void Test_Tick();
    [DllImport("EthhhSDK.dll", CallingConvention=CallingConvention.Cdecl)] private static extern void Test_Gui();
    [DllImport("EthhhSDK.dll", CallingConvention=CallingConvention.Cdecl)] private static extern void Test_Overlay(int open);
    [DllImport("EthhhSDK.dll", CallingConvention=CallingConvention.Cdecl)] private static extern int EthGame_StratClassName(uint addr,[Out] byte[] buffer,int capacity);
    private static int checks;
    private static void Check(bool ok,string label) { checks++; if(!ok) throw new Exception(label); }
    private static void Tap(int key) { Test_Set(key,1); Test_Tick(); Test_Set(key,0); Test_Tick(); }
    private static void Activate() { Tap(8); }
    private static void Panic() { Tap(9); }
    private static void Standby(string label) { Check(Test_Get(18)==0,label+" capture released"); float n=Test_Get(32); Test_Tick(); Check(Test_Get(32)==n,label+" no player writes"); }

    public static int Main(string[] args)
    {
        try
        {
            Test_Reset();
            Check(IntPtr.Size==4,"x86 CLR");
            Check(Marshal.SizeOf(typeof(CameraInfo))==152,"CameraInfo size");
            Check(Marshal.SizeOf(typeof(StratInfo))==56,"StratInfo size");
            Check(Marshal.OffsetOf(typeof(CameraInfo),"World").ToInt32()==24,"camera world ABI offset");
            Check(Marshal.OffsetOf(typeof(StratInfo),"Class").ToInt32()==44,"strat class ABI offset");
            Check(Marshal.SizeOf(typeof(StratIdentity))==40,"additive identity size");
            Check(Marshal.OffsetOf(typeof(StratIdentity),"Role").ToInt32()==32,"identity role ABI offset");
            Check(Game.GetStratClassName(0x1000)=="Ninja","caller-owned ASCII class name");
            Check(Game.GetStratClassName(0)=="","null class-name address");
            Check(Game.PlayerDetectionInfo=="test: exact Ninja","caller-owned detection information");
            Check(Game.TryReadStratIdentity(0x1000,out StratIdentity identity) && identity.IsExactNinja && identity.SelfHandle==0x123,"identity native/managed roundtrip");
            Check(!Game.TryReadStratIdentity(0,out identity) && identity.Addr==0,"invalid identity cleared");
            var nameBuffer=new byte[]{88,88,88,88,88,88,88,88};
            Check(EthGame_StratClassName(0x1000,nameBuffer,5)==0 && nameBuffer[0]==0 && nameBuffer[5]==88,"short name buffer fails without truncation or overflow");
            Check(EthGame_StratClassName(0x1000,nameBuffer,6)==1 && nameBuffer[5]==0 && nameBuffer[6]==88,"exact-sized NUL-terminated name buffer");
            Check(EthGame_StratClassName(0x1000,nameBuffer,0)==0,"zero capacity name buffer refused");
            Test_Set(21,1);
            Check(Game.GetStratClassName(0x1000)=="" && !Game.TryReadStratIdentity(0x1000,out identity) && identity.SelfHandle==0,"failed name/identity read exposes no stale identity");
            Test_Set(21,0);
            // A saved active flag must not re-enable movement on load.
            ModSettings.SetBool("active",true,"halflife_movement");
            ModSettings.SetFloat("gravity",float.NaN,"halflife_movement");
            float scale=args.Length>0 ? float.Parse(args[0],CultureInfo.InvariantCulture) : .025f;
            ModSettings.SetFloat("world_units_per_hl_unit",scale,"halflife_movement");
            scale=float.IsNaN(scale) || float.IsInfinity(scale) ? .025f : Math.Max(.001f,Math.Min(1,scale));
            string here=AppDomain.CurrentDomain.BaseDirectory;
            int result=Bootstrap.Initialize(here+"\n"+
                "halflife_movement|"+Path.Combine(here,"Mod.dll")+"|"+Path.Combine(here,"hl")+"\n"+
                "fixture|"+Path.Combine(here,"LifecycleFixture.dll")+"|"+Path.Combine(here,"fixture")+"\n");
            Check(result==0 && Test_Get(34)==0,"real host loads real compiled HL mod and fixture");
            var traceSegments=new[]{new TraceSegment(0,1,0,0,-1,0),new TraceSegment(0,1,0,10,1,0)};
            var traceHits=new TraceHit[2];
            Check(Game.TraceSegments(0x1000,traceSegments,traceHits) && traceHits[0].HasHit && Math.Abs(traceHits[0].NormalY-1)<.001,
                "batched native ground trace contract");
            Check(traceHits[1].HasHit && Math.Abs(traceHits[1].PointX-5)<.001 && traceHits[1].NormalX<-.9,
                "batched native wall trace contract");
            Test_Tick(); Standby("initial standby"); Check(LifecycleFixture.Ticks>0,"native-to-managed rooted tick delegate");
            Test_Set(3,0); Activate(); Standby("auto-detected player refused"); Test_Set(3,1);
            foreach (int role in new[]{0,2,3,4}) {
                Test_Set(19,role); Activate(); Standby("non-Ninja role "+role+" refused");
            }
            Test_Set(19,1); Test_Set(21,1); Activate(); Standby("unreadable identity refused"); Test_Set(21,0);
            Activate(); Check(Test_Get(18)==1,"activation requests capture"); Check(Test_Get(22)==1,"HL camera claims render ownership"); Check(Test_Get(33)>0,"camera callback");
            Check(Math.Abs(Test_Get(37)-90)<.001,"FOV override");
            Check(Math.Abs(Test_Get(39)-Test_Get(15)-69*scale)<.001,"camera eye height converted to game units");
            Check(Math.Abs(Test_Get(40)-Test_Get(17)-24*scale)<.001,"camera forward offset converted to game units");
            Test_Set(10,1); Test_Tick(); Check(Test_Get(17)>0,"W moves in assumed +Z basis"); Check(Test_Get(24)>0,"Ninja facing follows view yaw"); Test_Set(10,0);
            Test_Set(7,1); Test_Tick(); Test_Set(7,0);
            float baseY=0, maxY=Test_Get(15);
            for(int i=0;i<55;i++) { Test_Tick(); maxY=Math.Max(maxY,Test_Get(15)); }
            Check(Math.Abs(maxY-45*scale)<.002,"actual mod jump apex is anchored and converted to game units");
            Check(Math.Abs(Test_Get(15)-baseY)<.001,"actual mod returns to takeoff height");
            Test_Set(7,1);
            for(int i=0;i<150;i++) Test_Tick();
            Check(Math.Abs(Test_Get(15)-baseY)<.001,"holding jump does not accumulate repeated jumps");
            Test_Set(7,0); Test_Tick();
            Panic(); Test_Set(15,2); Activate();
            Test_Set(7,1); Test_Tick(); Test_Set(7,0); maxY=Test_Get(15);
            for(int i=0;i<55;i++) { Test_Tick(); maxY=Math.Max(maxY,Test_Get(15)); }
            Check(Math.Abs(maxY-(2+45*scale))<.002,"nonzero takeoff height remains anchored");
            Check(Math.Abs(Test_Get(15)-2)<.001,"jump returns to nonzero takeoff height");
            Test_Set(6,0); float pausedWrites=Test_Get(32), pausedCamera=Test_Get(33);
            Test_Tick(); Check(Test_Get(32)==pausedWrites && Test_Get(33)==pausedCamera,"zero time step suspends all movement/camera writes");
            Test_Set(6,1.0f/60);
            Test_Overlay(1); float n=Test_Get(32); Test_Tick(); Check(Test_Get(32)==n,"overlay suspends movement");
            Check(Test_Get(18)==0,"overlay releases capture request");
            Panic(); Standby("F3 works with overlay open"); Check(Test_Get(22)==0,"panic releases render camera ownership"); Check(Math.Abs(Test_Get(37)-60)<.001,"panic restores FOV"); Test_Overlay(0);

            Activate(); Test_Set(0,0); Test_Tick(); Standby("disabled mod");
            int disabled=LifecycleFixture.Disabled, ticks=LifecycleFixture.Ticks;
            Test_Tick(); Test_Gui(); Test_Overlay(1);
            Check(LifecycleFixture.Disabled==disabled && disabled==1,"disable lifecycle delivered only once");
            Check(LifecycleFixture.Ticks==ticks,"disabled callback gating");
            Test_Set(0,1); Test_Overlay(0); Test_Tick(); Standby("re-enabled starts inactive");
            Check(LifecycleFixture.Enabled==1,"re-enable lifecycle delivered once");

            Activate(); Test_Set(11,0x1100); Test_Tick(); Standby("tracked player address change"); Test_Set(11,0x1000);
            Activate(); Test_Set(12,0x2200); Test_Tick(); Standby("strat class reuse"); Test_Set(12,0x2000);
            Activate(); Test_Set(20,0x124); Test_Tick(); Standby("same-class handle generation reuse"); Test_Set(20,0x123);
            Activate(); Test_Set(19,2); Test_Tick(); Standby("identity changes to attachment"); Test_Set(19,1);
            Activate(); Test_Set(21,1); Test_Tick(); Standby("identity read failure while active"); Test_Set(21,0);
            Activate(); Test_Set(6,float.NaN); Test_Tick(); Standby("NaN time step"); Test_Set(6,1.0f/60);
            Activate(); Test_Set(6,float.PositiveInfinity); Test_Tick(); Standby("infinite time step"); Test_Set(6,1.0f/60);
            Activate(); float traversalWrites=Test_Get(32); Test_Set(15,400); Test_Tick();
            Check(Test_Get(18)==1 && Test_Get(32)==traversalWrites,"native traversal/respawn displacement yields without writing or mode toggle");
            Test_Set(15,0); Panic(); Standby("native traversal panic cleanup");
            Activate(); Test_Set(13,1); Test_Tick(); Standby("failed position write"); Test_Set(13,0);
            Activate(); Test_Set(14,1); Test_Tick(); Standby("failed camera write"); Test_Set(14,0);
            Activate(); Test_Set(4,0); for(int i=0;i<30;i++) Test_Tick();
            Check(Test_Get(4)==1 && Test_Get(18)==1,"unique exact Ninja reacquired without mode toggle after respawn"); Panic(); Standby("reacquire cleanup");
            Activate(); Test_Set(25,0); Test_Set(4,0);
            for(int i=0;i<180;i++) Test_Tick();
            Standby("lost player after bounded exact-reacquire wait"); Test_Set(25,1); Test_Set(4,1);
            Test_Set(5,0); Activate(); Check(Test_Get(18)==1,"unfocused capture request retained");
            Panic(); Standby("panic cancels suspended capture"); Test_Set(5,1);

            Test_Gui(); Check(LifecycleFixture.Guis>0,"native-to-managed GUI delegate");
            Check(Test_Get(35)==0 && Test_Get(36)==0,"balanced host GUI stacks");
            LifecycleFixture.ThrowGui=true; Test_Gui();
            Check(Test_Get(34)==1,"GUI exception reported");
            Check(Test_Get(35)==0 && Test_Get(36)==0,"host-owned stacks cleaned after exception");
            ticks=LifecycleFixture.Ticks; Test_Tick(); Check(LifecycleFixture.Ticks==ticks,"errored fixture stays disabled");
            Check(ModContext.ModId=="" && ModContext.ModDir=="","callback context cleared");
            Console.WriteLine("PASS managed interop: "+checks+" assertions, scale "+scale+" (fake engine; no live collision/camera verification)");
            return 0;
        }
        catch(Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }
}
