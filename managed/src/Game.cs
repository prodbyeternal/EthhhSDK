// ===========================================================================
//  EthhhSDK.Managed - game / camera / strat access for C# mods
// ===========================================================================
using System;
using System.Runtime.InteropServices;

namespace EthhhSDK
{
    public static class Game
    {
        public static bool IsReady => Native.EthGame_IsReady() != 0;
        public static uint TickCount => Native.EthGame_TickCount();
        public static uint FrameCount => Native.EthGame_FrameCount();
        public static float Time => Native.EthGame_Time();
        public static float TimeStep => Native.EthGame_TimeStep();
        public static float TimeScale
        {
            get => Native.EthGame_TimeScale();
            set => Native.EthGame_SetTimeScale(value);
        }
        public static int DebugFlags
        {
            get => Native.EthGame_DebugFlags();
            set => Native.EthGame_SetDebugFlags(value);
        }
        public static IntPtr Window => Native.EthGame_Hwnd();
        public static IntPtr Device => Native.EthGame_Device();
        public static IntPtr Renderer => Native.EthGame_Renderer();
        public static void UpdateView() => Native.EthGame_UpdateView();

        // ----------------------------------------------------------------- camera
        public static bool TryReadCamera(out CameraInfo camera)
            => Native.EthGame_ReadCamera(out camera) != 0;

        public static bool SetCameraPosition(float x, float y, float z)
            => Native.EthGame_SetCameraPos(x, y, z) != 0;

        public static bool SetCameraWorld(float[] world16)
            => world16 != null && world16.Length >= 16 && Native.EthGame_SetCameraWorld(world16) != 0;

        public static float FovDegrees
        {
            get => Native.EthGame_CameraFovDegrees();
            set => Native.EthGame_SetCameraFovDegrees(value);
        }

        public static unsafe bool GetCameraPosition(out float x, out float y, out float z)
        {
            x = y = z = 0;
            if (!TryReadCamera(out CameraInfo cam)) return false;
            x = cam.World[12];
            y = cam.World[13];
            z = cam.World[14];
            return true;
        }

        // ----------------------------------------------------------------- strats
        public static uint PlayerAddr => Native.EthGame_PlayerAddr();
        public static bool PlayerValid => Native.EthGame_PlayerValid() != 0;
        public static int PlayerMode => Native.EthGame_PlayerMode();   // 0=auto 1=manual 2=learning
        public static int PlayerCandidateCount => Native.EthGame_PlayerCandidateCount();
        public static int StratCount => Native.EthGame_StratCount();

        public static bool TryReadStratIdentity(uint addr, out StratIdentity identity)
        {
            identity = default;
            return addr != 0 && Native.EthGame_ReadStratIdentity(addr, out identity) != 0;
        }

        public static bool TraceSegments(uint exactNinja, TraceSegment[] segments, TraceHit[] hits)
            => exactNinja != 0 && segments != null && hits != null && segments.Length > 0 &&
               segments.Length == hits.Length && segments.Length <= 16 &&
               Native.EthGame_TraceSegments(exactNinja, segments, segments.Length, hits) != 0;
        public static void SetExternalCameraOwned(bool owned) => Native.EthPatch_SetExternalCameraOwned(owned ? 1 : 0);
        public static bool SetNinjaFacing(float yawRadians) => PlayerAddr != 0 && Native.EthGame_SetNinjaFacing(PlayerAddr, yawRadians) != 0;

        public static string GetStratClassName(uint addr)
        {
            if (addr == 0) return string.Empty;
            var bytes = new byte[64];
            if (Native.EthGame_StratClassName(addr, bytes, bytes.Length) == 0) return string.Empty;
            int end = Array.IndexOf(bytes, (byte)0);
            return end > 0 ? System.Text.Encoding.ASCII.GetString(bytes, 0, end) : string.Empty;
        }

        public static string PlayerDetectionInfo
        {
            get
            {
                var bytes = new byte[256];
                if (Native.EthGame_PlayerDetectionInfo(bytes, bytes.Length) == 0) return string.Empty;
                int end = Array.IndexOf(bytes, (byte)0);
                return end >= 0 ? System.Text.Encoding.ASCII.GetString(bytes, 0, end) : string.Empty;
            }
        }

        public static bool TryReadStrat(uint addr, out StratInfo strat)
        {
            strat = default;
            return addr != 0 && Native.EthGame_ReadStrat(addr, out strat) != 0;
        }

        public static bool TryReadPlayer(out StratInfo strat)
            => TryReadStrat(PlayerAddr, out strat);

        public static unsafe bool TryGetPlayerPosition(out float x, out float y, out float z)
        {
            x = y = z = 0;
            if (!TryReadPlayer(out StratInfo s)) return false;
            x = s.Pos[0]; y = s.Pos[1]; z = s.Pos[2];
            return true;
        }

        public static bool WriteStratPosition(uint addr, float x, float y, float z)
            => addr != 0 && Native.EthGame_WriteStratPos(addr, x, y, z) != 0;

        public static bool WritePlayerPosition(float x, float y, float z)
            => WriteStratPosition(PlayerAddr, x, y, z);

        public static StratInfo[] SnapshotStrats(int maxCount = 4096)
        {
            if (maxCount <= 0) return Array.Empty<StratInfo>();
            var list = new StratInfo[maxCount];
            int n = Native.EthGame_SnapshotStrats(list, maxCount);
            if (n < list.Length) Array.Resize(ref list, n);
            return list;
        }

        public static void PlayerSetMode(int mode) => Native.EthGame_PlayerSetMode(mode);
        public static void PlayerSelect(uint addr) => Native.EthGame_PlayerSelect(addr);
        public static void PlayerLearn() => Native.EthGame_PlayerLearn();
        public static bool PlayerReacquireExactNinja() => Native.EthGame_PlayerReacquireExactNinja() != 0;

        // ------------------------------------------------------------ save stats
        public static bool TryGetStatInt(int index, out int value)
            => Native.EthGame_StatInt(index, out value) != 0;
        public static bool SetStatInt(int index, int value) => Native.EthGame_SetStatInt(index, value) != 0;
        public static int GetStatBit(int index) => Native.EthGame_StatBit(index);   // -1 invalid
        public static bool SetStatBit(int index, bool value) => Native.EthGame_SetStatBit(index, value ? 1 : 0) != 0;

        // ------------------------------------------------------------- native table
        public static uint NativeAddress(int index) => Native.EthGame_NativeAddr(index);
        public static int NativeCount => Native.EthGame_NativeCount();

        // -------------------------------------------------------------- debug bits
        public static class Debug
        {
            public const int LevelSelect = 0x001;
            public const int DemoLevelSelect = 0x002;
            public const int Sound = 0x004;
            public const int Pal = 0x008;
            public const int Cheats = 0x010;
            public const int Text = 0x020;
            public const int Fly = 0x040;
            public const int DemoBuild = 0x080;
            public const int DemoMagazine = 0x100;

            public static bool Has(int bit) => (Game.DebugFlags & bit) != 0;
            public static void Set(int bit, bool on)
            {
                int flags = Game.DebugFlags;
                Game.DebugFlags = on ? (flags | bit) : (flags & ~bit);
            }
        }
    }

    public static class Memory
    {
        public static uint ModuleBase => Native.EthMem_ModuleBase();
        public static uint GameAddr(uint va) => Native.EthMem_GameAddr(va);
        public static bool IsReadable(uint addr, int size = 4) => Native.EthMem_IsReadable(addr, size) != 0;

        public static unsafe bool ReadBytes(uint addr, byte[] buffer, int count)
        {
            if (buffer == null || count <= 0 || count > buffer.Length) return false;
            fixed (byte* p = buffer)
                return Native.EthMem_Read(addr, (IntPtr)p, count) != 0;
        }

        public static unsafe bool WriteBytes(uint addr, byte[] data, int count = -1)
        {
            if (data == null) return false;
            if (count < 0) count = data.Length;
            if (count > data.Length) return false;
            fixed (byte* p = data)
                return Native.EthMem_Write(addr, (IntPtr)p, count) != 0;
        }

        [ThreadStatic] private static IntPtr s_scratch;
        private static IntPtr Scratch()
        {
            if (s_scratch == IntPtr.Zero) s_scratch = Marshal.AllocHGlobal(256);
            return s_scratch;
        }

        public static bool Read<T>(uint addr, out T value) where T : unmanaged
        {
            value = default;
            int size = Marshal.SizeOf<T>();
            if (size <= 0 || size > 256) return false;
            IntPtr mem = Scratch();
            if (Native.EthMem_Read(addr, mem, size) == 0) return false;
            value = Marshal.PtrToStructure<T>(mem);
            return true;
        }

        public static bool Write<T>(uint addr, T value) where T : unmanaged
        {
            int size = Marshal.SizeOf<T>();
            if (size <= 0 || size > 256) return false;
            IntPtr mem = Scratch();
            Marshal.StructureToPtr(value, mem, false);
            return Native.EthMem_Write(addr, mem, size) != 0;
        }

        public static bool ReadInt32(uint addr, out int value) => Read(addr, out value);
        public static bool ReadUInt32(uint addr, out uint value) => Read(addr, out value);
        public static bool ReadSingle(uint addr, out float value) => Read(addr, out value);
        public static bool ReadByte(uint addr, out byte value) => Read(addr, out value);
        public static bool ReadInt16(uint addr, out short value) => Read(addr, out value);
        public static bool WriteInt32(uint addr, int value) => Write(addr, value);
        public static bool WriteUInt32(uint addr, uint value) => Write(addr, value);
        public static bool WriteSingle(uint addr, float value) => Write(addr, value);
        public static bool WriteByte(uint addr, byte value) => Write(addr, value);

        public static string ReadString(uint addr, int maxLength = 256)
        {
            if (maxLength <= 0) return string.Empty;
            byte[] bytes = new byte[maxLength];
            if (!ReadBytes(addr, bytes, maxLength)) return string.Empty;
            int len = Array.IndexOf(bytes, (byte)0);
            if (len < 0) len = bytes.Length;
            return System.Text.Encoding.UTF8.GetString(bytes, 0, len);
        }
    }

    public static class Input
    {
        public static bool KeyDown(int vk) => Native.EthInput_KeyDown(vk) != 0;
        public static bool KeyPressed(int vk) => Native.EthInput_KeyPressed(vk) != 0;
        public static bool MouseDown(int button = 0) => Native.EthInput_MouseDown(button) != 0;

        public static void GetMouseDelta(out float dx, out float dy) => Native.EthInput_GetMouseDelta(out dx, out dy);
        public static void ConsumeMouseDelta() => Native.EthInput_ConsumeMouseDelta();
        public static bool SetCapture(bool on) => Native.EthInput_SetCapture(on ? 1 : 0) != 0;
        public static bool CaptureActive => Native.EthInput_GetCapture() != 0;
        public static bool OverlayOpen => Native.EthOverlay_IsOpen() != 0;
        public static int OverlayToggleKey => Native.EthOverlay_ToggleKey();
        public static void ToggleOverlay() => Native.EthOverlay_Toggle();
        public static void SetOverlayOpen(bool open) => Native.EthOverlay_SetOpen(open ? 1 : 0);
    }

    /// <summary>Win32 virtual-key codes (subset of the most useful ones).</summary>
    public static class Keys
    {
        public const int Back = 0x08, Tab = 0x09, Enter = 0x0D, Shift = 0x10, Ctrl = 0x11, Alt = 0x12;
        public const int Pause = 0x13, CapsLock = 0x14, Escape = 0x1B, Space = 0x20;
        public const int PageUp = 0x21, PageDown = 0x22, End = 0x23, Home = 0x24;
        public const int Left = 0x25, Up = 0x26, Right = 0x27, Down = 0x28;
        public const int PrintScreen = 0x2C, Insert = 0x2D, Delete = 0x2E;
        public const int D0 = 0x30, D1 = 0x31, D2 = 0x32, D3 = 0x33, D4 = 0x34;
        public const int D5 = 0x35, D6 = 0x36, D7 = 0x37, D8 = 0x38, D9 = 0x39;
        public const int A = 0x41, B = 0x42, C = 0x43, D = 0x44, E = 0x45, F = 0x46, G = 0x47;
        public const int H = 0x48, I = 0x49, J = 0x4A, K = 0x4B, L = 0x4C, M = 0x4D, N = 0x4E;
        public const int O = 0x4F, P = 0x50, Q = 0x51, R = 0x52, S = 0x53, T = 0x54, U = 0x55;
        public const int V = 0x56, W = 0x57, X = 0x58, Y = 0x59, Z = 0x5A;
        public const int LWin = 0x5B, RWin = 0x5C;
        public const int Numpad0 = 0x60, Numpad1 = 0x61, Numpad2 = 0x62, Numpad3 = 0x63, Numpad4 = 0x64;
        public const int Numpad5 = 0x65, Numpad6 = 0x66, Numpad7 = 0x67, Numpad8 = 0x68, Numpad9 = 0x69;
        public const int F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73, F5 = 0x74, F6 = 0x75;
        public const int F7 = 0x76, F8 = 0x77, F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B;
        public const int LeftShift = 0xA0, RightShift = 0xA1, LeftCtrl = 0xA2, RightCtrl = 0xA3;
        public const int LeftAlt = 0xA4, RightAlt = 0xA5;
    }
}
