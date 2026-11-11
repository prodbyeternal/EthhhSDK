#include "movement_input_policy.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <limits>

static int checks;
static void Check(bool ok, const char* label) { ++checks; if (!ok) throw std::runtime_error(label); }
int main()
{
    try {
        using namespace eth::input;
        MovementKeysLease lease;
        auto consume = [&](int refusal = 0, float dt = 1.0f / 60) {
            return lease.Consume(refusal != 1, refusal != 2, refusal != 3, refusal != 4,
                refusal == 5, refusal == 6, dt, refusal == 7 ? 0x1100 : 0x1000,
                refusal == 8 ? 0x2100 : 0x2000, refusal == 9 ? 0x124 : 0x123);
        };
        Check(!consume(), "no implicit/persisted lease");
        Check(!lease.Request(0, 0x2000, 0x123), "null player refused");
        Check(!lease.Request(0x1000, 0, 0x123), "null class refused");
        Check(!lease.Request(0x1000, 0x2000, 0), "null handle refused");
        Check(lease.Request(0x1000, 0x2000, 0x123) && consume(), "explicit exact-Ninja lease");
        Check(!consume(), "lease expires before missing/error/disabled callback can reuse it");
        for (int refusal = 1; refusal <= 9; ++refusal) {
            lease.Request(0x1000, 0x2000, 0x123);
            Check(!consume(refusal), "manual/valid/role/focus/F9/Ctrl/address/class/handle refusal");
            Check(!consume(), "refused lease consumed, not delayed until regained focus/identity");
        }
        for (float dt : { 0.0f, -1.0f, 0.11f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
            lease.Request(0x1000, 0x2000, 0x123); Check(!consume(0, dt), "invalid/nonpositive tick refuses lease");
            Check(!consume(), "paused lease cannot reactivate");
        }
        lease.Request(0x1000, 0x2000, 0x123); lease.Reset(); Check(!consume(), "panic/disable/shutdown revoke");
        for (int sample = 0; sample < 80; ++sample) {
            std::array<uint8_t, 520> bytes{}, original{};
            for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = uint8_t(i * 73 + sample * 17);
            original = bytes;
            Check(FilterMovementKeys(bytes.data() + 4, 512, false) && bytes == original, "inactive filter changes no bytes");
            Check(!FilterMovementKeys(bytes.data() + 4, 511, true) && bytes == original, "short keyboard refused without writes");
            Check(FilterMovementKeys(bytes.data() + 4, 512, true), "owned filter succeeds");
            for (size_t i = 0; i < bytes.size(); ++i) {
                const size_t key = i >= 4 ? (i - 4) % 256 : 0;
                const bool masked = i >= 4 && i < 516 && (key == 0x11 || key == 0x1e || key == 0x1f || key == 0x20);
                Check(bytes[i] == (masked ? 0 : original[i]), "only eight WASD down/edge bytes change, sentinels/Ctrl/jump/arrows preserved");
            }
        }
        Check(!FilterMovementKeys(nullptr, 512, true), "null keyboard refused");
        std::printf("PASS native movement input: %d assertions (lease/byte-range policy; not a live keyboard hook)\n", checks);
        return 0;
    } catch (const std::exception& ex) { std::fprintf(stderr, "%s\n", ex.what()); return 1; }
}
