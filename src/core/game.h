#pragma once
#include "precomp.h"
#include "player_learning.h"
#include "strat_identity.h"
#include "ninja_control_policy.h"
#include "player_mode_policy.h"
#include "util.h"

namespace eth::game {

// ---------------------------------------------------------------------------
//  Engine addresses (RVAs from C:\...\ininja\ininja-offsets.h, verified against
//  the binary shipped on this machine). All are rebased at runtime.
// ---------------------------------------------------------------------------
namespace off {
constexpr uintptr_t ImageBase       = 0x400000;
constexpr uintptr_t pD3DDevice      = 0x5256F8;
constexpr uintptr_t pRenderer       = 0x525500;
constexpr uintptr_t hWnd            = 0x50FFAC;
constexpr uintptr_t Fn_Tick         = 0x43E0A0;
constexpr uintptr_t Fn_RenderStep   = 0x43E170;
constexpr uintptr_t TickCount       = 0x51AB38;
constexpr uintptr_t FrameCount      = 0x51AB3C;
constexpr uintptr_t TimeStepF       = 0x51AB40;
constexpr uintptr_t GameTimeF       = 0x51AB44;
constexpr uintptr_t pCamera         = 0x51AB4C;
constexpr uintptr_t pView           = 0x51AB64;
constexpr uintptr_t pApp            = 0x50FFA4; // -> App object (ctor 0x43D800)
constexpr uintptr_t AppTimer        = 0x08;     // fixed-step timer inside App
constexpr uintptr_t AppInterpolation= 0x28;     // render pose fraction consumed by View_Render
constexpr uintptr_t TimerMsPerTick  = 0x00;     // float, 1000/60 at the default rate
constexpr uintptr_t TimerTicksPerMs = 0x04;     // float, 0.06 at 60 Hz
constexpr uintptr_t TimerRemainderMs= 0x08;     // float, sub-tick remainder, floored by Timer_Update
constexpr uintptr_t DebugFlags      = 0x51AB30;
constexpr uintptr_t StratMgrObject  = 0x51019C;
constexpr uintptr_t StratListHead   = 0x51019C + 0x189C;
constexpr uintptr_t StratListTail   = 0x51019C + 0x18A0;
constexpr uintptr_t StratTimeScale  = 0x51019C + 0x180C;
constexpr uintptr_t StratIterCur    = 0x51019C + 0x18B8;
constexpr uintptr_t pProfile        = 0x51AA88;
constexpr uintptr_t Fn_UpdateView   = 0x40B750;
constexpr uintptr_t Fn_ViewTick     = 0x43EFA0;
constexpr uintptr_t Fn_SetInt       = 0x40EA50;
constexpr uintptr_t Fn_SetBit       = 0x40EA60;
constexpr uintptr_t Fn_GetInt       = 0x40EAC0;
constexpr uintptr_t Fn_GetBit       = 0x40EAD0;
constexpr uintptr_t NativeTable     = 0x4C7E70;
constexpr int       NativeCount     = 926;

constexpr uintptr_t CamMat          = 0x80;   // 4x4 world matrix, row major
constexpr uintptr_t CamViewMat      = 0xC0;   // inverse(world), recomputed by Fn_UpdateView
constexpr uintptr_t CamPos          = 0xB0;   // world[12..14]
constexpr uintptr_t CamFov          = 0x108;  // radians (vertical? full angle - engine uses tan(fov/2))
constexpr uintptr_t CamParam104     = 0x104;

constexpr uintptr_t StratPos        = 0x160;  // float x,y,z,w
constexpr uintptr_t StratAngles     = 0x170;
constexpr uintptr_t StratQuat       = 0x180;
constexpr uintptr_t StratFlags      = 0x190;
constexpr uintptr_t StratTypeMask   = 0x194;
constexpr uintptr_t StratNext       = 0x198;
constexpr uintptr_t StratPrev       = 0x19C;
constexpr uintptr_t StratClass      = 0x1A8;
constexpr uintptr_t StratSelfHandle = 0x1A0;
constexpr uintptr_t StratOwnerHandle= 0x238;  // only known owner-consuming classes
constexpr uintptr_t StratModel     = 0x268;
constexpr uintptr_t StratAnimation = 0x26C;
constexpr uintptr_t StratNodeMatrices=0x13C;
constexpr uintptr_t StratNodeCount  = 0x140; // equals model[0] and animation-control+0x24
constexpr uintptr_t StratRenderWorld= 0x10;  // inline world consumed by 0x43E660 / 0x44A490
constexpr uintptr_t StratHashVec    = 0x2B0;
constexpr uintptr_t StratFrameA     = 0x158;
constexpr uintptr_t StratFrameB     = 0x15C;
} // namespace off

inline uintptr_t A(uintptr_t va) { return GameAddr(va); }

// ---------------------------------------------------------------------------
//  Globals / timing
// ---------------------------------------------------------------------------
bool  IsReady();                       // game image sanity + engine globals readable
uint32_t TickCount();
uint32_t FrameCount();
float GameTime();
float TimeStep();
float TimeScale();
void  SetTimeScale(float v);

uint16_t DebugFlags();
void     SetDebugFlags(uint16_t flags);
void     ToggleDebugFlag(uint16_t bit, bool on);
const char* DebugFlagName(uint16_t bit);

void* Device();                        // IDirect3DDevice9*
void* Renderer();
void* GameHwnd();

// ---------------------------------------------------------------------------
//  Camera
// ---------------------------------------------------------------------------
struct CameraView {
    uintptr_t addr = 0;
    float world[16] = {};   // row major, row 3 = position
    float view[16] = {};
    float x = 0, y = 0, z = 0;
    float fov = 0;          // radians
    float param104 = 0;
};
bool  ReadCamera(CameraView& out);
void  UpdateView();                     // call Fn_UpdateView(camera)
bool  SetCameraPosition(float x, float y, float z);   // keeps orientation
bool  SetCameraWorld(const float m[16]);              // then refreshes view matrix
bool  SetCameraFovRadians(float fov);
bool  SetCameraFovUnits(float units);                 // script units: 4096 = full turn
bool  SetCameraFovDegrees(float degrees);
float CameraFovUnits(float radians);
float CameraFovRadians(float units);
void  GetCameraBasis(float* right, float* up, float* forward);
bool  MakeCameraWorld(float px, float py, float pz, float yawRad, float pitchRad, float rollRad, float out[16]);

// ---------------------------------------------------------------------------
//  Strats (everything script driven: the ninja, enemies, pickups, ...)
// ---------------------------------------------------------------------------
struct StratView {
    uintptr_t addr = 0;
    float pos[4] = {};
    float angles[3] = {};
    float quat[4] = {};
    uint32_t flags = 0;
    uint32_t typeMask = 0;
    void* pClass = nullptr;
    uintptr_t next = 0;
    uintptr_t prev = 0;
    // Internal additions only: the existing 56-byte C ABI remains unchanged.
    char className[kClassNameCapacity] = {};
    uint32_t selfHandle = 0;
};

struct StratIdentity {
    uint32_t addr = 0, pClass = 0, selfHandle = 0;
    uint32_t rawOwnerHandle = 0, ownerAddr = 0;
    uint32_t model = 0, animation = 0, nodeMatrices = 0;
    StratRole role = StratRole::Unknown;
    int ownerFieldKnown = 0;
};
static_assert(sizeof(StratIdentity) == 40, "additive identity ABI");
bool ReadStratIdentity(uintptr_t addr, StratIdentity& out);

struct TraceSegment { float start[3], end[3]; };
struct TraceHit {
    int hit = 0;
    float point[3] = {}, normal[3] = {};
    uint32_t collisionObject = 0;
    uint32_t metadata = 0;
};
static_assert(sizeof(TraceSegment) == 24 && sizeof(TraceHit) == 36, "trace ABI");
bool TraceWorldSegments(uintptr_t exactNinja, const TraceSegment* segments, int count, TraceHit* hits);
bool WriteNinjaFacing(uintptr_t exactNinja, float yawRadians);

bool  ReadStrat(uintptr_t addr, StratView& out);
bool  WriteStratPos(uintptr_t addr, float x, float y, float z);
std::vector<StratView> SnapshotStrats(size_t maxStrats = 8192, bool* complete = nullptr);
size_t CountStrats();
void  EachStrat(const std::function<void(const StratView&)>& fn);

// ---------------------------------------------------------------------------
//  Player detection
// ---------------------------------------------------------------------------

struct PlayerCandidate {
    StratView strat;
    float distanceToCamera = 0;
    bool  selected = false;
    float learnScore = 0;
    float activeSpeed = 0;
    float idleSpeed = 0;
};

class PlayerTracker {
public:
    static PlayerTracker& Get();

    void Update();                        // call once per tick
    void Reset();

    uintptr_t Addr() const;               // 0 when unknown
    bool  Valid() const;
    PlayerMode Mode() const { return mode_; }
    bool Pinned() const { return PinnedMode(mode_) && manual_ && manualValid_; }
    void  SetMode(PlayerMode m);
    void  Select(uintptr_t addr);         // manual selection
    void  BeginLearning();                // six seconds of idle/move/stop; ranks, does NOT pin
    bool  ReacquireExactNinja();          // only after prior Manual/Auto+ pin; complete-list unique exact class
    bool  LearningDone() const { return learnDone_; }
    bool  LearningActive() const { return mode_ == PlayerMode::Learning && learnTotal_ > 0.0f; }
    const std::vector<PlayerCandidate>& Candidates() const { return candidates_; }
    std::string DetectionInfo() const;

private:
    PlayerTracker() = default;
    void RefreshCandidates();
    void AutoPick();

    PlayerMode mode_ = PlayerMode::AutoPlus;
    uintptr_t  manual_ = 0;
    uintptr_t  auto_ = 0;
    void*      knownClass_ = nullptr;
    uint32_t   knownHandle_ = 0;
    bool       manualValid_ = false;
    NinjaPinConsent pinConsent_;
    float      updateTimer_ = 0.0f;

    // learning
    struct Snapshot { uintptr_t addr; void* pClass; uint32_t selfHandle; float pos[3]; MovementEvidence evidence; };
    std::vector<Snapshot> learnStart_;
    float learnTotal_ = 0.0f;
    float learnElapsed_ = 0.0f;
    bool  learnDone_ = false;
    bool  learnRanked_ = false;

    std::vector<PlayerCandidate> candidates_;
    bool candidatesComplete_ = false;
    std::string info_;
};

// ---------------------------------------------------------------------------
//  Save / profile stats
// ---------------------------------------------------------------------------
bool  GetStatInt(int index, int& out);
bool  SetStatInt(int index, int value);
bool  GetStatBit(int index, bool& out);
bool  SetStatBit(int index, bool value);

// ---------------------------------------------------------------------------
//  Script natives table
// ---------------------------------------------------------------------------
uintptr_t NativeAddr(int index);

} // namespace eth::game
