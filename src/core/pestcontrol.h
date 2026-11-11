#pragma once
#include "precomp.h"

namespace eth::patch::pest {
struct Status {
    bool enabled = false, verified = false, faulted = false;
    size_t detected = 0, pending = 0, visited = 0;
    uint64_t searches = 0, requested = 0, removed = 0, refused = 0, budgetStops = 0;
    double searchMs = 0;
    bool listComplete = false;
};
void SetEnabled(bool enabled); // Does not write config or activate HL/display patches.
Status ReadStatus();
void OnNativeTickCompleted(); // Tick thread only, before tracker/mod sampling.
} // namespace eth::patch::pest
