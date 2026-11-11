#pragma once
#include <algorithm>
#include <cmath>

namespace eth::ui::motion {
// Exact critically damped spring, independent of render frequency. Reversing a
// transition changes only the target: position and velocity remain continuous.
class Transition {
public:
    void Target(bool visible) {target_=visible ? 1.0f : 0.0f;}
    void Snap(bool visible) {value_=target_=visible ? 1.0f : 0.0f;velocity_=0;}
    void Advance(float dt,bool reduced=false) {
        if(reduced) {value_=target_;velocity_=0;return;}
        if(!std::isfinite(dt) || dt<=0) return;
        const float t=std::min(dt,.25f),omega=18.0f;
        const float offset=value_-target_,c=velocity_+omega*offset,e=std::exp(-omega*t);
        value_=target_+(offset+c*t)*e;velocity_=(velocity_-omega*c*t)*e;
        if(std::abs(value_-target_)<.0005f && std::abs(velocity_)<.005f) {value_=target_;velocity_=0;}
    }
    float Value() const {return std::clamp(value_,0.0f,1.0f);}
    bool Active() const {return value_>.0005f || target_>0;}
private:
    float value_=0,velocity_=0,target_=0;
};
}
