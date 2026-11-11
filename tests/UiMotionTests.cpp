#include "ui_motion_policy.h"
#include "load_island_policy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

static int checks=0;
static void Check(bool value,const char* label) {++checks;if(!value) {fprintf(stderr,"FAIL %s\n",label);exit(1);}}
int main() {
    using eth::ui::motion::Transition;
    for(int hz:{60,100,144,240,360,600,1000}) {
        Transition t;t.Target(true);float previous=0;
        for(int i=0;i<hz;++i) {t.Advance(1.0f/hz);Check(t.Value()>=previous,"opening never duplicates/overshoots the surface");previous=t.Value();}
        Check(t.Value()>.999f,"opening settles independently of refresh rate");
        t.Target(false);previous=t.Value();
        for(int i=0;i<hz;++i) {t.Advance(1.0f/hz);Check(t.Value()<=previous,"closing is monotonic");previous=t.Value();}
        Check(!t.Active(),"closed manager stops drawing and sampling");
        for(int i=0;i<200;++i) {const float before=t.Value();t.Target(i%2==0);Check(t.Value()==before,"rapid reversal does not restart position");t.Advance(1.0f/hz);Check(t.Value()>=0 && t.Value()<=1,"reversal remains bounded");}
        t.Target(true);t.Advance(1.0f/hz,true);Check(t.Value()==1,"reduced motion opens immediately");
        t.Target(false);t.Advance(1.0f/hz,true);Check(!t.Active(),"reduced motion closes immediately");
    }
    Transition a,b;a.Target(true);b.Target(true);a.Advance(.10f);for(int i=0;i<10;++i) b.Advance(.01f);
    Check(std::abs(a.Value()-b.Value())<.00001f,"analytic spring is invariant to frame partitioning");
    const float before=a.Value();a.Advance(std::numeric_limits<float>::quiet_NaN());a.Advance(-1);Check(a.Value()==before,"invalid elapsed time cannot corrupt motion");
    using namespace eth::ui::load;
    for(float s:{.75f,1.0f,1.5f,2.0f,2.5f}) for(int width:{320,640,1152,1920,3840}) for(int height:{240,480,704,1080,2160})
    for(int corner=0;corner<4;++corner) for(bool reduced:{false,true}) {
        Rect target{corner%2 ? float(width)-12*s-280*s : 12*s,corner<2 ? 12*s : float(height)-12*s-100*s,280*s,100*s};
        for(int i=0;i<=1000;++i) {
            const float seconds=i*.01f;const Frame f=Sample(seconds,float(width),float(height),s,target,reduced);
            if(seconds<kDelay) {Check(f.fade==0 && f.notice==0 && f.watermark==0 && f.rect.w==0,"four-second quiet period, independent of monitor refresh");continue;}
            Check(std::isfinite(f.rect.x) && std::isfinite(f.rect.y) && f.rect.w>0 && f.rect.h>0,"island geometry remains finite");
            Check(f.rect.x>=0 && f.rect.y>=0 && f.rect.x+f.rect.w<=width+.01f && f.rect.y+f.rect.h<=height+.01f,"every animation stage stays on screen");
            Check(f.notice*f.watermark<.0001f,"welcome and watermark never appear as overlapping duplicates");
            Check(f.fade>=0 && f.fade<=1 && f.notice>=0 && f.notice<=1 && f.watermark>=0 && f.watermark<=1,"all optical/content fades bounded");
        }
        const Frame end=Sample(20,float(width),float(height),s,target,reduced);Check(end.complete && end.watermark==1,"island finishes as the exact configured watermark");
    }
    Check(Sample(5,640,480,std::numeric_limits<float>::quiet_NaN(),{}).fade==0,"invalid UI scale fails safely");
    printf("PASS UI motion/delayed single-island geometry: %d assertions\n",checks);return 0;
}
