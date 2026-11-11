// Test-only native ABI. It reads/writes no game process or user configuration.
#include "fake_api.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <cmath>
#include "../src/core/movement_input_policy.h"
#include "../src/core/player_mode_policy.h"

namespace {
struct Camera { uint32_t addr; float x,y,z,fov,param; float world[16],view[16]; };
struct Strat { uint32_t addr; float pos[4],quat[4]; uint32_t flags,type,cls,next; int valid; };
struct Identity { uint32_t addr,cls,self,rawOwner,owner,model,animation,nodes; int role,ownerKnown; };
struct TraceSegment { float start[3], end[3]; };
struct TraceHit { int hit; float point[3],normal[3]; uint32_t object,metadata; };
static_assert(sizeof(Identity) == 40, "identity ABI layout");
static_assert(sizeof(Camera) == 152 && sizeof(Strat) == 56, "native ABI layout");
using Tick = void(__cdecl*)(int);
using Gui = void(__cdecl*)();
using Overlay = void(__cdecl*)(int);
Tick tick = nullptr; Gui gui = nullptr; Overlay overlay = nullptr;
float state[32];
int writes = 0, cameraWrites = 0, errors = 0, ids = 0, styles = 0, ticks = 0;
float fov = 60.0f;
float groundY = 0.0f;
float wallX = 5.0f, ceilingY = INFINITY;
bool groundEnabled = true, wallEnabled = true, traceFailed = false, malformedHit = false;
int traceCalls = 0, maxTraceCount = 0;
int failAtTrace = 0;
eth::input::MovementKeysLease movementLease;
float mouseX = 0, mouseY = 0, nativeWalkX = 0, nativeWalkZ = 0;
bool movementMasked = false;
Camera camera{};
std::map<std::string, std::string> settings;
std::string key(const char* id, const char* k) { return std::string(id ? id : "") + "." + (k ? k : ""); }
}

ETH_CAPI(void, EthhhSDK_EntryGate, (void)) {}
ETH_CAPI(void, Test_Reset, (void)) {
    memset(state, 0, sizeof(state));
    state[0] = state[2] = state[3] = state[4] = state[5] = 1;
    state[25] = 1;
    state[26] = 1; // fixture represents an earlier explicit unique-Ninja pin
    state[6] = 1.0f / 60.0f; state[11] = 0x1000; state[12] = 0x2000;
    state[19] = 1; state[20] = 0x123;
    writes = cameraWrites = errors = ids = styles = ticks = 0; fov = 60;
    groundY = 0;
    wallX = 5; ceilingY = INFINITY;
    groundEnabled = wallEnabled = true; traceFailed = malformedHit = false;
    traceCalls = maxTraceCount = 0;
    failAtTrace = 0;
    movementLease.Reset(); movementMasked = false;
    mouseX = mouseY = nativeWalkX = nativeWalkZ = 0;
    settings.clear(); memset(&camera, 0, sizeof(camera));
    camera.addr = 0x3000; camera.z = -100; camera.y = 55;
    camera.world[0] = camera.world[5] = camera.world[10] = camera.world[15] = 1;
}
ETH_CAPI(void, Test_Set, (int index, float value)) {
    if (index >= 0 && index < 32) { state[index] = value; if(index==15) groundY=value; }
    // Separate raw engine displacement from floor motion: changing the player
    // must not move the fake floor along with a native jump/fall.
    switch(index) {
    case 41: groundY=value; break; case 42: state[15]=value; break;
    case 43: wallX=value; break; case 44: ceilingY=value; break;
    case 45: groundEnabled=value!=0; break; case 46: traceFailed=value!=0; break;
    case 47: malformedHit=value!=0; break; case 48: wallEnabled=value!=0; break;
    case 52: camera.x=state[16]; camera.y=state[15]+55; camera.z=state[17]-100; break;
    case 53: failAtTrace=value>0 ? traceCalls+(int)value : 0; break;
    case 54: camera.x=value; break; case 55: camera.y=value; break; case 56: camera.z=value; break;
    case 57: mouseX=value; break; case 58: mouseY=value; break;
    case 59: nativeWalkX=value; break; case 60: nativeWalkZ=value; break;
    }
}
ETH_CAPI(float, Test_Get, (int index)) {
    switch(index) {
    case 32: return (float)writes; case 33: return (float)cameraWrites;
    case 34: return (float)errors; case 35: return (float)ids; case 36: return (float)styles;
    case 37: return fov;
    case 38: return camera.x; case 39: return camera.y; case 40: return camera.z;
    case 41: return groundY; case 43: return wallX; case 44: return ceilingY;
    case 50: return (float)traceCalls; case 51: return (float)maxTraceCount;
    case 61: return movementMasked ? 1.0f : 0.0f;
    default: if(index>=64 && index<80) return camera.world[index-64];
        return index >= 0 && index < 32 ? state[index] : -1;
    }
}
ETH_CAPI(void, Test_Tick, (void)) {
    movementMasked = movementLease.Consume(eth::game::PinnedMode(eth::game::PlayerMode(int(state[3]))), state[4]!=0 && state[21]==0,
        state[19]==1, state[5]!=0, state[1]!=0, state[27]!=0 || state[8]!=0 || state[9]!=0, state[6],
        (uint32_t)state[11], (uint32_t)state[12], (uint32_t)state[20]);
    if (!movementMasked && (state[10] || state[28] || state[29] || state[30])) {
        state[16] += nativeWalkX; state[17] += nativeWalkZ;
    }
    if (tick) tick(++ticks);
}
ETH_CAPI(void, Test_Gui, (void)) { if (gui) gui(); }
ETH_CAPI(void, Test_Overlay, (int open)) { state[1] = (float)open; if (overlay) overlay(open); }
ETH_CAPI(void, EthLog_Write, (int level, const char* message)) { if (level >= 2) printf("  log: %s\n", message); }
ETH_CAPI(void, EthHooks_SetManagedCallbacks, (Tick t, Gui g, Overlay o)) { tick=t; gui=g; overlay=o; }
ETH_CAPI(int, EthMod_IsEnabled, (const char*)) { return state[0] != 0; }
ETH_CAPI(void, EthMod_ReportError, (const char*, const char* message)) { ++errors; printf("  expected host error: %s\n",message); }
ETH_CAPI(void, EthMod_SetStatus, (const char*, const char*)) {}
ETH_CAPI(int, EthMod_GetSetting, (const char* id, const char* k, char* out, int size)) {
    auto i = settings.find(key(id,k)); if (i == settings.end()) return 0;
    strncpy_s(out, size, i->second.c_str(), _TRUNCATE); return 1;
}
ETH_CAPI(void, EthMod_SetSetting, (const char* id, const char* k, const char* value)) { settings[key(id,k)] = value; }
ETH_CAPI(float, EthMod_GetSettingFloat, (const char* id, const char* k, float def)) {
    auto i = settings.find(key(id,k)); return i == settings.end() ? def : std::stof(i->second);
}
ETH_CAPI(void, EthMod_SetSettingFloat, (const char* id, const char* k, float v)) { settings[key(id,k)] = std::to_string(v); }
ETH_CAPI(int, EthMod_GetSettingBool, (const char* id, const char* k, int def)) {
    auto i = settings.find(key(id,k)); return i == settings.end() ? def : i->second == "1";
}
ETH_CAPI(void, EthMod_SetSettingBool, (const char* id, const char* k, int v)) { settings[key(id,k)] = v ? "1" : "0"; }
ETH_CAPI(void, EthMod_SaveSettings, (void)) {}
ETH_CAPI(int, EthGame_IsReady, (void)) { return state[2] != 0; }
ETH_CAPI(uint32_t, EthGame_PlayerAddr, (void)) { return (uint32_t)state[11]; }
ETH_CAPI(int, EthGame_PlayerMode, (void)) { return (int)state[3]; }
ETH_CAPI(int, EthGame_PlayerValid, (void)) { return state[4] != 0; }
ETH_CAPI(int, EthGame_PlayerReacquireExactNinja, (void)) { if(!eth::game::PinnedMode(eth::game::PlayerMode(int(state[3])))||state[26]==0||state[25]==0||state[19]!=1)return 0;state[4]=1;return 1; }
ETH_CAPI(int, EthGame_ReadStrat, (uint32_t addr, Strat* out)) {
    if (!state[4] || addr != (uint32_t)state[11]) return 0;
    memset(out, 0, sizeof(*out)); out->addr=addr; out->cls=(uint32_t)state[12]; out->valid=1;
    out->pos[0]=state[16]; out->pos[1]=state[15]; out->pos[2]=state[17]; out->quat[3]=1; return 1;
}
ETH_CAPI(int, EthGame_WriteStratPos, (uint32_t addr, float x, float y, float z)) {
    if (state[13] || addr != (uint32_t)state[11] || !std::isfinite(x+y+z)) return 0;
    state[16]=x; state[15]=y; state[17]=z; ++writes; return 1;
}
ETH_CAPI(int, EthGame_ReadStratIdentity, (uint32_t addr, Identity* out)) {
    if (!out) return 0;
    memset(out,0,sizeof(*out));
    if (!state[4] || state[21] || addr != (uint32_t)state[11]) return 0;
    out->addr=addr; out->cls=(uint32_t)state[12]; out->self=(uint32_t)state[20];
    out->role=(int)state[19]; out->model=0x5000; out->animation=0x6000; out->nodes=0x7000;
    if(out->role==2 || out->role==3) { out->rawOwner=0x321; out->owner=0x8000; out->ownerKnown=1; }
    return 1;
}
ETH_CAPI(int, EthGame_TraceSegments, (uint32_t addr, const TraceSegment* segments, int count, TraceHit* hits)) {
    ++traceCalls; if(count>maxTraceCount) maxTraceCount=count;
    if(traceFailed || traceCalls==failAtTrace || !segments || !hits || count<=0 || count>16 || addr!=(uint32_t)state[11] || state[19]!=1) return 0;
    memset(hits,0,sizeof(TraceHit)*count);
    for(int i=0;i<count;++i) {
        const auto& s=segments[i]; float best=2.0f; int axis=-1; float normal=0;
        float dy=s.end[1]-s.start[1];
        if(groundEnabled && std::abs(dy)>1e-8f) { float t=(groundY-s.start[1])/dy; if(t>=0&&t<=1&&t<best){best=t;axis=1;normal=1;} }
        if(std::isfinite(ceilingY) && std::abs(dy)>1e-8f) { float t=(ceilingY-s.start[1])/dy; if(t>=0&&t<=1&&t<best){best=t;axis=1;normal=-1;} }
        float dx=s.end[0]-s.start[0];
        if(wallEnabled && std::abs(dx)>1e-8f) { float t=(wallX-s.start[0])/dx; if(t>=0&&t<=1&&t<best){best=t;axis=0;normal=dx>0?-1.0f:1.0f;} }
        if(axis>=0) {
            hits[i].hit=1; for(int j=0;j<3;++j) hits[i].point[j]=s.start[j]+(s.end[j]-s.start[j])*best;
            hits[i].normal[axis]=normal; hits[i].object=0x9000; hits[i].metadata=0x10000;
            if(malformedHit) hits[i].point[0]=NAN;
        }
    }
    return 1;
}
ETH_CAPI(void, EthPatch_SetExternalCameraOwned, (int owned)) { state[22]=(float)owned; }
ETH_CAPI(int, EthGame_SetNinjaFacing, (uint32_t addr, float yaw)) { if(addr!=(uint32_t)state[11]||!std::isfinite(yaw))return 0;state[23]=yaw;state[24]++;return 1; }
ETH_CAPI(int, EthGame_StratClassName, (uint32_t addr, char* out, int size)) {
    if(!out || size<=0) return 0;
    out[0]=0;
    if(!state[4] || state[21] || addr!=(uint32_t)state[11]) return 0;
    const char* name=state[19]==1 ? "Ninja" : state[19]==2 ? "NinjaRageAttachment" : state[19]==3 ? "Camera" : "Other";
    if(strlen(name)+1>(size_t)size) return 0;
    strcpy_s(out,size,name); return 1;
}
ETH_CAPI(int, EthGame_PlayerDetectionInfo, (char* out, int size)) {
    if(!out || size<=0) return 0;
    strncpy_s(out,size,"test: exact Ninja",_TRUNCATE); return 1;
}
ETH_CAPI(float, EthGame_TimeStep, (void)) { return state[6]; }
ETH_CAPI(int, EthGame_ReadCamera, (Camera* out)) { camera.fov=fov*3.14159265f/180; *out=camera; return 1; }
ETH_CAPI(int, EthGame_SetCameraWorld, (const float* m)) {
    if (state[14]) return 0;
    memcpy(camera.world,m,64); camera.x=m[12]; camera.y=m[13]; camera.z=m[14]; ++cameraWrites; return 1;
}
ETH_CAPI(float, EthGame_CameraFovDegrees, (void)) { return fov; }
ETH_CAPI(int, EthGame_SetCameraFovDegrees, (float v)) { fov=v; return 1; }
ETH_CAPI(int, EthInput_KeyDown, (int vk)) {
    switch(vk) { case 0x56: return state[7] != 0; case 0x71: return state[8] != 0;
    case 0x58: return state[7] != 0; case 0x72: return state[9] != 0;
    case 0x57: return state[10] != 0; case 0x11: return state[27] != 0;
    case 0x53: return state[28] != 0; case 0x41: return state[29] != 0;
    case 0x44: return state[30] != 0; default: return 0; }
}
ETH_CAPI(int, EthInput_LeaseMovementKeys, (uint32_t addr, int enabled)) {
    movementLease.Reset();
    if(!enabled) return 1;
    if(!eth::game::PinnedMode(eth::game::PlayerMode(int(state[3]))) || !state[4] || state[21] || state[19]!=1 || addr!=(uint32_t)state[11]) return 0;
    return movementLease.Request(addr,(uint32_t)state[12],(uint32_t)state[20]) ? 1 : 0;
}
ETH_CAPI(void, EthInput_GetMouseDelta, (float* x, float* y)) { *x=mouseX; *y=mouseY; }
ETH_CAPI(void, EthInput_ConsumeMouseDelta, (void)) { mouseX=mouseY=0; }
ETH_CAPI(int, EthInput_SetCapture, (int on)) { state[18]=(float)on; return on && state[5]; }
ETH_CAPI(int, EthInput_GetCapture, (void)) { return state[18] && state[5] && !state[1]; }
ETH_CAPI(int, EthOverlay_IsOpen, (void)) { return state[1] != 0; }
ETH_CAPI(uint32_t, EthImGui_RGBA, (float r, float g, float b, float a)) {
    return (uint32_t)(r*255) | (uint32_t)(g*255)<<8 | (uint32_t)(b*255)<<16 | (uint32_t)(a*255)<<24;
}
ETH_CAPI(void, EthImGui_PushIDStr, (const char*)) { ++ids; }
ETH_CAPI(void, EthImGui_PopID, (void)) { --ids; }
ETH_CAPI(void, EthImGui_PushStyleVarVec2, (int, float, float)) { ++styles; }
ETH_CAPI(void, EthImGui_PopStyleVar, (int n)) { styles-=n; }
ETH_CAPI(int, EthImGui_CollapsingHeader, (const char* label, int)) { return strcmp(label,"Lifecycle fixture") == 0; }
