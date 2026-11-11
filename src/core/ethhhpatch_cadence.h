#pragma once
#include <cmath>
#include <cstdint>

namespace eth::patch {

inline bool FreshObservation(double now, double observed, double maxAge = 2.5)
{
    return std::isfinite(now) && std::isfinite(observed) && std::isfinite(maxAge) &&
        observed > 0 && maxAge > 0 && now >= observed && now - observed <= maxAge;
}

struct CadenceCounts {
    uint64_t zeroTickFrames = 0, repeatedFramePresents = 0, catchupFrames = 0;
};

// Read-only acceptance evidence. A completed zero-dt/paused tick is still a tick;
// it must not be mistaken for a zero-tick render. Repeated Present of the same
// engine frame likewise is not evidence that a scene was rebuilt at high Hz.
class RenderCadence {
public:
    void Tick() { if (ticks_ != UINT64_MAX) ++ticks_; }
    void GamePresent(uint32_t frame)
    {
        if (haveFrame_) {
            if (frame == frame_) ++counts_.repeatedFramePresents;
            else if (!ticks_) ++counts_.zeroTickFrames;
            else if (ticks_ > 1) ++counts_.catchupFrames;
        }
        frame_ = frame;
        haveFrame_ = true;
        ticks_ = 0;
    }
    void InvalidateBaseline() { haveFrame_ = false; ticks_ = 0; }
    void Reset() { counts_ = {}; InvalidateBaseline(); }
    CadenceCounts Counts() const { return counts_; }
private:
    CadenceCounts counts_;
    uint64_t ticks_ = 0;
    uint32_t frame_ = 0;
    bool haveFrame_ = false;
};

} // namespace eth::patch
