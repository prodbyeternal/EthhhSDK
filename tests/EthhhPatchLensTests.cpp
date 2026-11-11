#include "ethhhpatch_lens_policy.h"
#include "ethhhpatch_camera_policy.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace eth::patch::runtime;
static int checks=0;
static void Check(bool ok,const char* name) { ++checks; if(!ok) { std::fprintf(stderr,"FAIL %s\n",name); std::exit(1); } }
int main() {
    PrimaryViewPass pass;
    constexpr uintptr_t primary=0x43e230, viewAddress=0x1234;
    for(uintptr_t secondary : {0x44ec69u,0x44f576u,0x44fa5cu,0x450953u})
        Check(!pass.Acquire(true,secondary,primary,viewAddress,viewAddress),"secondary before primary never takes camera");
    Check(!pass.Acquire(false,primary,primary,viewAddress,viewAddress),"outside frame remains native");
    Check(!pass.Acquire(true,primary,primary,viewAddress+4,viewAddress),"foreign View rejected");
    Check(pass.Acquire(true,primary,primary,viewAddress,viewAddress),"primary call takes camera once");
    for(int i=0;i<1000;++i) Check(!pass.Acquire(true,primary,primary,viewAddress,viewAddress),"reentry cannot recapture native baseline");
    pass.Reset(); Check(pass.Acquire(true,primary,primary,viewAddress,viewAddress),"zero-tick next frame gets own primary override");

    std::array<float,96> view{};
    for(size_t i=0;i<view.size();++i) view[i]=float(i)*.01f;
    const auto original=view; const uintptr_t va=uintptr_t(view.data());
    int writes=0;
    auto read=[](uintptr_t a,void* b,size_t n) { std::memcpy(b,(void*)a,n); return true; };
    auto write=[&](uintptr_t a,const void* b,size_t n) { ++writes; std::memcpy((void*)a,b,n); return true; };
    RenderLensBackup lens;
    for(float degrees : {30.f,60.f,90.f,120.f}) {
        Check(lens.Apply(va,degrees,read,write),"bounded vertical FOV installed");
        const float radians=degrees*.017453292519943295f;
        Check(std::abs(view[2]-radians)<1e-6f && view[2]==view[3],"projection full vertical angle");
        Check(std::abs(view[4]-std::tan(radians*.5f))<1e-6f &&
            std::abs(view[5]-1/std::cos(radians*.5f))<1e-6f,"culling uses matching half-angle margins");
        for(size_t i=0;i<view.size();++i) if(i<2 || i>=6) Check(view[i]==original[i],"lens write leaves pose/aspect/far fields untouched");
        Check(!lens.Apply(va,70,read,write),"secondary render cannot replace saved lens");
        view[72]=7; // native View_Render rebuilt View+0x120 projection
        Check(lens.Restore(va,read,write) && view==original,"full lens and derived projection restored before tick");
        const int previous=writes; Check(lens.Restore(va,read,write) && writes==previous,"duplicate restore is no-op");
    }
    Check(lens.Apply(va,90,read,write),"foreign lens fixture");
    view[2]=.75f; const auto foreign=view;
    Check(!lens.Restore(va,read,write) && view==foreign,"foreign script lens preserved");
    Check(lens.Apply(va,90,read,write),"replacement fixture");
    const int previous=writes; Check(!lens.Restore(va+4,read,write) && writes==previous,"replaced View never written");
    for(float v : {0.f,29.f,121.f,std::numeric_limits<float>::quiet_NaN()})
        Check(!lens.Apply(va,v,read,write) && !lens.pending,"invalid FOV cannot arm restoration");
    Check(NormalFovDegrees(20)==30 && NormalFovDegrees(130)==120 && NormalFovDegrees(std::numeric_limits<float>::quiet_NaN())==60,"persisted tuning clamped");
    view=original;
    auto failWrite=[](uintptr_t,const void*,size_t) { return false; };
    Check(!lens.Apply(va,90,read,failWrite) && lens.pending,"failed installation retains cleanup obligation");
    Check(lens.Restore(va,read,write) && view==original,"failed write cleanup");
    // Coupled frame fixture: the actual primary gate, pose backup and lens
    // backup coexist. Secondary matrix rebuilds must not recapture baseline.
    std::array<float,64> camera{};
    for(size_t i=0;i<camera.size();++i) camera[i]=float(i)+10;
    const auto originalCamera=camera; const uintptr_t ca=uintptr_t(camera.data());
    float world[16]{},pose[16]{}; world[0]=world[5]=world[10]=world[15]=1;
    pose[3]=pose[7]=pose[11]=pose[15]=1;
    RenderCameraBackup cameraBackup; pass.Reset();
    Check(pass.Acquire(true,primary,primary,va,va) && cameraBackup.Capture(ca,va,read),"frame primary captures native pose baseline");
    write(ca+0x80,world,sizeof(world)); write(va+0x20,pose,sizeof(pose));
    cameraBackup.MarkInstalled(world,pose);
    Check(lens.Apply(va,90,read,write),"FOV and orbit coexist in frame");
    for(uintptr_t secondary : {0x44ec69u,0x44f576u,0x44fa5cu,0x450953u}) {
        Check(!pass.Acquire(true,secondary,primary,va,va) && cameraBackup.pending && cameraBackup.installed,"secondary preserves installed camera endpoints");
        view[24]+=1; view[72]+=1; // renderer-owned View world/projection rebuild
    }
    Check(cameraBackup.Restore(ca,va,read,write) && lens.Restore(va,read,write) &&
        camera==originalCamera && view==original,"frame cleanup restores pose/lens/projection together");
    std::printf("PASS primary render pass / scoped normal FOV: %d assertions\n",checks);
}
