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
    private static void Activate() { Test_Set(52,1); Tap(8); } // Fake native camera resumes behind the player.
    private static void Panic() { Tap(9); }
    private static void Standby(string label) { Check(Test_Get(18)==0,label+" capture released"); float n=Test_Get(32); Test_Tick(); Check(Test_Get(32)==n,label+" no player writes"); }

    private static void MovementDirections(float scale)
    {
        Panic(); Test_Set(48,0);
        foreach(float yaw in new[]{0,45,90,135,180,225,270,315}) {
            Test_Set(15,0); Test_Set(16,0); Test_Set(17,0); Test_Set(52,1);
            float r=yaw*(float)Math.PI/180;
            Test_Set(54,-100*(float)Math.Sin(r)); Test_Set(56,-100*(float)Math.Cos(r));
            Tap(8); // Keep this deliberately non-canonical starting camera.
            float fx=Test_Get(72),fz=Test_Get(74),len=(float)Math.Sqrt(fx*fx+fz*fz);
            fx/=len; fz/=len;
            foreach(int key in new[]{10,28,29,30}) {
                // Native keyboard walk points perpendicular to the visible view.
                Test_Set(59,3*scale*fz); Test_Set(60,-3*scale*fx);
                float x=Test_Get(16),z=Test_Get(17); Test_Set(key,1);
                for(int i=0;i<12;i++) Test_Tick();
                float dx=Test_Get(16)-x,dz=Test_Get(17)-z;
                float along=dx*fx+dz*fz,side=dx*fz-dz*fx;
                Check(key==10 ? along>scale && Math.Abs(side)<.002f*scale :
                      key==28 ? along<-scale && Math.Abs(side)<.002f*scale :
                      key==29 ? side<-scale && Math.Abs(along)<.002f*scale :
                                side>scale && Math.Abs(along)<.002f*scale,
                      "actual camera-relative key "+key+" rejects perpendicular native walk at yaw "+yaw);
                Check(Test_Get(61)==1,"HL replacement owns native WASD at yaw "+yaw);
                Check(Input.KeyDown(key==10 ? Keys.W : key==28 ? Keys.S : key==29 ? Keys.A : Keys.D),"native mask leaves SDK raw key state readable");
                Test_Set(key,0); Test_Set(59,0); Test_Set(60,0);
                Panic(); Test_Set(15,0); Test_Set(16,0); Test_Set(17,0);
                Test_Set(52,1); Test_Set(54,-100*(float)Math.Sin(r)); Test_Set(56,-100*(float)Math.Cos(r)); Tap(8);
            }
            // Mouse yaw changes must change the movement basis as well as pose.
            Test_Set(57,3000); Test_Tick();
            fx=Test_Get(72);fz=Test_Get(74);len=(float)Math.Sqrt(fx*fx+fz*fz);fx/=len;fz/=len;
            float mx=Test_Get(16),mz=Test_Get(17);Test_Set(10,1);
            Test_Set(59,3*scale*fz);Test_Set(60,-3*scale*fx);
            for(int i=0;i<12;i++)Test_Tick();
            float vx=Test_Get(16)-mx,vz=Test_Get(17)-mz;
            Check(vx*fx+vz*fz>scale && Math.Abs(vx*fz-vz*fx)<.002f*scale,"mouse turn keeps W aligned with rendered camera at yaw "+yaw);
            Test_Set(10,0);Test_Set(59,0);Test_Set(60,0);Panic();
        }
        Test_Set(48,1);Test_Set(52,1);
    }

    private static void TraversalHandoff(float scale,bool autoHop)
    {
        Panic();Test_Set(48,0);Test_Set(15,0);Test_Set(16,0);Test_Set(17,0);Activate();
        Test_Set(27,1);Test_Set(10,1);Test_Set(59,3*scale);
        float n=Test_Get(32);
        for(int i=0;i<40;i++) { Test_Tick(); Check(Test_Get(32)==n && Test_Get(61)==0,"held Ctrl keeps native traversal authority"); }
        Test_Set(27,0);Test_Set(7,1);float maxY=0;
        for(int i=0;i<45;i++) { Test_Tick();maxY=Math.Max(maxY,Test_Get(15)); }
        Check(Test_Get(32)>n,"continued native walking cannot perpetually refresh traversal grace");
        if(autoHop) Check(maxY>10*scale,"held Auto-hop resumes after supported chain/rail exit while walking");
        float exitX=Test_Get(16);for(int i=0;i<6;i++)Test_Tick();
        Check(Math.Abs(Test_Get(16)-exitX)<.0001f,"post-traversal W cannot retain perpendicular native keyboard drift");
        Test_Set(7,0);for(int i=0;i<90;i++)Test_Tick();
        Test_Set(7,1);Test_Tick();float y=Test_Get(15);Test_Tick();
        Check(y>scale && Test_Get(15)>y+scale,"fresh HL jump rises after Ctrl exit without toggling mode");
        Test_Set(7,0);Test_Set(10,0);Test_Set(59,0);Panic();

        // Unsupported rail motion has a hard timeout even while translating.
        Test_Set(15,0);Activate();Test_Set(27,1);Test_Tick();Test_Set(42,20*scale);Test_Tick();
        Test_Set(27,0);Test_Set(10,1);Test_Set(59,3*scale);Test_Set(7,1);n=Test_Get(32);
        for(int i=0;i<29;i++) {Test_Tick();Check(Test_Get(32)==n,"unsupported traversal grace preserves native motion before expiry");}
        for(int i=0;i<3;i++)Test_Tick();
        Check(Test_Get(32)>n && Math.Abs(Test_Get(15)-20*scale)<.0001f,"rail timeout resumes horizontal control without an airborne HL impulse");
        Test_Set(7,0);Test_Set(42,0);Test_Tick();Test_Set(7,1);Test_Tick();y=Test_Get(15);Test_Tick();
        Check(y>scale && Test_Get(15)>y+scale,"HL jump returns after queried landing from rail");
        Test_Set(7,0);Test_Set(10,0);Test_Set(59,0);Panic();Test_Set(15,0);Test_Set(16,0);Test_Set(17,0);Test_Set(48,1);Activate();
    }

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
            Test_Set(26,0); Test_Set(4,0);
            Check(!Game.PlayerReacquireExactNinja() && Test_Get(4)==0, "native reacquire contract refuses missing prior pin");
            Test_Set(26,1); Test_Set(3,0);
            Check(!Game.PlayerReacquireExactNinja() && Test_Get(3)==0 && Test_Get(4)==0, "native reacquire contract cannot convert auto mode into a manual pin");
            Test_Set(3,1); Test_Set(19,2);
            Check(!Game.PlayerReacquireExactNinja(), "native reacquire contract refuses attachment");
            Test_Set(19,1); Test_Set(4,1);
            // A saved active flag must not re-enable movement on load.
            ModSettings.SetBool("active",true,"halflife_movement");
            ModSettings.SetFloat("gravity",float.NaN,"halflife_movement");
            float scale=args.Length>0 ? float.Parse(args[0],CultureInfo.InvariantCulture) : .025f;
            ModSettings.SetFloat("world_units_per_hl_unit",scale,"halflife_movement");
            scale=float.IsNaN(scale) || float.IsInfinity(scale) ? .025f : Math.Max(.001f,Math.Min(1,scale));
            bool liveTuning=args.Length>1 && args[1]=="live-tuning";
            bool currentTuning=args.Length>1 && args[1]=="current-tuning";
            bool additive=args.Length>1 && args[1]=="additive";
            bool cameraOnly=args.Length>1 && args[1]=="camera-only";
            ModSettings.SetBool("replace_movement",!additive,"halflife_movement");
            ModSettings.SetBool("movement",!cameraOnly,"halflife_movement");
            bool autoHop=args.Length>1 && (args[1]=="autohop" || liveTuning || currentTuning);
            ModSettings.SetBool("autohop",autoHop,"halflife_movement");
            ModSettings.SetString("jump_key","X","halflife_movement");
            ModSettings.SetFloat("game_vertical_threshold",20,"halflife_movement");
            float expectedApex=45;
            if(liveTuning) {
                ModSettings.SetFloat("gravity",598,"halflife_movement");
                ModSettings.SetFloat("jumpspeed",268.33f,"halflife_movement");
                ModSettings.SetFloat("airaccelerate",29.7f,"halflife_movement");
                expectedApex=268.33f*268.33f/(2*598);
            }
            if(currentTuning) {
                // Read-only snapshot of the user's latest tuning, not INI writes.
                ModSettings.SetFloat("gravity",531,"halflife_movement");
                ModSettings.SetFloat("jumpspeed",268.33f,"halflife_movement");
                ModSettings.SetFloat("airaccelerate",40,"halflife_movement");
                ModSettings.SetFloat("game_vertical_threshold",134,"halflife_movement");
                ModSettings.SetFloat("eye_height",101,"halflife_movement");
                ModSettings.SetFloat("forward_offset",5,"halflife_movement");
                ModSettings.SetFloat("fov",130,"halflife_movement");
                expectedApex=268.33f*268.33f/(2*531);
            }
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
            Check(Math.Abs(Test_Get(37)-(currentTuning?130:90))<.001,"FOV override");
            Check(Math.Abs(Test_Get(39)-Test_Get(15)-(currentTuning?101:69)*scale)<.001,"camera eye height converted to game units");
            Check(Math.Abs(Test_Get(40)-Test_Get(17)-(currentTuning?5:24)*scale)<.001,"camera forward offset converted to game units");
            if(additive || cameraOnly) {
                Test_Set(48,0);Test_Set(59,3*scale);Test_Set(10,1);float x=Test_Get(16),modeWrites=Test_Get(32);
                for(int i=0;i<12;i++)Test_Tick();
                Check(Test_Get(61)==0 && Test_Get(16)>x+30*scale,"additive/camera-only never masks native WASD");
                Check(cameraOnly ? Test_Get(32)==modeWrites : Test_Get(32)>modeWrites,"movement writes respect camera-only/additive mode");
                Test_Set(10,0);Panic();Console.WriteLine("PASS "+args[1]+": "+checks+" assertions");return 0;
            }
            if(args.Length>1 && args[1]=="direction-only") { MovementDirections(scale);Console.WriteLine("PASS direction-only: "+checks+" assertions");return 0; }
            if(args.Length>1 && args[1]=="traversal-only") { TraversalHandoff(scale,autoHop);Console.WriteLine("PASS traversal-only: "+checks+" assertions");return 0; }
            Test_Set(10,1); Test_Tick(); Check(Test_Get(17)>0,"W moves in assumed +Z basis"); Check(Test_Get(24)>0,"Ninja facing follows view yaw"); Test_Set(10,0);
            Test_Set(7,1); Test_Tick(); Test_Set(7,0);
            float baseY=0, maxY=Test_Get(15);
            for(int i=0;i<90;i++) { Test_Tick(); maxY=Math.Max(maxY,Test_Get(15)); }
            Check(Math.Abs(maxY-expectedApex*scale)<.002,"actual mod jump apex is anchored and converted to game units");
            Check(Math.Abs(Test_Get(15)-baseY)<.001,"actual mod returns to takeoff height");
            Test_Set(7,1);
            float priorY=Test_Get(15); maxY=priorY;
            for(int i=0;i<150;i++) {
                Test_Tick(); float y=Test_Get(15); maxY=Math.Max(maxY,y);
                if(i<8) Check(y>priorY+scale,"holding jump rises on every takeoff frame (Auto-hop="+autoHop+")");
                priorY=y;
            }
            Check(maxY<=expectedApex*scale+.002,"holding jump does not accumulate unbounded height");
            if(!autoHop) Check(Math.Abs(Test_Get(15)-baseY)<.001,"holding jump does not accumulate repeated jumps");
            Test_Set(7,0); Test_Tick();
            Panic(); Test_Set(15,2); Activate();
            Test_Set(7,1); Test_Tick(); Test_Set(7,0); maxY=Test_Get(15);
            for(int i=0;i<90;i++) { Test_Tick(); maxY=Math.Max(maxY,Test_Get(15)); }
            Check(Math.Abs(maxY-(2+expectedApex*scale))<.002,"nonzero takeoff height remains anchored");
            Check(Math.Abs(Test_Get(15)-2)<.001,"jump returns to nonzero takeoff height");

            Panic(); Test_Set(15,0); Test_Set(16,0); Test_Set(17,0); Activate();
            Test_Set(7,1); Test_Tick(); priorY=Test_Get(15); maxY=priorY;
            for(int i=0;i<80;i++) {
                // Simulate a bounded native downward correction before the
                // post-tick adapter, independent of its own previous writes.
                Test_Set(42,Math.Max(0,Test_Get(15)-scale)); Test_Tick();
                if(i<7) Check(Test_Get(15)>priorY+scale,"held jump rises despite native near-ground correction");
                maxY=Math.Max(maxY,Test_Get(15)); priorY=Test_Get(15);
                if(i==7) Test_Set(7,0);
            }
            Check(Math.Abs(maxY-expectedApex*scale)<.002 && Math.Abs(Test_Get(15))<.0001,"HL flight absorbs bounded native gravity without rebasing/rejumping");

            Panic(); Test_Set(15,0); Test_Set(16,0); Test_Set(17,0); Activate();
            Test_Set(7,1);
            foreach(float height in new[]{.4f,.8f,1.2f,1.5f,4,10,20}) {
                Test_Set(42,height*scale); Test_Tick();
                Check(Math.Abs(Test_Get(15)-height*scale)<.0001,"native takeoff not ground-snapped or given an HL impulse at "+height+" HL units");
            }
            Test_Set(42,1.5f*scale); Test_Tick(); Test_Tick();
            Check(Math.Abs(Test_Get(15)-1.5f*scale)<.0001,"native flight latch survives a near-floor pause");
            Test_Set(7,0); Test_Tick(); Check(Math.Abs(Test_Get(15)-1.5f*scale)<.0001,"release does not launch or snap native jump");
            Test_Set(42,0); Test_Tick(); Check(Math.Abs(Test_Get(15))<.0001,"native flight hands back on landing");

            Panic(); Test_Set(15,0); Test_Set(42,scale); Activate(); Test_Set(7,1); Test_Tick();
            Check(Math.Abs(Test_Get(15)-scale)<.0001,"activation near support cannot grant airborne jump permission"); Test_Set(7,0);
            Panic(); Test_Set(15,0); Activate(); Test_Set(7,1);
            Test_Set(42,3*scale); Test_Tick(); Check(Math.Abs(Test_Get(15)-3*scale)<.0001,"short fall cannot start an HL jump above the floor");
            Test_Set(42,scale); Test_Tick(); Check(Math.Abs(Test_Get(15)-scale)<.0001,"fast descending near-support frame is not snapped/rejumped");
            Test_Set(7,0); Test_Set(42,0); Test_Tick();

            Panic(); Test_Set(15,0); Activate(); Test_Set(7,1); Test_Tick(); Test_Set(7,0);
            for(int i=0;i<10;i++) Test_Tick();
            Test_Set(41,-20*scale); bool crossedTakeoff=false;
            for(int i=0;i<100;i++) { Test_Tick(); if(Test_Get(15)<-scale) crossedTakeoff=true; }
            Check(crossedTakeoff,"HL flight descends below takeoff without an imaginary floor");
            Check(Math.Abs(Test_Get(15)+20*scale)<.0001,"HL flight lands on a lower queried platform: y="+Test_Get(15)+", floor="+Test_Get(41)+", capture="+Test_Get(18));

            Panic(); Test_Set(15,0); Test_Set(44,80*scale); Activate();
            Test_Set(7,1); Test_Tick(); Test_Set(7,0); maxY=Test_Get(15);
            for(int i=0;i<90;i++) { Test_Tick(); maxY=Math.Max(maxY,Test_Get(15)); }
            Check(maxY<=7.5f*scale+.0001,"real mod ceiling sweep stops the head");
            Check(Math.Abs(Test_Get(15))<.0001,"real mod falls and lands after ceiling contact");
            Test_Set(44,float.PositiveInfinity);

            Panic(); Test_Set(15,0); Test_Set(16,0); Test_Set(17,0); Test_Set(43,40*scale); Activate();
            Test_Set(30,1); Test_Set(10,1);
            for(int i=0;i<100;i++) Test_Tick();
            Check(Test_Get(16)<=28*scale+.0001 && Test_Get(17)>45*scale,"real mod keeps leading radius out of wall while sliding: "+Test_Get(16)+", "+Test_Get(17));
            Test_Set(16,24*scale); Test_Tick();
            Check(Test_Get(16)<=28*scale+.0001,"native wall push-out preserved, never rewound to old pose");
            Test_Set(30,0); Test_Set(10,0); Test_Set(29,1);
            float contactX=Test_Get(16); for(int i=0;i<30;i++) Test_Tick();
            Check(Test_Get(16)<contactX-scale,"real mod can retreat from a wall"); Test_Set(29,0); Test_Set(43,5);

            Panic(); Test_Set(15,0); Test_Set(16,0); Test_Set(17,0); Activate(); Test_Set(7,1); Test_Tick(); Test_Set(7,0);
            float nativeCorrection=Test_Get(15)-20*scale; Test_Set(42,nativeCorrection); Test_Tick();
            Check(Math.Abs(Test_Get(15)-nativeCorrection)<.0001,"large native vertical correction cancels stale HL flight");

            Panic(); Test_Set(15,0); Activate(); float beforeFailure=Test_Get(32); Test_Set(46,1); Test_Tick();
            Check(Test_Get(32)==beforeFailure,"failed support queries commit no player position"); Standby("failed trace cleanup"); Test_Set(46,0);
            Activate(); beforeFailure=Test_Get(32); Test_Set(47,1); Test_Tick();
            Check(Test_Get(32)==beforeFailure,"nonfinite support hit commits no player position"); Standby("invalid trace cleanup"); Test_Set(47,0);
            // Order per active tick: ground support, 16-ray wall probe, body
            // sweep, step support, vertical, final support. The wall probe adds
            // one batch before the first movement query.
            Activate(); beforeFailure=Test_Get(32); Test_Set(30,1); Test_Set(53,3); Test_Tick();
            Check(Test_Get(32)==beforeFailure,"failed body query commits no player position"); Standby("body trace cleanup"); Test_Set(30,0);
            Activate(); beforeFailure=Test_Get(32); Test_Set(7,1); Test_Set(53,3); Test_Tick();
            Check(Test_Get(32)==beforeFailure,"failed vertical query commits no player position"); Standby("vertical trace cleanup"); Test_Set(7,0);
            Activate(); beforeFailure=Test_Get(32); Test_Set(7,1); Test_Set(53,4); Test_Tick();
            Check(Test_Get(32)==beforeFailure,"failed final support query commits no partially resolved player position"); Standby("final trace cleanup"); Test_Set(7,0); Test_Set(53,0);
            Check(Test_Get(51)<=16,"runtime ray batches remain within the native 16-ray limit");
            Test_Set(15,2); Activate();
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
            MovementDirections(scale); TraversalHandoff(scale,autoHop);
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
            Console.WriteLine("PASS managed interop: "+checks+" assertions, scale "+scale+", Auto-hop="+autoHop+", live-tuning="+liveTuning+", current-tuning="+currentTuning+" (fake engine; no live collision/camera verification)");
            return 0;
        }
        catch(Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }
}
