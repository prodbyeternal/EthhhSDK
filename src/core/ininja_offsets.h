// ============================================================================
// I-Ninja.exe  -  modding SDK header   (static analysis only, merged + extended)
// File size : 1,040,384 bytes
// SHA-256   : 49099fef26610e84f94f1aa2dd33aa9e83413ee24ff87fed63a8a98cc4f3daf0
// 32-bit PE, ImageBase 0x400000, no ASLR -> every address is an absolute VA and
// is valid as-is for THIS exact file only. Other builds: use the SIGS.
// Build as x86 (32-bit) and inject into the game process.
//
// Confidence tags
//   [V] verified in disassembly (address/offset/semantics read from the code)
//   [I] inferred from usage - plausible, confirm at runtime before relying on it
//   [?] unresolved / only the location is known
//
// WHAT THIS HEADER DOES *NOT* CONTAIN (be aware before you build on it)
//   * The player ("Ninja") object, his movement state, health/ki and model
//     data were NOT located. They are not fixed globals: gameplay lives in the
//     ASL script VM, and the player is just one CStrat in the strat list.
//     Section "player" below gives you the verified way to enumerate every
//     strat plus a runtime workflow to pin the ninja down. Nothing in this file
//     claims a player offset that was not read from the binary.
//   * Static analysis cannot prove "100% accurate". Treat [I]/[?] as leads.
// ============================================================================
#pragma once
#include <cstdint>
#include <cstddef>

#if defined(_WIN32) && !defined(_M_IX86) && !defined(__i386__)
#error "I-Ninja is a 32-bit process: build this SDK as x86."
#endif

namespace INinja {

constexpr uintptr_t ImageBase = 0x400000;

// ------------------------------------------------------------ small helpers --
template <class T> inline T& At(uintptr_t addr)            { return *reinterpret_cast<T*>(addr); }
template <class T> inline T& Field(void* base, size_t off) { return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(base) + off); }

// Angle unit used by script natives: 4096 units = one full turn  [V] (0x4189D0 / 0x418A00)
constexpr float kAngleUnitsPerTurn = 4096.0f;
constexpr float kPi                = 3.14159265358979f;
inline float AngleUnitsToRad(float u) { return u * (2.0f * kPi / kAngleUnitsPerTurn); }
inline float RadToAngleUnits(float r) { return r * (kAngleUnitsPerTurn / (2.0f * kPi)); }

// ----------------------------------------------------------------- GRAPHICS --
// Renderer is Direct3D 9 (Direct3DCreate9(0x1F)); D3DX is statically linked.
namespace gfx {
    constexpr uintptr_t pD3D9          = 0x5256F4; // [V] IDirect3D9*            (written 0x45223C)
    constexpr uintptr_t D3DWrapperObj  = 0x5256FC; // [V] static wrapper object; dword[0] == IDirect3D9*
    constexpr uintptr_t pDevice        = 0x5256F8; // [V] IDirect3DDevice9*      (written after CreateDevice, 0x4524FD/508/514)
    constexpr uintptr_t pRenderer      = 0x525500; // [V] -> Renderer object (pointer to pointer)

    // Renderer object layout (only fields seen)
    constexpr uintptr_t Renderer_Flag10             = 0x10;    // [V] byte, tested in Fn_Tick (0x43E0C8); +0x14 cleared when 0
    constexpr uintptr_t Renderer_pD3DWrapper        = 0x7B0;   // [V] Renderer+0x7B0 -> wrapper; *(wrapper) == IDirect3DDevice9*
    constexpr uintptr_t Renderer_Enabled            = 0x7C4;   // [V] byte, RenderFrame returns immediately if 0
    constexpr uintptr_t Renderer_FlagClearedPerTick = 0x5C2DC; // [V] byte, zeroed every tick in Fn_Tick (0x43E0E0)

    // D3D wrapper object layout
    constexpr uintptr_t Wrapper_pDevice      = 0x00;    // [V] IDirect3DDevice9*
    constexpr uintptr_t Wrapper_PresentParams= 0x0C;    // [V] D3DPRESENT_PARAMETERS (0x38 bytes)
    constexpr uintptr_t Wrapper_hFocusWindow = 0x28;    // [V] == PresentParams.hDeviceWindow
    constexpr uintptr_t Wrapper_StencilFlag  = 0x44;    // [V] byte; if set, Clear() flags get D3DCLEAR_STENCIL
    constexpr uintptr_t Wrapper_Adapter      = 0xAC;    // [V] adapter ordinal passed to CreateDevice

    // Functions / call sites
    constexpr uintptr_t RenderFrame        = 0x44A340; // [V] thiscall(Renderer*): Begin -> Clear -> draw -> EndScene -> Present
    constexpr uintptr_t Call_BeginScene    = 0x44A378; // [V] call [vtbl+0xA4]
    constexpr uintptr_t Call_Clear         = 0x44A3AB; // [V] call [vtbl+0xAC]  (color 0xFF000000, z=1.0)
    constexpr uintptr_t Call_EndScene      = 0x44A471; // [V] call [vtbl+0xA8]   <-- ImGui render point (hook at 0x44A46E)
    constexpr uintptr_t Call_Present       = 0x44A482; // [V] call [vtbl+0x44]  Present(0,0,0,0)
    constexpr uintptr_t Call_Reset         = 0x45247F; // [V] call [vtbl+0x40]  Reset(&wrapper->PresentParams)
    constexpr uintptr_t Call_CreateDevice_HW = 0x4524C0; // [V] HAL, behavior 0x40 (HW VP)
    constexpr uintptr_t Call_CreateDevice_SW = 0x4524E0; // [V] fallback, behavior 0x20 (SW VP)
    constexpr uintptr_t Thunk_Direct3DCreate9 = 0x45CEB0; // [V] jmp [0x4C7284]; only caller 0x452235
    constexpr uintptr_t Fn_CreateD3D       = 0x452230; // [V] thiscall(wrapper): wrapper[0]=Direct3DCreate9(0x1F); also sets pD3D9
    constexpr uintptr_t Fn_CreateDevice    = 0x452490; // [V] thiscall(wrapper)

    // Bink movie path uses its own Begin/Clear/End/Present. A vtable hook on
    // EndScene (index 42) fires for BOTH paths - filter by return address or
    // detour Call_EndScene directly if you do not want ImGui over cutscenes.
    constexpr uintptr_t Movie_Clear        = 0x496C71; // [V]
    constexpr uintptr_t Movie_BeginScene   = 0x496C7D; // [V]
    constexpr uintptr_t Movie_EndScene     = 0x496C9F; // [V]
    constexpr uintptr_t Movie_Present      = 0x496CB3; // [V]

    // IDirect3DDevice9 vtable indices (standard)
    enum Vtbl { Reset = 16, Present = 17, SetRenderState = 57, BeginScene = 41, EndScene = 42, Clear = 43 };

    // Signatures (unique in .text of this build)
    constexpr const char* SIG_EndScene_Present = "8B 0F 57 FF 91 A8 00 00 00 8B 17 6A 00 6A 00 6A 00 6A 00 57 FF 52 44"; // [V]
    constexpr const char* SIG_BeginScene_Clear = "8B 0F 57 FF 91 A4 00 00 00 8B 8E B0 07 00 00 8A 59 44 8B 11";            // [V]
    constexpr const char* SIG_Direct3DCreate9  = "56 6A 1F 8B F1 E8 ?? ?? ?? ??";                                        // [V]
}

// ------------------------------------------------------------- WINDOW / INPUT --
namespace win {
    constexpr uintptr_t hWnd           = 0x50FFAC; // [V] HWND (copied from window object+8 at 0x43DDDE)
    constexpr uintptr_t pApp           = 0x50FFA4; // [V] -> App object (alloc 0x43A4B0, ctor 0x43D800)
    constexpr uintptr_t pHInstance     = pApp;     // App+0x00 = HINSTANCE (not a standalone pointer)
    constexpr uintptr_t pClassName     = 0x4F00F8; // [V] -> "LegoNinjaPCWindowClass"
    constexpr uintptr_t WindowMap      = 0x525874; // [V] HWND -> window object lookup (find fn 0x457240)
    constexpr uintptr_t WndProc_Static = 0x4572B0; // [V] registered WNDPROC, stdcall(hwnd,msg,wp,lp); falls back to DefWindowProcA
    constexpr uintptr_t WndProc_Member = 0x457140; // [V] window object vtable[0] (vtable @ 0x4CA920)
    constexpr uintptr_t Fn_WindowCtor  = 0x456FA0; // [V] thiscall; this+8 = HWND, this+0xC = D3D wrapper*
    constexpr uintptr_t Call_RegisterClassA = 0x456F6B; // [V] style 0x4003
    constexpr uintptr_t Call_CreateWindowExA= 0x457015; // [V] style 0xC30000, default 640x480
    constexpr uintptr_t Thunk_DirectInput8Create = 0x494920; // [V] jmp [0x4C7018]; caller 0x444908
    // Input is DirectInput 8 (keyboard.cfm holds the key map). The input object
    // is created at 0x4448F0 (DirectInput8Create) and sets up devices at ~0x444750.
    // ImGui: either SetWindowLongPtr(hWnd, GWLP_WNDPROC, ...) or detour WndProc_Static.
}

// ------------------------------------------------------------------- MAIN LOOP --
namespace loop {
    constexpr uintptr_t App_Run        = 0x43D8F0; // [V] thiscall; PeekMessage pump (call at 0x43D909), WM_QUIT=0x12
    constexpr uintptr_t Fn_Tick        = 0x43E0A0; // [V] thiscall(App*, int tickIdx) - one fixed logic tick
    constexpr uintptr_t Fn_RenderStep  = 0x43E170; // [V] per-frame render step (increments frame counter)
    constexpr uintptr_t Fn_ExtraTickPre= 0x43E8A0; // [V] run before each EXTRA catch-up tick: walks strat list,
                                                   //     rewrites per-strat frame stamps (+0x158/+0x15C) to TickCount-1
    constexpr uintptr_t Call_TickInLoop_First = 0x43D99B; // [V] first Fn_Tick call of a frame
    constexpr uintptr_t Call_TickInLoop_Extra = 0x43D9B8; // [V] Fn_Tick call inside the catch-up loop
    constexpr uintptr_t TickCount      = 0x51AB38; // [V] dword, ++ at end of Fn_Tick
    constexpr uintptr_t FrameCount     = 0x51AB3C; // [V] dword, ++ per render step
    constexpr uintptr_t TimeStepF      = 0x51AB40; // [V] float = StratMgr.TimeScale * (1/60); set at top of StratMgr tick (0x4246F3..0x4246FF)
    constexpr uintptr_t GameTimeF      = 0x51AB44; // [V] float, GameTimeF += TimeStepF at end of Fn_Tick (0x43E14F..0x43E166)
    constexpr uintptr_t pSettings      = 0x529214; // [I] -> settings struct (bytes at +2,+4,+0xD gate render/UI)

    // App fields [V] (App = *(void**)0x50FFA4; ctor 0x43D800 initialises them):
    //   +0x00 HINSTANCE
    //   +0x08 fixed-step timer, set to 60 Hz by Timer_SetRate (0x4010B0)
    //   +0x28 render pose interpolation fraction (View_Render, 0x43F2E4)
    //   +0x2C input object, +0x30 window/message object, +0x38..+0x44 lifecycle
    namespace app {
        constexpr uintptr_t HInstance          = 0x00; // [V]
        constexpr uintptr_t Timer              = 0x08; // [V]
        constexpr uintptr_t Interpolation      = 0x28; // [V] float, normally forced to 1.0 by Fn_RenderStep
        constexpr uintptr_t Input              = 0x2C; // [V] pointer, used by input::MovementTarget
    }
    // Timer object (App+0x08), verified by Timer_SetRate 0x4010B0 and
    // Timer_Update 0x401110. Timer_Update floors whole ticks and leaves a
    // non-negative sub-tick remainder used here for render interpolation.
    namespace timer {
        constexpr uintptr_t MsPerTick          = 0x00; // [V] float, 1000 / rate (16.6667 at 60 Hz)
        constexpr uintptr_t TicksPerMs         = 0x04; // [V] float, rate / 1000 (0.06 at 60 Hz)
        constexpr uintptr_t RemainderMs        = 0x08; // [V] float, unconsumed time since the last whole tick
        constexpr uintptr_t Clock              = 0x10; // [V] 16-byte performance-counter clock
    }

    // Timing model [V]: logic is FIXED-STEP 60 Hz (constant 0x4C7E60 == 0.016666667f).
    // App_Run asks the timer (0x401160) for the number of due ticks, clamps to 4
    // (cmp esi,4 @ 0x43D987) and calls Fn_Tick that many times per rendered frame.
    constexpr float kTickDt  = 0.016666667f;
    constexpr int   kMaxTicksPerFrame = 4;

    // SIG for App_Run (prefix, rel-call bytes omitted): 83 EC 20 53 55 56 8B D9 57 8D 7B 08 8B CF 89 7C 24 10

    // Fn_Tick order [V] (0x43E0A0): input/pump(0x456E50, 0x448450) -> 0x456710(*0x525868) -> View tick (0x43EFA0 on *0x51AB64)
    //   -> zero Renderer+0x5C2DC -> 0x43FEC0 -> StratMgr::Tick (0x4246F0, this=0x51019C) -> 0x42B580 (this=0x519A60)
    //   -> 0x4593B0(*0x510194) -> 0x40CFD0(this=0x500D80) -> 0x40CDD0/0x40C620(*0x51AB54) -> TickCount++, GameTimeF += dt
}

// ------------------------------------------------------------ ENGINE GLOBALS --
// 0x51AB30..0x51AB68 is a block of engine state. The pointers in it
// (0x51AB48..0x51AB68) are NEVER stored through an absolute-address instruction
// in .text (verified by raw scan of all 4-byte windows), so there is no single
// "write site" to hook: read the values at runtime, after the level is loaded.
namespace eng {
    constexpr uintptr_t pCamera    = 0x51AB4C; // [V] -> Camera object (see cam::)
    constexpr uintptr_t p51AB48    = 0x51AB48; // [?] object ptr, +0x10 used; passed to 0x447110 from the 'show UI' native
    constexpr uintptr_t p51AB50    = 0x51AB50; // [I] -> large object (>=0x5C464 bytes) built in ctor ~0x44C2xx; same family as Renderer
    constexpr uintptr_t pWorld51AB54 = 0x51AB54;// [?] most-referenced global (78 sites); ticked via 0x40CDD0/0x40C620 each tick
    constexpr uintptr_t p51AB58    = 0x51AB58;  // [?]
    constexpr uintptr_t p51AB5C    = 0x51AB5C;  // [?] used by HUD/script code around 0x418D41..0x418E59
    constexpr uintptr_t pView      = 0x51AB64;  // [V] -> View/frustum object (0x160 bytes, ctor 0x43F180, stored @0x43A52A)
    constexpr uintptr_t p51AB68    = 0x51AB68;  // [?] object ptr (+0x6C,+0x58,+0x20 used)
    constexpr uintptr_t StaticObj_51AB70 = 0x51AB70; // [V] static 0x90-byte object, ctor 0x4402C0 (CRT init @0x4C5630)
}

// --------------------------------------------------------------- DEBUG FLAGS --
// Config keys parsed at startup into a WORD; ASL scripts read it through native getters.
namespace dbg {
    constexpr uintptr_t Flags = 0x51AB30; // [V] WORD
    enum : uint16_t {
        LEVEL_SELECT      = 0x001, // [V] key "LEVEL_SELECT"
        DEMO_LEVEL_SELECT = 0x002, // [V]
        SOUND             = 0x004, // [V]
        PAL               = 0x008, // [V]
        DEBUG_CHEATS      = 0x010, // [V] read at 0x4224CB
        DEBUG_TEXT        = 0x020, // [V] read at 0x406E08 / 0x418A47 / 0x43E1FD
        DEBUG_FLY         = 0x040, // [V] mask confirmed in parser; consumer is script-side (whole-word getter 0x422A62)
        DEMO_BUILD        = 0x080, // [V]
        DEMO_MAGAZINE     = 0x100, // [V]
    };
    // Parser reads these keys: NAME WAD LANGUAGE BUILD HEAPSIZE KEYCODE PRODUCT_NUMBER SAVE_NAME (+ flag names above)
    // Setting the flag bits directly at runtime is the easiest way to get cheats / fly / debug text.
    inline uint16_t& FlagsRef() { return At<uint16_t>(Flags); }
}

// ------------------------------------------------------------ DATA FILE NAMES --
// options.dat, engine.opt, save.dat, keyboard.cfm, white.bmp, Movies\EA_Movie.bik, "\Saved Games\"
// engine.opt keys: render_fps render_profile render_asl_text render_free_memory render_line_count
//                  dual_monitor force_intel always_windowed play_movies intel_shadows auto_detect_intel user_mark
// (render_fps / render_profile / render_asl_text give built-in overlays with no code)

// ------------------------------------------------------------- SAVE / STATS --
namespace save {
    constexpr uintptr_t pProfile   = 0x51AA88; // [V] ptr -> 0x1EC4-byte profile struct (Load copies into it @0x457696)
    constexpr uintptr_t Prog       = 0x14;     // [V] progress sub-object = profile+0x14
    constexpr uintptr_t Ints       = 0x34;     // [V] int32 table: *(profile+0x34+idx*4)
    constexpr uintptr_t Bits       = 0x1034;   // [V] bitfield: byte (bit>>3), mask 1<<(bit&7)
    // profile+0x00..0x04 (bytes) and +0x10 (dword) are options/volumes [I], not stats
    constexpr uintptr_t Fn_SetInt  = 0x40EA50; // [V] thiscall(prog, idx, val)
    constexpr uintptr_t Fn_SetBit  = 0x40EA60; // [V] thiscall(prog, idx, bool)
    constexpr uintptr_t Fn_GetInt  = 0x40EAC0; // [V] thiscall(prog, idx)
    constexpr uintptr_t Fn_GetBit  = 0x40EAD0; // [V] thiscall(prog, idx)
    // Script wrappers: SetInt 0x414F40, SetBit 0x414F60, GetInt 0x414F80, GetBit 0x414FA0 [V]
    // Save manager (exported names): Initialise 0x457920, Load 0x457640, Save 0x457990 [V]
    //   3 slots, stride 0x1EC8, data at mgr+0xC+slot*0x1EC8, checksum dword right after data
    //   (checksum fn 0x406540(ptr,len)). Edited save data needs the checksum fixed.
}

// ---------------------------------------------------------------------- CAMERA --
// *(Camera**)0x51AB4C. Layout below is what the engine itself reads/writes.
namespace cam {
    constexpr uintptr_t pCamera    = 0x51AB4C; // [V] read in 20 places (writer is indirect, see eng::)

    // 0x40..0x7F: a second 4x4 float matrix that Fn_UpdateView multiplies with the
    // inverse (view) matrix                                                        [I]
    constexpr uintptr_t Mat40      = 0x40;
    // World matrix of the camera: 4x4 floats, ROW-MAJOR, row 3 = translation.
    constexpr uintptr_t Matrix     = 0x80;     // [V] 4x4 floats
    constexpr uintptr_t RowX       = 0x80;     // [I] rows 0..2 = basis vectors (which is right/up/fwd is untested)
    constexpr uintptr_t RowY       = 0x90;     // [I]
    constexpr uintptr_t RowZ       = 0xA0;     // [I]
    constexpr uintptr_t Position   = 0xB0;     // [V] float x,y,z (+0xB0,+0xB4,+0xB8) - used as a world POSITION at
                                               //     0x40E79A, 0x449639 (world - cam), 0x4598EE, 0x45B929
    // View matrix = inverse of Matrix, recomputed by Fn_UpdateView:
    //   inverse(cam+0x80) via 0x4B2A60, then rep-movs 16 dwords -> cam+0xC0.
    constexpr uintptr_t ViewMatrix = 0xC0;     // [V] 4x4 floats (row-major, same convention as Matrix)
    constexpr uintptr_t Param104   = 0x104;    // [V] float. NOT the FOV. Copied x2 into View+0x04 every tick (0x43EFA0);
                                               //     [I] far-clip / draw-distance style scale. Script native 308 sets it raw (0x418C60),
                                               //     native 735 returns Param104*1.2+150 (0x418CA0)
    constexpr uintptr_t Fov        = 0x108;    // [V] float, FIELD OF VIEW IN RADIANS (full angle). View+0x10 = tan(Fov/2).
                                               //     Native 292 (0x4189D0) writes arg*pi/2048, i.e. arg is in 1/4096-turn units;
                                               //     native 744 (0x418A00) reads it back in those units.
    constexpr uintptr_t Fn_UpdateView = 0x40B750; // [V] thiscall(Camera*)
    constexpr uintptr_t Fn_Dirty      = 0x438FF0; // [V] a bare `ret` (virtual no-op) called after every camera setter
    constexpr uintptr_t Fn_ViewTick   = 0x43EFA0; // [V] thiscall(View*) called every tick from Fn_Tick (ecx = *0x51AB64)

    // Script natives that touch the camera (index into native table, see natives::)
    constexpr int Native_SetFov_Units     = 292; // [V] 0x4189D0  Fov = arg * pi/2048
    constexpr int Native_SetParam104      = 308; // [V] 0x418C60
    constexpr int Native_GetParam104      = 734; // [V] 0x418C80
    constexpr int Native_GetParam104Scaled= 735; // [V] 0x418CA0  = Param104*1.2 + 150
    constexpr int Native_GetFov_Units     = 744; // [V] 0x418A00  = Fov * 4096/(2*pi)

    struct Camera {                               // overlay; never allocate this yourself
        uint8_t  _pad0[0x40];
        float    mat40[16];                       // 0x40 [I]
        float    world[16];                       // 0x80 [V]  world[12..14] = position
        float    view[16];                        // 0xC0 [V]
        uint8_t  _pad100[0x4];                    // 0x100 [?]
        float    param104;                        // 0x104 [V]
        float    fov;                             // 0x108 [V] radians
        float&       X()       { return world[12]; }
        float&       Y()       { return world[13]; }
        float&       Z()       { return world[14]; }
    };
    static_assert(offsetof(Camera, world)   == 0x80,  "cam matrix");
    static_assert(offsetof(Camera, view)    == 0xC0,  "cam view");
    static_assert(offsetof(Camera, param104)== 0x104, "cam param104");
    static_assert(offsetof(Camera, fov)     == 0x108, "cam fov");

    inline Camera* Get() { return At<Camera*>(pCamera); }   // null until a level/camera exists

    // The View object (*0x51AB64) - rebuilt every tick by Fn_ViewTick [V]
    namespace view {
        constexpr uintptr_t Aspect    = 0x00; // [V] float, constant 4/3 (0x3FAAAAAB) written each tick
        constexpr uintptr_t Param104x2= 0x04; // [V] = 2 * Camera.Param104
        constexpr uintptr_t Fov       = 0x08; // [V] = Camera.Fov (radians)
        constexpr uintptr_t FovCopy   = 0x0C; // [V]
        constexpr uintptr_t TanHalfFov= 0x10; // [V] tan(Fov/2)
        constexpr uintptr_t SecHalfFov= 0x14; // [V] 1/cos(Fov/2)
        // +0x40..+0x5F are copied to +0x20..+0x3F every tick (previous-frame copy) [V]
    }
}

// ------------------------------------------------- STRATS (game objects) + MANAGER --
// A CStrat is every script-driven object: the ninja, enemies, pickups, the HUD...
namespace strat {
    // ----- per-strat layout
    constexpr uintptr_t Pos     = 0x160; // [V] float x,y,z (+0x16C = 4th component, used as extra term in distance test 0x4145D0)
    constexpr uintptr_t Angles  = 0x170; // [I] 3 floats derived from the quaternion (0x4209B0)
    constexpr uintptr_t Quat    = 0x180; // [V] float x,y,z,w (setters 0x417540/580/5C0)
    constexpr uintptr_t Flags   = 0x190; // [V] dword; bit 0x8000 cleared on list removal (0x423C35); bit 0x10000 toggled by 0x4156A0
    constexpr uintptr_t TypeMask= 0x194; // [I] dword tested against a caller mask in FindNearest (0x4145EB)
    constexpr uintptr_t pNext   = 0x198; // [V] next strat in the manager list (NULL at end)
    constexpr uintptr_t pPrev   = 0x19C; // [V] previous strat
    constexpr uintptr_t pClass  = 0x1A8; // [V] -> type/script-class info (WORD at +0x14 = entry count); also leads to the strat heap
    constexpr uintptr_t pParent = 0x1C4; // [I] used by Pos getter 0x416760
    constexpr uintptr_t Vec2B0  = 0x2B0; // [I] vec3 set by native 0x415670 (velocity?)
    constexpr uintptr_t FrameStampA = 0x158; // [I] compared with TickCount-2 / rewritten to TickCount-1 in Fn_ExtraTickPre
    constexpr uintptr_t FrameStampB = 0x15C; // [I] same
    constexpr uintptr_t FlagsByteC  = 0xC;   // [I] byte, bit 0x20 tested in Fn_ExtraTickPre
    // Natives are cdecl: (CStrat*, [ASLVar* ret,] ASLVar* args).

    struct Strat {                              // overlay of the known part; sizeof is NOT known (>= 0x240)
        uint8_t  _pad0[0x160];
        float    pos[4];                        // 0x160 [V]
        float    angles[3];                     // 0x170 [I]
        uint8_t  _pad17c[0x4];
        float    quat[4];                       // 0x180 [V]
        uint32_t flags;                         // 0x190 [V]
        uint32_t typeMask;                      // 0x194 [I]
        Strat*   next;                          // 0x198 [V]
        Strat*   prev;                          // 0x19C [V]
        uint8_t  _pad1a0[0x8];
        void*    pClass;                        // 0x1A8 [V]
    };
    static_assert(offsetof(Strat, pos)    == 0x160, "pos");
    static_assert(offsetof(Strat, quat)   == 0x180, "quat");
    static_assert(offsetof(Strat, flags)  == 0x190, "flags");
    static_assert(offsetof(Strat, next)   == 0x198, "next");
    static_assert(sizeof(void*) != 4 || offsetof(Strat, prev)   == 0x19C, "prev");   // exact only in an x86 build
    static_assert(sizeof(void*) != 4 || offsetof(Strat, pClass) == 0x1A8, "pClass");

    // ----- STRAT MANAGER: static object at 0x51019C  [V]
    // Fn_Tick does `mov ecx,0x51019C ; call 0x4246F0`, and 0x51019C+0x189C == 0x511A38,
    // the "global" every list-walking native reads. That is why 0x511A38 has no
    // absolute store: it is a member of this static object.
    namespace mgr {
        constexpr uintptr_t Object     = 0x51019C; // [V]
        constexpr uintptr_t Fn_Tick    = 0x4246F0; // [V] thiscall(mgr): sets TimeStepF, then iterates the list calling each strat's script
        constexpr uintptr_t Fn_ListRemove = 0x423C50; // [V] thiscall(mgr, Strat*)
        constexpr uintptr_t Fn_ListInsert = 0x423CC0; // [V] thiscall(mgr, Strat*, mode 0..3, ref) - head/before/after/tail
        constexpr uintptr_t Fn_FindNearest= 0x4145D0; // [I] distance-sorted search over the list (nonstandard regs: edx=self, esi=type mask)
        constexpr uintptr_t TimeScale  = 0x180C;   // [V] float, default 1.0 (ctor 0x424554). TimeStepF = TimeScale/60.
        constexpr uintptr_t ListHead   = 0x189C;   // [V] Strat*   (== VA 0x511A38)
        constexpr uintptr_t ListTail   = 0x18A0;   // [V] Strat*   (== VA 0x511A3C)
        constexpr uintptr_t IterCur    = 0x18B8;   // [V] current strat during Tick
        constexpr uintptr_t IterNext   = 0x18BC;   // [V] cached next (safe against removal while iterating)
        constexpr uintptr_t ListHeadVA = Object + ListHead; // 0x511A38
        constexpr uintptr_t ListTailVA = Object + ListTail; // 0x511A3C
        static_assert(ListHeadVA == 0x511A38, "strat list head VA");
    }

    inline Strat* First() { return At<Strat*>(mgr::ListHeadVA); }
    inline Strat* Last()  { return At<Strat*>(mgr::ListTailVA); }
    inline float& TimeScale() { return At<float>(mgr::Object + mgr::TimeScale); } // slow-mo / pause for the whole game

    // Walk every live strat. Copy `next` first so the callback may be defensive.
    template <class F> inline void ForEach(F&& fn) {
        for (Strat* s = First(); s; ) { Strat* n = s->next; fn(s); s = n; }
    }
}

// ------------------------------------------------------------------- SCRIPT NATIVES --
namespace natives {
    constexpr uintptr_t Table = 0x4C7E70;  // [V] array of 926 function pointers (0x4C8CE8 end); index-based, names are not in the exe
    constexpr int       Count = 926;       // [V]
    inline uintptr_t Addr(int idx) { return At<uintptr_t>(Table + 4u * static_cast<unsigned>(idx)); }
    // cdecl (CStrat* self, [ASLVar* ret,] ASLVar* args); ASLVar scalars are 32-bit (int or float) at offset 0.
    // Natives whose body (up to the first ret) references strat+0x160 (position), from a scan of every table entry
    // [V that they touch it; what each one does is [I]]: idx 16,47-59,62,71-80,83,85,88-90,147,151,153,163-164,170,173,
    // 262,311,314-315,379,423,428,430-431,632-641,665-669,689,705,747,750-751,753,756,759-761,788,820-821,828-831,886.
    // Of those, also referencing +0x180 (quaternion): 147, 262, 314-315, 428 [V].
    // Several indices share one address (e.g. 48/49 -> 0x416C00): they are aliases (int/float overloads).
    // 0x41C120 (idx 147), 0x41C5D0 (idx 262), 0x41CA10 (idx 428) are the big move/orient natives (~300 instrs).
}

// ------------------------------------------------------------------------ PLAYER --
// NOT FOUND statically (see top of file). What is known and what to do:
//  * The ninja is one Strat in strat::First()..->next. Script code drives his pos/quat each tick.
//  * Movement/jump/ki/health are ASL script state, stored in the strat's script heap
//    (pClass -> class info; the "Strat heap" allocator is 0x438260, out-of-memory text @0x4C8F1C).
//  * Runtime workflow to pin him down (do it once, then hard-code the discovered values):
//      1. strat::ForEach: log {ptr, pClass, pos, flags, typeMask} to a file.
//      2. Move the ninja, log again: the strat whose pos changes with your input is him.
//      3. Record his pClass: it is stable across ticks/levels of the same build and is a
//         cheap identity test (s->pClass == kNinjaClass).
//      4. Diff his raw bytes (0x000..0x300, plus the script heap block) while jumping /
//         running / taking damage to find velocity, grounded, health, ki.
namespace player {
    inline strat::Strat*& Cached() { static strat::Strat* p = nullptr; return p; }
    // Fill these after step 3 above; left null on purpose so nothing pretends to be known.
    inline void*& KnownClass() { static void* c = nullptr; return c; }
    inline strat::Strat* Find() {
        if (!KnownClass()) return nullptr;
        for (strat::Strat* s = strat::First(); s; s = s->next)
            if (s->pClass == KnownClass()) { Cached() = s; return s; }
        return nullptr;
    }
}

// ------------------------------------------- NOTES FOR A HALF-LIFE-STYLE MOVEMENT PORT --
//  * Run your pmove in a Fn_Tick hook (loop::Fn_Tick). It is fixed 60 Hz (loop::kTickDt), so
//    use dt = kTickDt (times strat::TimeScale()) instead of wall-clock frame time.
//  * Because ASL scripts rewrite the ninja's pos/quat every tick, writes from your hook must
//    land AFTER StratMgr::Tick (hook the return of Fn_Tick) or the script must be bypassed.
//  * Camera: write cam::Camera::world (position in row 3) then call cam::Fn_UpdateView(Camera*)
//    so ViewMatrix (+0xC0) and the View object stay consistent. Fov is radians at +0x108.
//    Script natives use 1/4096-turn angle units, not degrees.
//  * Axis/handedness of the world and which matrix row is forward/up/right are UNVERIFIED [I].

} // namespace INinja
