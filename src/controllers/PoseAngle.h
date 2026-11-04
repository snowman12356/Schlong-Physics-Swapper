#pragma once

#include <algorithm>
#include <cmath>

namespace SPS::Core {

// Angle mapping for the SPS pose angle.
//
// Pure and game-independent so the range and sign behaviour can be tested
// without CommonLib or a running game. SosPoseAngle applies the result to the
// base bone; nothing here knows about skeletons or matrices.
struct PoseAngle {
    static constexpr int kMaxBend = 20;
    static constexpr float kDefaultDegreesAtMaxBend = 45.0F;
    static constexpr float kMaxDegrees = 90.0F;

    // Clamps bend to the SOS range 0-20 and scales it onto the degrees at full
    // bend. The result is continuous, which is the point of this channel: the
    // graph events it replaces only have ten states.
    static float Degrees(int bend, float degreesAtMaxBend = kDefaultDegreesAtMaxBend)
    {
        const int clamped = std::clamp(bend, 0, kMaxBend);
        // std::clamp propagates NaN, so a non-finite configured angle is rejected
        // before it can reach a rotation matrix.
        if (!std::isfinite(degreesAtMaxBend)) {
            return 0.0F;
        }
        const float magnitude = std::clamp(
            std::abs(degreesAtMaxBend), 0.0F, kMaxDegrees);
        const float scaled =
            static_cast<float>(clamped) / static_cast<float>(kMaxBend) * magnitude;
        // A negative full-bend angle flips the direction of travel, so the sign
        // is preserved instead of being folded into the magnitude.
        return degreesAtMaxBend < 0.0F ? -scaled : scaled;
    }

    // Bend that produces the given degrees, used to report a state in the units
    // the rest of the plugin uses.
    static int BendFor(float degrees, float degreesAtMaxBend = kDefaultDegreesAtMaxBend)
    {
        if (!std::isfinite(degrees) || !std::isfinite(degreesAtMaxBend)) {
            return 0;
        }
        const float magnitude = std::clamp(
            std::abs(degreesAtMaxBend), 0.0F, kMaxDegrees);
        if (magnitude <= 0.0F) {
            return 0;
        }
        const float bend = std::abs(degrees) / magnitude * static_cast<float>(kMaxBend);
        return std::clamp(static_cast<int>(std::lround(bend)), 0, kMaxBend);
    }

    // Degrees are only meaningful inside the configured range. Used to reject a
    // request rather than silently clamping a value someone meant differently.
    static bool ValidDegrees(float degreesAtMaxBend)
    {
        return std::isfinite(degreesAtMaxBend) &&
            std::abs(degreesAtMaxBend) <= kMaxDegrees;
    }
};

}
