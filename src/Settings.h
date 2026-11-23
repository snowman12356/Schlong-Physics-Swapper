#pragma once

namespace SPS::Core {

struct Settings {
    bool enabled{ true };
    bool basicNpcEnabled{ false };
    bool basicNpcProviderSosDisabled{ false };
    // One fully managed NPC, per Stage 3. Separate from the basic bridge flag:
    // enrolling an NPC transfers responsibility rather than adding behaviour.
    bool managedNpcEnabled{ false };
    // Raises the managed NPC limit from 1 to 8. Off by default.
    bool managedNpcUpToEight{ false };
    float threshold{ 60.0F };
    float hysteresis{ 5.0F };
    int mode{ 0 };  // 0 automatic, 1 force SMP, 2 force CBPC
    int erectBend{ 14 };
    bool flaccidAngleControl{ false };
    int flaccidBend{ 0 };
    int pollMs{ 1000 };
    bool sexLabOverride{ true };
    bool sexLabRoleSwitching{ true };
    int sexLabBottomBehavior{ 0 };  // 0 keep entry, 1 live arousal, 2 SMP, 3 CBPC
    int sexLabUnknownRole{ 0 };  // 0 keep current, 1 SMP, 2 CBPC
    int sceneEndDelayMs{ 1500 };
    bool ostimOverride{ true };
    bool ostimRoleSwitching{ true };
    int switchCooldownMs{ 750 };
    bool resetSMPAfterLoad{ true };
    int loadResetDelayMs{ 10000 };
    bool positionControl{ true };
    int bendMethod{ 2 };  // 0 native, 1 animation event, 2 automatic, 3 SPS pose angle
    // Degrees at bend 20 for the SPS pose angle. The tilt is applied on top of
    // the captured base pose, so this only sets how far from neutral it travels.
    float poseAngleDegrees{ 45.0F };
    bool animatePosition{ true };
    bool gradualErection{ true };
    bool arousalBasedErection{ false };
    float erectionStartArousal{ 20.0F };
    int erectionDurationMs{ 3000 };
    int softeningDurationMs{ 5000 };
    bool randomErections{ false };
    int randomErectionMinMinutes{ 15 };
    int randomErectionMaxMinutes{ 45 };
    int randomErectionDurationSeconds{ 60 };
    bool randomErectionSafeMoments{ true };
    bool spontaneousRefractory{ true };
    int refractoryMinutes{ 5 };
    bool morningErections{ false };
    int morningErectionDurationSeconds{ 90 };
    bool equipmentChangeRecovery{ true };
    bool bounceGuard{ true };
    bool useSexLabBend{ false };
    int sexLabBend{ 14 };
    int settleDelayMs{ 350 };
    int maxBendFailures{ 3 };
    bool verboseLogging{ false };

    bool operator==(const Settings&) const = default;
};

}
