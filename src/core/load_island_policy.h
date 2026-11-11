#pragma once
#include <algorithm>
#include <cmath>

namespace eth::ui::load {
constexpr float kDelay=4.0f;
constexpr float kDuration=4.8f;
struct Rect { float x=0,y=0,w=0,h=0; };
struct Frame { Rect rect; float notice=0,watermark=0,fade=0; bool complete=false; };
inline float Ease(float t) { t=std::clamp(t,0.0f,1.0f); return std::clamp(t*t*t*(t*(t*6-15)+10),0.0f,1.0f); }
inline float Lerp(float a,float b,float t) { return a+(b-a)*t; }
inline Frame Sample(float seconds,float width,float height,float scale,Rect watermark,bool reduced=false) {
    Frame out;
    if (!std::isfinite(seconds) || !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(scale) || scale<=0 || width<16 || height<16 ||
        !std::isfinite(watermark.x) || !std::isfinite(watermark.y) || !std::isfinite(watermark.w) || !std::isfinite(watermark.h)) return out;
    if(seconds<kDelay) return out; // no island, watermark, glyph or sound yet
    seconds-=kDelay;
    const float margin=std::min(12*scale,std::min(width,height)*.05f);
    watermark.w=std::clamp(watermark.w,1.0f,width-2*margin); watermark.h=std::clamp(watermark.h,1.0f,height-2*margin);
    watermark.x=std::clamp(watermark.x,margin,width-margin-watermark.w); watermark.y=std::clamp(watermark.y,margin,height-margin-watermark.h);
    const float duration=reduced ? 2.5f : kDuration;
    if (seconds>=duration) {out.rect=watermark;out.watermark=out.fade=1;out.complete=true;return out;}
    const float appear=Ease(seconds/(reduced ? .15f : .45f));
    const float expand=Ease((seconds-.25f)/.55f);
    const float travel=Ease((seconds-(reduced ? 2.2f : 3.4f))/(reduced ? .3f : 1.4f));
    const float startW=std::min(width-2*margin,Lerp(140*scale,420*scale,expand));
    const float startH=std::min(height-2*margin,Lerp(42*scale,82*scale,expand));
    const float startX=width*.5f,startY=margin+startH*.5f;
    const float endX=watermark.x+watermark.w*.5f,endY=watermark.y+watermark.h*.5f;
    const float bend=reduced ? 0 : std::min(90*scale,height*.12f)*4*travel*(1-travel);
    out.rect.w=Lerp(startW,watermark.w,travel); out.rect.h=Lerp(startH,watermark.h,travel);
    out.rect.x=Lerp(startX,endX,travel)-out.rect.w*.5f;
    out.rect.y=Lerp(startY,endY,travel)+bend-out.rect.h*.5f;
    out.rect.x=std::clamp(out.rect.x,margin,width-margin-out.rect.w);
    out.rect.y=std::clamp(out.rect.y,margin,height-margin-out.rect.h);
    out.fade=appear; out.notice=Ease((seconds-.25f)/.45f)*(1-Ease((travel-.25f)/.30f));
    out.watermark=Ease((travel-.60f)/.40f); return out;
}
class Timeline {
public:
    void Reset() { seconds_=0; }
    void Advance(float dt) { if (std::isfinite(dt) && dt>0) seconds_+=std::min(dt,.05f); }
    float Seconds() const { return seconds_; }
private: float seconds_=0;
};
}
