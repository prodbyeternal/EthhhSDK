#include "ethhhpatch_camera_policy.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace eth::patch::runtime;
static int checks = 0;
static void Check(bool ok, const char* name) { ++checks; if (!ok) { std::fprintf(stderr, "FAIL %s\n", name); std::exit(1); } }
static bool Near(float a, float b) { return std::abs(a - b) < 0.00002f; }

static void World(float yaw, float pitch, CameraPoint position, float out[16])
{
    const float cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const float m[16] = {cy, 0, -sy, 0, -sp * sy, cp, -sp * cy, 0,
        cp * sy, sp, cp * cy, 0, position.x, position.y, position.z, 1};
    std::memcpy(out, m, sizeof(m));
}

static void Acquire(OrbitPolicy& p, uint32_t& tick, float world[16])
{
    for (int i = 0; i < 3; ++i) p.Sample(++tick, double(tick) / 60, {}, world, 1.0f, false);
    Check(p.state == OrbitState::Following && p.initialized, "stable gameplay acquisition");
}

int main()
{
    // Compare against the actual row-vector quaternion expansion in 0x4B2680,
    // including +/-pi where a matrix->quat helper uses its other diagonal branch.
    for (int y = -32; y <= 32; ++y) for (int p = -12; p <= 8; ++p) {
        const float yaw = y * 0.1f, pitch = p * 0.1f;
        float m[16]{}, q[4]{};
        World(yaw, pitch, {}, m); OrbitQuaternion(yaw, pitch, q);
        const float x = q[0], yy = q[1], z = q[2], w = q[3];
        const float rebuilt[9] = {1-2*(yy*yy+z*z), 2*(x*yy+z*w), 2*(x*z-yy*w),
            2*(x*yy-z*w), 1-2*(x*x+z*z), 2*(yy*z+x*w),
            2*(x*z+yy*w), 2*(yy*z-x*w), 1-2*(x*x+yy*yy)};
        for (int i = 0; i < 9; ++i) Check(Near(rebuilt[i], m[(i/3)*4+i%3]), "View quaternion matches orbit camera basis");
        Check(Near(x*x+yy*yy+z*z+w*w, 1), "unit quaternion");
    }
    float world[16]{}; World(0, 0, {0, 0, -5}, world);
    OrbitPolicy policy; uint32_t tick = 0;
    for (int i = 0; i < 2; ++i) Check(!policy.Sample(++tick, double(tick)/60, {}, world, 1, false), "no early camera takeover");
    Check(policy.Sample(++tick, double(tick)/60, {}, world, 1, false), "three stable simulation samples acquire");
    const float oldYaw = policy.yaw;
    policy.Mouse(12, -4, .002f);
    Check(policy.yaw > oldYaw && policy.pitch > 0, "mouse updates without simulation tick");
    policy.Mouse(std::numeric_limits<float>::quiet_NaN(), 0, .002f);
    Check(std::isfinite(policy.yaw), "invalid raw input refused");
    policy.Mouse(1e7f, 1e7f, .002f);
    Check(std::abs(policy.yaw) <= 3.141593f && policy.pitch >= OrbitPolicy::kMinPitch, "bounded yaw and pitch");
    policy.SetDistance(7.0f); policy.Zoom(2.0f);
    Check(Near(policy.distance, 5.4f) && Near(policy.preferredDistance, 5.4f), "wheel zoom updates persistent preferred distance");
    policy.Zoom(100.0f); Check(Near(policy.distance, policy.minDistance), "zoom clamps near distance");
    policy.Zoom(-100.0f); Check(Near(policy.distance, policy.maxDistance), "zoom clamps far distance");

    World(0, 0, {1, 0, -5}, world);
    const double sampled = double(++tick)/60;
    Check(policy.Sample(tick, sampled, {1, 0, 0}, world, 1, false), "moving target accepted");
    const float a = policy.Target(sampled + .005).x, b = policy.Target(sampled + .010).x;
    Check(a > 0 && b > a && b < 1, "distinct zero-tick interpolated targets");
    Check(Near(policy.Target(sampled + 5).x, 1), "stalls do not extrapolate through geometry");
    Check(Near(policy.Target(sampled - 1).x, 0), "backwards clock bounded");
    policy.Obstruction(1, .01); Check(Near(policy.collisionDistance, 1), "wall retracts immediately");
    policy.Obstruction(5, .01); Check(policy.collisionDistance > 1 && policy.collisionDistance < 2, "wall release damped");
    policy.Obstruction(0, .01); Check(Near(policy.collisionDistance, 0), "no unsafe minimum distance past close hit");

    policy.Reset();
    World(0, 0, {1, 0, -30}, world);
    for (int i = 0; i < 3; ++i) policy.Sample(++tick, double(tick)/60, {1,0,0}, world, 1, false);
    Check(policy.state == OrbitState::Following, "stable distant gameplay framing remains controllable");
    World(0, 0, {1, 0, -50}, world);
    Check(!policy.Sample(++tick, double(tick)/60, {1,0,0}, world, 1, false) && policy.scriptedHold, "extreme cinematic framing yields");
    World(0, 0, {1, 0, -5}, world);
    for (int i = 0; i < 80; ++i) Check(!policy.Sample(++tick, double(tick)/60, {1,0,0}, world, 1, false), "scripted hold protects the camera temporarily");
    for (int i = 0; i < 16; ++i) policy.Sample(++tick, double(tick)/60, {1,0,0}, world, 1, false);
    Check(policy.state == OrbitState::Following && !policy.scriptedHold, "stable gameplay automatically reacquires after hold");

    World(3.1415926f, 0, {1, 0, -5}, world);
    Check(!policy.Sample(++tick, double(tick)/60, {1,0,0}, world, 1, false), "camera cut yields before taking control");
    policy.OwnerChanged();
    Check(policy.scriptedHold && !policy.initialized && policy.state == OrbitState::Waiting,
        "camera replacement cannot erase an active cinematic hold");
    policy.RequestResume();
    World(0, 0, {0, 0, -5}, world);
    Acquire(policy, tick, world);
    policy.RequestResume();
    Check(policy.state == OrbitState::Following, "manual resume is harmless while already following");
    policy.OwnerChanged();
    Check(!policy.initialized && !policy.scriptedHold && policy.state == OrbitState::Waiting,
        "ordinary owner replacement requires fresh stable gameplay");
    policy.Reset(); World(0, 0, {0,0,-5}, world);
    world[0]=world[5]=std::cos(.3f); world[1]=std::sin(.3f); world[4]=-std::sin(.3f);
    Acquire(policy, tick, world);
    Check(policy.state == OrbitState::Following, "moderate native camera roll does not disable free look");
    policy.Reset(); World(0, 0, {0,0,-5}, world); Acquire(policy, tick, world);
    Check(!policy.Sample(++tick, double(tick)/60, {}, world, 1.2f, false), "abrupt cinematic lens cut yields");
    policy.RequestResume();
    for (int i = 0; i < 3; ++i) policy.Sample(++tick, double(tick)/60, {}, world, 1.2f, false);
    Check(policy.state == OrbitState::Following, "manual resume bypasses hold after stable samples");
    policy.Reset(); Acquire(policy, tick, world); policy.Suspend(OrbitState::Movie);
    Check(!policy.initialized && policy.state == OrbitState::Movie, "movie drops camera initialization");
    // Same frame may render repeatedly: cannot satisfy the tick-based reacquire gate.
    for (int i = 0; i < 100; ++i) Check(!policy.Sample(tick, double(tick)/60, {}, world, 1, false), "render frames cannot count as gameplay samples");
    Acquire(policy, tick, world); policy.Suspend(OrbitState::External);
    Check(!policy.initialized && policy.state == OrbitState::External, "HL handoff yields");
    policy.Suspend(OrbitState::Paused); Check(!policy.initialized, "pause releases orbit state");
    policy.Reset(); Check(!policy.Sample(0, 0, {}, world, 1, false), "tick zero is valid first sample not initialized target");
    Check(!policy.Sample(1, 1.0/60, {}, world, 1, true), "replacement/teleport invalidates target history");

    policy.Reset(); World(0,0,{0,0,-5},world); Acquire(policy,tick,world);
    for (int i=1;i<=300;++i) {
        const float yaw=i*.01f;
        World(yaw,0,{-std::sin(yaw)*5,0,-std::cos(yaw)*5},world);
        Check(policy.Sample(++tick,double(tick)/60,{},world,1,false),"ordinary native orbit is not a scripted pan");
    }
    for (int index=0;index<16;++index) {
        policy.Reset(); World(0,0,{0,0,-5},world);
        world[index]=std::numeric_limits<float>::quiet_NaN();
        Check(!policy.Sample(++tick,double(tick)/60,{},world,1,false) && policy.state==OrbitState::Unavailable,
            "non-finite full camera matrix refused");
    }
    policy.Reset(); World(0,0,{0,0,-5},world); world[4]=.25f;
    Check(!policy.Sample(++tick,double(tick)/60,{},world,1,false),"malformed camera basis refused");
    policy.Reset(); World(0,0,{5,0,-5},world); Acquire(policy,tick,world);
    for (int i=1;i<=12;++i) { World(0,0,{5+i*.04f,0,-5},world); policy.Sample(++tick,double(tick)/60,{},world,1,false); }
    Check(policy.state==OrbitState::Scripted,"sustained off-target pan yields");
    const float collisionBefore=policy.collisionDistance;
    policy.Obstruction(9,std::numeric_limits<double>::quiet_NaN());
    Check(Near(policy.collisionDistance,collisionBefore),"invalid collision time cannot poison camera");

    std::array<unsigned char, 0x140> camera{};
    std::array<unsigned char, 0x180> view{};
    for (size_t i = 0; i < camera.size(); ++i) camera[i] = (unsigned char)i;
    for (size_t i = 0; i < view.size(); ++i) view[i] = (unsigned char)(i * 3);
    const auto originalCamera = camera;
    const auto originalView = view;
    const uintptr_t ca = (uintptr_t)camera.data(), va = (uintptr_t)view.data();
    int writes = 0;
    auto read = [](uintptr_t a, void* b, size_t n) { std::memcpy(b, (void*)a, n); return true; };
    auto write = [&](uintptr_t a, const void* b, size_t n) { ++writes; std::memcpy((void*)a, b, n); return true; };
    RenderCameraBackup backup;
    Check(backup.Capture(ca, va, read), "capture verified camera/View regions");
    Check(!backup.Capture(ca, va, read), "nested capture cannot overwrite native baseline");
    std::memset(camera.data(), 0, 0x40); std::memset(camera.data()+0x80, 0, 0x80);
    std::memset(view.data()+0x20, 0, 0x100);
    Check(backup.Restore(ca, va, read, write), "frame cleanup restore");
    Check(camera == originalCamera && view == originalView && writes == 3, "exact byte restoration including derived matrices");
    Check(backup.Restore(ca, va, read, write) && writes == 3, "duplicate cleanup no-op");
    auto unreadable = [](uintptr_t, void*, size_t) { return false; };
    Check(!backup.Capture(ca, va, unreadable) && !backup.pending, "partial capture cannot arm a restore");
    Check(backup.Capture(ca, va, read), "recapture");
    Check(!backup.Restore(ca+4, va, read, write) && writes == 3 && !backup.pending, "replaced object is never written");
    Check(backup.Capture(ca, va, read), "capture for failed write cleanup");
    int attempted = 0;
    auto failedWrite = [&](uintptr_t, const void*, size_t) { return ++attempted != 1; };
    Check(!backup.Restore(ca, va, read, failedWrite) && attempted == 3, "failed region still attempts remaining restorations");
    Check(backup.Capture(ca, va, read), "guarded capture");
    float ownWorld[16]{}, ownPose[16]{};
    World(0, 0, {}, ownWorld);
    std::memcpy(camera.data()+0x80, ownWorld, sizeof(ownWorld));
    std::memcpy(view.data()+0x20, ownPose, sizeof(ownPose));
    backup.MarkInstalled(ownWorld, ownPose);
    camera[0x80] = 42; // another script/mod changed Camera.world after our write
    const auto foreignCamera = camera;
    Check(!backup.Restore(ca, va, read, write), "intervening camera ownership detected");
    Check(camera == foreignCamera && view == originalView, "foreign camera edits preserved; our View override restored");
    Check(backup.Capture(ca, va, read), "guarded success capture");
    std::memcpy(camera.data()+0x80, ownWorld, sizeof(ownWorld));
    std::memcpy(view.data()+0x20, ownPose, sizeof(ownPose));
    backup.MarkInstalled(ownWorld, ownPose);
    Check(backup.Restore(ca, va, read, write), "guarded unchanged owner restore");
    Check(camera == foreignCamera && view == originalView, "guarded restore exactly restores native baseline");
    Check(backup.Capture(ca, va, read), "foreign View capture");
    std::memcpy(camera.data()+0x80, ownWorld, sizeof(ownWorld));
    std::memcpy(view.data()+0x20, ownPose, sizeof(ownPose));
    backup.MarkInstalled(ownWorld, ownPose);
    view[0x20] = 43;
    const auto foreignView = view;
    Check(!backup.Restore(ca, va, read, write), "intervening View ownership detected");
    Check(camera == foreignCamera && view == foreignView, "foreign View edit retained; our Camera override restored");
    Check(backup.Capture(ca, va, read), "capture for replaced View");
    const int beforeReplacement = writes;
    Check(!backup.Restore(ca, va+4, read, write) && writes == beforeReplacement && !backup.pending,
        "replaced View refuses all stale restoration writes");
    std::printf("PASS EthhhPatch camera policy/restore: %d assertions\n", checks);
}
