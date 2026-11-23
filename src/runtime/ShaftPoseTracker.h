#pragma once

#include <RE/Skyrim.h>

#include <cmath>
#include <cstdint>
#include <cstring>

namespace SPS::Runtime {

// Tells whether an actor's six shaft bones have stopped moving. SMP rebuilds its
// bodies from the skeleton as it is at reset time, so a reset issued while the
// resting clip is still blending bakes a half-blended pose. One tracker per
// actor: it remembers the previous sample.
//
// Stable() is true when every local rotation changed by less than 0.5 degrees
// since the previous sample (at least ~400 ms apart) twice in a row. It also
// returns true when the bones cannot be read, so a missing node never blocks a
// reset; the callers cap the wait anyway.
class ShaftPoseTracker {
public:
    [[nodiscard]] bool Stable(RE::NiAVObject* root, std::int64_t now)
    {
        if (!root) return true;
        if (havePrev_ && now - lastMs_ < 400) return stableRuns_ >= 2;
        static constexpr const char* names[6] = {
            "NPC Genitals01 [Gen01]", "NPC Genitals02 [Gen02]", "NPC Genitals03 [Gen03]",
            "NPC Genitals04 [Gen04]", "NPC Genitals05 [Gen05]", "NPC Genitals06 [Gen06]" };
        float cur[6][3]{};
        for (int i = 0; i < 6; ++i) {
            auto* node = root->GetObjectByName(RE::BSFixedString(names[i]));
            if (!node) return true;
            node->local.rotate.ToEulerAnglesXYZ(cur[i][0], cur[i][1], cur[i][2]);
        }
        constexpr float deg = 57.2957795f;
        bool steady = havePrev_ && now - lastMs_ < 3000;
        if (steady) {
            for (int i = 0; i < 6 && steady; ++i)
                for (int k = 0; k < 3; ++k)
                    if (std::fabs(cur[i][k] - prev_[i][k]) * deg > 0.5f) { steady = false; break; }
        }
        stableRuns_ = steady ? stableRuns_ + 1 : 0;
        std::memcpy(prev_, cur, sizeof(prev_));
        havePrev_ = true;
        lastMs_ = now;
        return stableRuns_ >= 2;
    }

    // Forget the previous sample, so a new wait never compares against an old one.
    void Reset() { havePrev_ = false; stableRuns_ = 0; lastMs_ = 0; }

private:
    float prev_[6][3]{};
    bool havePrev_{ false };
    std::int64_t lastMs_{ 0 };
    int stableRuns_{ 0 };
};

}
