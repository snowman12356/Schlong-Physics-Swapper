#include "controllers/PoseAngle.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

int RunPoseAngleTests()
{
    using namespace SPS::Core;
    int failures = 0;
    const auto expect = [&](bool condition, std::string_view message) {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    };

    const auto near = [](float a, float b) { return std::abs(a - b) < 0.001F; };

    expect(near(PoseAngle::Degrees(0), 0.0F), "bend 0 is the neutral reference");
    expect(near(PoseAngle::Degrees(20), 45.0F), "bend 20 reaches the default full angle");
    expect(near(PoseAngle::Degrees(10), 22.5F), "bend maps linearly onto degrees");
    // Continuity is the reason this channel exists: the graph only has ten steps.
    expect(near(PoseAngle::Degrees(1), 2.25F) && near(PoseAngle::Degrees(19), 42.75F),
        "intermediate bends produce distinct angles rather than snapping to stages");

    expect(near(PoseAngle::Degrees(-5), 0.0F) && near(PoseAngle::Degrees(200), 45.0F),
        "out-of-range bend is clamped instead of extrapolated");

    expect(near(PoseAngle::Degrees(20, 90.0F), 90.0F) && near(PoseAngle::Degrees(10, 90.0F), 45.0F),
        "a configured full angle rescales the whole range");
    expect(near(PoseAngle::Degrees(20, -45.0F), -45.0F) && near(PoseAngle::Degrees(10, -45.0F), -22.5F),
        "a negative full angle reverses the direction with the same magnitude");
    expect(near(PoseAngle::Degrees(20, 0.0F), 0.0F),
        "a zero full angle holds neutral instead of dividing by zero");

    expect(near(PoseAngle::Degrees(20, 500.0F), 90.0F),
        "an absurd configured angle is capped at the maximum");
    expect(near(PoseAngle::Degrees(20, std::numeric_limits<float>::quiet_NaN()), 0.0F),
        "a NaN configured angle degrades to neutral rather than producing NaN rotation");

    expect(PoseAngle::BendFor(0.0F) == 0 && PoseAngle::BendFor(45.0F) == 20 &&
        PoseAngle::BendFor(22.5F) == 10,
        "degrees round-trip back to the bend that produced them");
    expect(PoseAngle::BendFor(-22.5F) == 10,
        "the reported bend ignores direction because bend is always non-negative");
    expect(PoseAngle::BendFor(45.0F, 0.0F) == 0,
        "an unusable scale reports neutral instead of a division result");

    expect(PoseAngle::ValidDegrees(45.0F) && PoseAngle::ValidDegrees(-90.0F) &&
        !PoseAngle::ValidDegrees(90.1F) && !PoseAngle::ValidDegrees(std::nanf("")) &&
        !PoseAngle::ValidDegrees(std::numeric_limits<float>::infinity()),
        "only finite angles inside the documented range are accepted");

    return failures;
}
