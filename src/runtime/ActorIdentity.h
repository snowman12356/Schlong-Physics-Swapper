#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace SPS::Runtime {

// SOS AE schlong and SexLab P+ sex for an actor SPS is tracking (the player and
// managed NPCs), plus the per-save list of actors the user made futa in SexLab.
//
// Sources (verified against the installed scripts):
//   SOS AE 4.0  SOSAE_SKSE.GetActorAddonName / GetActorRank (read-only getters)
//   SexLab P+   SexLabRegistry.GetSex(actor, ignoreOverwrite)  0 M, 1 F, 2 futa
//               SexLabFramework.TreatAsFuta / ClearForcedSex
// SexLab keeps its override only as a faction rank on the actor reference, so it
// is lost when the reference is rebuilt. SPS remembers the choice in its own
// co-save and re-applies it.
struct ActorIdentity {
    std::uint32_t formID{ 0 };
    bool known{ false };      // a describe call has returned for this session
    bool sosRead{ false };
    std::string addon;        // "" = no SOS schlong
    int rank{ 0 };
    bool sexLabRead{ false };
    int baseSex{ -1 };        // SexLab mapping, ignoring overrides
    int sex{ -1 };            // SexLab mapping, including overrides
    bool rememberedFuta{ false };
    bool pending{ false };
};

// Female base sex with a SOS schlong (or SOS not readable): SexLab futa applies.
[[nodiscard]] bool FutaEligible(const ActorIdentity& identity);
[[nodiscard]] const char* SexLabSexName(int sex);

// Game thread. `watched` is the player plus managed NPCs; remembered futa actors
// are added internally so a reset reference is re-applied when it is loaded.
void TickActorIdentities(const std::vector<std::uint32_t>& watched, std::int64_t allowedAfterMs);
// Thread-safe. Queues a SexLab futa set/clear for the next tick.
bool RequestFuta(std::uint32_t formID, bool futa);
// Thread-safe snapshot. Empty until the actor has been described or remembered.
[[nodiscard]] std::optional<ActorIdentity> ReadActorIdentity(std::uint32_t formID);
// Load boundary: drops cached descriptions and in-flight results. The per-save
// futa list is owned by the co-save callbacks, not by this.
void ResetActorIdentities();
// Called once from plugin load.
void RegisterActorIdentitySerialization();
// Papyrus registration: ResolveIdentityActor on SPS_IdentityBridge and SPS_SexLabBridge.
bool RegisterActorIdentityNatives(RE::BSScript::IVirtualMachine* vm);

}
