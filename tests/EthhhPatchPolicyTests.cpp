#include "ethhhpatch_policy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace eth::patch;
static int checks=0;
static void Check(bool ok,const char* label) { ++checks; if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);} }
int main()
{
    Check(ValidPlan({1920,1080,100}) && ValidPlan({3840,2160,0}),"1080p/4K plans");
    for(DisplayPlan p : {DisplayPlan{0,1080,100},{1920,0,100},{8000,2160,100},{1920,1080,1000},{1920,1080,-1}})
        Check(!ValidPlan(p) && !TargetRenderHz(p,100),"invalid display plans refused");
    Check(TargetRenderHz({1920,1080,0},100)==100,"monitor target stays 100 Hz, not 60");
    Check(!TargetRenderHz({1920,1080,0},0),"unknown monitor refresh remains unknown");
    Check(TargetRenderHz({1920,1080,144},100)==144,"explicit render target independent of simulation");
    DisplayPlan fullscreen{1920,1080,100,false}, windowed{1920,1080,0,true};
    Check(AppliedModeMatches(fullscreen,100,1920,1080,false,100),"exact fullscreen result accepted");
    Check(!AppliedModeMatches(fullscreen,100,1920,1080,false,60) &&
        !AppliedModeMatches(fullscreen,100,1280,720,false,100),"fullscreen fallback mismatch detected");
    Check(AppliedModeMatches(windowed,100,1920,1080,true,0) &&
        !AppliedModeMatches(windowed,100,1920,1080,false,100),"windowed result requires desktop-refresh semantics");
    Check(std::abs(Aspect(1920,1080)-16.0/9)<1e-12 && Aspect(0,0)==0,"aspect guards");
    Check(std::abs(HorizontalFov(60,16.0/9)-91.49284452)<1e-6,"vertical-to-horizontal FOV math only");
    Check(HorizontalFov(60,16.0/9)>HorizontalFov(60,4.0/3),"Hor+ mathematical policy");
    Check(!HorizontalFov(std::numeric_limits<double>::quiet_NaN(),1) && !HorizontalFov(180,1) && !HorizontalFov(60,0),"invalid FOV preview guards");
    for(int renderHz : {30,60,100,120,144,165,240,360}) {
        Fixed60Prototype scheduler;
        int ticks=0,zeroTickFrames=0;
        for(int frame=0;frame<renderHz*10;++frame) {
            auto step=scheduler.Advance(1.0/renderHz);
            ticks+=step.ticks;
            zeroTickFrames+=step.ticks==0;
            Check(step.ticks>=0 && step.ticks<=4 && !step.dropped,"bounded fixed simulation step");
            Check(step.alpha>=0 && step.alpha<1,"bounded interpolation alpha");
        }
        Check(ticks==600,"ten seconds always 600 simulation ticks");
        if(renderHz>60) Check(zeroTickFrames>0,"high-refresh needs render-only frames");
    }
    Fixed60Prototype scheduler;
    Check(scheduler.Advance(0).ticks==0,"zero time never forces a tick");
    Check(!scheduler.Advance(-1).ticks && !scheduler.Advance(std::numeric_limits<double>::infinity()).ticks &&
        !scheduler.Advance(std::numeric_limits<double>::quiet_NaN()).ticks,"invalid time refused");
    auto stall=scheduler.Advance(1.005);
    Check(stall.ticks==4 && stall.dropped==56 && std::abs(stall.alpha-.3)<1e-8,"bounded catchup with explicit debt policy");
    scheduler.Reset(); Check(scheduler.Advance(1.0/100).ticks==0,"reset clears tick debt");
    Check(kSimulationStep==1.0/60,"simulation interval unchanged");
    printf("PASS EthhhPatch policy: %d assertions (prototype NOT installed in App_Run)\n",checks);
}
