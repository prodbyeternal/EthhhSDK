// ===========================================================================
//  EthhhSDK.Managed - native interop layer (P/Invoke into EthhhSDK.dll)
// ===========================================================================
using System;
using System.Runtime.InteropServices;

namespace EthhhSDK
{
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct CameraInfo
    {
        public uint Addr;
        public float X, Y, Z;
        public float Fov;          // radians
        public float Param104;
        public fixed float World[16];
        public fixed float View[16];
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct StratInfo
    {
        public uint Addr;
        public fixed float Pos[4];
        public fixed float Quat[4];
        public uint Flags;
        public uint TypeMask;
        public uint Class;
        public uint Next;
        public int Valid;

        public bool IsValid => Valid != 0;
    }

    public enum StratRole { Unknown = 0, Ninja = 1, Attachment = 2, Related = 3, Other = 4 }

    // New separate struct; StratInfo stays 56 bytes for existing native mods.
    [StructLayout(LayoutKind.Sequential)]
    public struct StratIdentity
    {
        public uint Addr, Class, SelfHandle;
        public uint RawOwnerHandle, OwnerAddr;
        public uint Model, Animation, NodeMatrices;
        public StratRole Role;
        public int OwnerFieldKnown;
        public bool IsExactNinja => Role == StratRole.Ninja;
        public bool HasResolvedOwner => OwnerFieldKnown != 0 && OwnerAddr != 0;
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct TraceSegment
    {
        public fixed float Start[3];
        public fixed float End[3];
        public TraceSegment(float sx,float sy,float sz,float ex,float ey,float ez)
        {
            this=default;
            fixed(float* a=Start) fixed(float* b=End) { a[0]=sx;a[1]=sy;a[2]=sz;b[0]=ex;b[1]=ey;b[2]=ez; }
        }
        public float StartX { get { fixed(float* p=Start) return p[0]; } }
        public float StartY { get { fixed(float* p=Start) return p[1]; } }
        public float StartZ { get { fixed(float* p=Start) return p[2]; } }
        public float EndX { get { fixed(float* p=End) return p[0]; } }
        public float EndY { get { fixed(float* p=End) return p[1]; } }
        public float EndZ { get { fixed(float* p=End) return p[2]; } }
    }

    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct TraceHit
    {
        public int Hit;
        public fixed float Point[3];
        public fixed float Normal[3];
        public uint CollisionObject;
        public uint Metadata;
        public bool HasHit => Hit != 0;
        public float PointX { get { fixed(float* p=Point) return p[0]; } }
        public float PointY { get { fixed(float* p=Point) return p[1]; } }
        public float PointZ { get { fixed(float* p=Point) return p[2]; } }
        public float NormalX { get { fixed(float* p=Normal) return p[0]; } }
        public float NormalY { get { fixed(float* p=Normal) return p[1]; } }
        public float NormalZ { get { fixed(float* p=Normal) return p[2]; } }
    }

    internal static class Native
    {
        internal const string Dll = "EthhhSDK.dll";

        // ------------------------------------------------------------------ sdk
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr EthSDK_Version();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr EthSDK_BuildDate();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr EthSDK_GameDir();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr EthSDK_ModsDir();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr EthSDK_ConfigDir();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthLog_Write(int level, string message);

        // ----------------------------------------------------------------- game
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_IsReady();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint EthGame_TickCount();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint EthGame_FrameCount();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern float EthGame_Time();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern float EthGame_TimeStep();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern float EthGame_TimeScale();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthGame_SetTimeScale(float value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_DebugFlags();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthGame_SetDebugFlags(int flags);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr EthGame_Hwnd();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr EthGame_Device();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern IntPtr EthGame_Renderer();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthGame_UpdateView();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_StatInt(int index, out int value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_SetStatInt(int index, int value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_StatBit(int index);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_SetStatBit(int index, int value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint EthGame_NativeAddr(int index);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_NativeCount();

        // --------------------------------------------------------------- camera
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_ReadCamera(out CameraInfo camera);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_SetCameraPos(float x, float y, float z);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_SetCameraWorld(float[] world16);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_SetCameraFovDegrees(float degrees);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern float EthGame_CameraFovDegrees();

        // ---------------------------------------------------------------- strats
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_StratCount();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_SnapshotStrats([Out] StratInfo[] outStrats, int maxCount);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_ReadStrat(uint addr, out StratInfo strat);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_WriteStratPos(uint addr, float x, float y, float z);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_StratClassName(uint addr, [Out] byte[] buffer, int size);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_ReadStratIdentity(uint addr, out StratIdentity identity);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_TraceSegments(uint exactNinja, [In] TraceSegment[] segments, int count, [Out] TraceHit[] hits);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthPatch_SetExternalCameraOwned(int owned);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_SetNinjaFacing(uint exactNinja, float yawRadians);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_PlayerDetectionInfo([Out] byte[] buffer, int size);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint EthGame_PlayerAddr();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_PlayerMode();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthGame_PlayerSetMode(int mode);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthGame_PlayerSelect(uint addr);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthGame_PlayerLearn();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_PlayerReacquireExactNinja();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_PlayerValid();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthGame_PlayerCandidateCount();

        // ---------------------------------------------------------------- memory
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthMem_Read(uint addr, IntPtr buffer, int size);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthMem_Write(uint addr, IntPtr data, int size);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthMem_IsReadable(uint addr, int size);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint EthMem_ModuleBase();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern uint EthMem_GameAddr(uint va);

        // ----------------------------------------------------------------- input
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthInput_KeyDown(int vk);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthInput_LeaseMovementKeys(uint player, int enabled);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthInput_KeyPressed(int vk);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthInput_MouseDown(int button);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthInput_GetMouseDelta(out float dx, out float dy);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthInput_ConsumeMouseDelta();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthInput_SetCapture(int on);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthInput_GetCapture();

        // --------------------------------------------------------------- overlay
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthOverlay_IsOpen();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthOverlay_Toggle();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthOverlay_SetOpen(int open);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthOverlay_ToggleKey();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthOverlay_NotifyRenderException();

        // ----------------------------------------------------------------- hooks
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthHooks_SetManagedCallbacks(IntPtr tick, IntPtr gui, IntPtr overlayChanged);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthHooks_GameHooked();

        // ------------------------------------------------------------------ mods
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthMod_IsEnabled(string id);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthMod_Count();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthMod_GetId(int index, [Out] byte[] buffer, int bufferSize);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthMod_GetName(int index, [Out] byte[] buffer, int bufferSize);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthMod_GetSetting(string id, string key, [Out] byte[] buffer, int bufferSize);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthMod_SetSetting(string id, string key, string value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern float EthMod_GetSettingFloat(string id, string key, float def);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthMod_SetSettingFloat(string id, string key, float value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern int EthMod_GetSettingBool(string id, string key, int def);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthMod_SetSettingBool(string id, string key, int value);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthMod_SaveSettings();
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthMod_ReportError(string id, string message);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthMod_SetStatus(string id, string status);
        [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, ExactSpelling = true)]
        internal static extern void EthMod_OpenFolder();
    }
}
