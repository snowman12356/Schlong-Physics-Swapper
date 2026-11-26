#include "ActorIdentity.h"

#include "Compatibility.h"
#include "ManagedNpcRuntime.h"
#include "PapyrusGateway.h"
#include "SPSAPI.h"

#include <SKSE/SKSE.h>
#include <fmt/format.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <filesystem>
#include <functional>
#include <mutex>
#include <set>
#include <unordered_map>

namespace SPS::Runtime {
namespace {

constexpr std::uint32_t Tag(char a, char b, char c, char d)
{
    return (static_cast<std::uint32_t>(a) << 24) | (static_cast<std::uint32_t>(b) << 16) |
        (static_cast<std::uint32_t>(c) << 8) | static_cast<std::uint32_t>(d);
}
// Co-save identity. Unique to SPS among SKSE plugins ("SPSW" = SPS swapper).
constexpr std::uint32_t kSerializationID = Tag('S', 'P', 'S', 'W');
constexpr std::uint32_t kFutaRecord = Tag('F', 'U', 'T', 'A');
constexpr std::uint32_t kFutaRecordVersion = 1;
constexpr std::uint32_t kMaxRemembered = 256;
// Managed NPC membership for this save: count then FormIDs, at most kManagedNpcMaxCapacity.
constexpr std::uint32_t kManagedRecord = Tag('M', 'N', 'P', 'C');
constexpr std::uint32_t kManagedRecordVersion = 1;

constexpr int kSexLabBridgeVersion = 9;
// SexLab-free identity reads (SOS addon and, through SPS_SexLabBridge, SexLab sex).
constexpr int kIdentityBridgeVersion = 1;
constexpr std::int64_t kDescribeEveryMs = 5000;
constexpr std::int64_t kReapplyEveryMs = 10000;

struct Entry {
    ActorIdentity identity;
    std::int64_t describedAt{ -1 };
    std::int64_t reappliedAt{ -1 };
};

std::mutex lock;
std::unordered_map<std::uint32_t, Entry> entries;
std::set<std::uint32_t> remembered;          // this save's futa list
std::vector<std::pair<std::uint32_t, bool>> futaRequests;
std::uint64_t generation{ 1 };
std::atomic<int> bridgeVersion{ 0 };          // SexLab bridge: 0 unknown, -1 mismatch
std::atomic<bool> versionPending{ false };
std::atomic<int> identityVersion{ 0 };        // identity bridge: 0 unknown, -1 mismatch
std::atomic<bool> identityVersionPending{ false };

class Callback final : public RE::BSScript::IStackCallbackFunctor {
public:
    explicit Callback(std::function<void(const RE::BSScript::Variable&)> fn) : fn_(std::move(fn)) {}
    void operator()(RE::BSScript::Variable result) override { if (fn_) fn_(result); }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
private:
    std::function<void(const RE::BSScript::Variable&)> fn_;
};

RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> MakeCallback(
    std::function<void(const RE::BSScript::Variable&)> fn)
{
    return RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>{ new Callback(std::move(fn)) };
}

std::string NameOf(std::uint32_t formID)
{
    if (formID == SPS::API::kPlayerFormID) return "Player";
    const auto actor = RE::TESForm::LookupByID<RE::Actor>(formID);
    const char* name = actor ? actor->GetDisplayFullName() : nullptr;
    return name && name[0] != '\0' ? name : "Unnamed NPC";
}

// Feedback only, so it runs as a game task and is dropped if the VM is busy.
void Notify(std::string text, std::int64_t allowedAfterMs)
{
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([text = std::move(text), allowedAfterMs] {
            (void)DispatchStatic("Debug", "Notification", allowedAfterMs, NowMs(),
                RE::BSFixedString(text.c_str()));
        });
    }
}

bool ReadSexLab()
{
    return PluginLoaded({ "SexLab.esm" }) &&
        std::filesystem::exists("Data/Scripts/SPS_SexLabBridge.pex");
}

// The identity bridge is called with or without SexLab, so it holds no SexLab
// types. SPS_SexLabBridge declares them and is only reached when SexLab is
// loaded (2.3.0.3 rule: never declare an optional mod's type in a bridge that
// is called without that mod).
bool IdentityBridgeInstalled()
{
    const bool installed = std::filesystem::exists("Data/Scripts/SPS_IdentityBridge.pex");
    // Logged once per process so a partial install is visible in support logs.
    static std::atomic<bool> missingLogged{ false };
    if (!installed && !missingLogged.exchange(true))
        SKSE::log::error("SPS-013: SPS_IdentityBridge.pex is missing; schlong and SexLab identity reads are off. "
            "Install the paired DLL and scripts");
    return installed;
}

// "<addon>|<rank>|<baseSex>|<sex>", parsed from the right so an addon name
// containing '|' cannot shift the numbers.
bool Parse(std::string_view text, ActorIdentity& out)
{
    int numbers[3]{};
    for (int i = 2; i >= 0; --i) {
        const auto bar = text.rfind('|');
        if (bar == std::string_view::npos) return false;
        const auto field = text.substr(bar + 1);
        const auto result = std::from_chars(field.data(), field.data() + field.size(), numbers[i]);
        if (result.ec != std::errc{}) return false;
        text = text.substr(0, bar);
    }
    out.addon = std::string(text);
    out.rank = numbers[0];
    out.baseSex = numbers[1];
    out.sex = numbers[2];
    return true;
}

bool EnsureBridge(std::int64_t allowedAfterMs, std::int64_t now)
{
    const int version = bridgeVersion.load();
    if (version != 0) return version == kSexLabBridgeVersion;
    if (versionPending.exchange(true)) return false;
    const bool sent = DispatchStaticWithCallback("SPS_SexLabBridge", "GetSPSBridgeVersion",
        allowedAfterMs, now, MakeCallback([](const RE::BSScript::Variable& result) {
            const int value = result.IsInt() ? result.GetSInt() : -1;
            bridgeVersion.store(value == kSexLabBridgeVersion ? value : -1);
            versionPending.store(false);
            if (value != kSexLabBridgeVersion)
                SKSE::log::error("SPS-013: SexLab bridge version {} found, {} required; install the paired DLL and scripts",
                    value, kSexLabBridgeVersion);
        }));
    if (!sent) versionPending.store(false);
    return false;
}

bool EnsureIdentityBridge(std::int64_t allowedAfterMs, std::int64_t now)
{
    const int version = identityVersion.load();
    if (version != 0) return version == kIdentityBridgeVersion;
    if (identityVersionPending.exchange(true)) return false;
    const bool sent = DispatchStaticWithCallback("SPS_IdentityBridge", "GetSPSBridgeVersion",
        allowedAfterMs, now, MakeCallback([](const RE::BSScript::Variable& result) {
            const int value = result.IsInt() ? result.GetSInt() : -1;
            identityVersion.store(value == kIdentityBridgeVersion ? value : -1);
            identityVersionPending.store(false);
            if (value != kIdentityBridgeVersion)
                SKSE::log::error("SPS-013: identity bridge version {} found, {} required; install the paired DLL and scripts",
                    value, kIdentityBridgeVersion);
        }));
    if (!sent) identityVersionPending.store(false);
    return false;
}

void Describe(std::uint32_t formID, bool readSos, bool readSexLab, std::uint64_t session,
    std::int64_t allowedAfterMs, std::int64_t now)
{
    {
        std::scoped_lock guard(lock);
        auto& entry = entries[formID];
        entry.identity.formID = formID;
        if (entry.identity.pending) return;
        entry.identity.pending = true;
        entry.describedAt = now;
    }
    const bool sent = DispatchStaticWithCallback("SPS_IdentityBridge", "DescribeActor",
        allowedAfterMs, now,
        MakeCallback([formID, readSos, readSexLab, session](const RE::BSScript::Variable& result) {
            std::scoped_lock guard(lock);
            if (session != generation) return;
            auto& entry = entries[formID];
            entry.identity.pending = false;
            ActorIdentity parsed = entry.identity;
            if (result.IsString() && Parse(result.GetString(), parsed)) {
                parsed.known = true;
                parsed.sosRead = readSos;
                parsed.sexLabRead = readSexLab;
                parsed.pending = false;
                const auto& old = entry.identity;
                if (!old.known || old.addon != parsed.addon || old.baseSex != parsed.baseSex || old.sex != parsed.sex)
                    SKSE::log::info("Identity {:08X}: schlong '{}' size {}, SexLab base sex {}, current sex {}",
                        formID, parsed.addon, parsed.rank, parsed.baseSex, parsed.sex);
                entry.identity = std::move(parsed);
            }
        }),
        static_cast<std::int32_t>(formID), readSos, readSexLab);
    if (!sent) {
        std::scoped_lock guard(lock);
        entries[formID].identity.pending = false;
    }
}

void SendFuta(std::uint32_t formID, bool futa, bool reapply, std::uint64_t session,
    std::int64_t allowedAfterMs, std::int64_t now)
{
    const bool sent = DispatchStaticWithCallback("SPS_SexLabBridge", "SetActorFuta",
        allowedAfterMs, now,
        MakeCallback([formID, futa, reapply, session, allowedAfterMs](const RE::BSScript::Variable& result) {
            const int code = result.IsInt() ? result.GetSInt() : -1;
            const auto name = NameOf(formID);
            {
                std::scoped_lock guard(lock);
                if (session != generation) return;
                auto& entry = entries[formID];
                entry.describedAt = -1;  // re-read on the next tick
                if (code == 1) {
                    if (futa) remembered.insert(formID);
                    else remembered.erase(formID);
                    entry.identity.rememberedFuta = futa;
                }
            }
            if (code == 1) {
                SKSE::log::info("{} ({:08X}): SexLab futa {}", name, formID,
                    futa ? (reapply ? "re-applied after load or reset" : "set") : "cleared");
                if (!reapply)
                    Notify(futa ? fmt::format("SexLab now treats {} as futa", name)
                                : fmt::format("SexLab futa override cleared for {}", name), allowedAfterMs);
            } else {
                SKSE::log::warn("{} ({:08X}): SexLab futa {} refused ({})", name, formID,
                    futa ? "set" : "clear",
                    code == 0 ? "not a female in SexLab, or SexLab unavailable" : "actor not found");
                if (!reapply) Notify(fmt::format("SPS could not change SexLab futa for {}", name), allowedAfterMs);
            }
        }),
        static_cast<std::int32_t>(formID), futa);
    if (!sent)
        SKSE::log::warn("{} ({:08X}): SexLab futa request not sent; the script engine was busy", NameOf(formID), formID);
}

void OnSave(SKSE::SerializationInterface* intfc)
{
    std::vector<std::uint32_t> list;
    {
        std::scoped_lock guard(lock);
        list.assign(remembered.begin(), remembered.end());
    }
    if (intfc->OpenRecord(kFutaRecord, kFutaRecordVersion)) {
        const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(list.size(), kMaxRemembered));
        intfc->WriteRecordData(count);
        for (std::uint32_t i = 0; i < count; ++i) intfc->WriteRecordData(list[i]);
    }
    const auto managed = ReadManagedNpcMembership();
    if (!managed.empty() && intfc->OpenRecord(kManagedRecord, kManagedRecordVersion)) {
        const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(managed.size(), kManagedNpcMaxCapacity));
        intfc->WriteRecordData(count);
        for (std::uint32_t i = 0; i < count; ++i) intfc->WriteRecordData(managed[i]);
    }
}

void OnLoad(SKSE::SerializationInterface* intfc)
{
    std::set<std::uint32_t> loaded;
    std::vector<std::uint32_t> managed;
    std::uint32_t type = 0, version = 0, length = 0;
    while (intfc->GetNextRecordInfo(type, version, length)) {
        if (type == kManagedRecord && version == kManagedRecordVersion) {
            std::uint32_t count = 0;
            if (intfc->ReadRecordData(count) != sizeof(count)) break;
            count = std::min<std::uint32_t>(count, static_cast<std::uint32_t>(kManagedNpcMaxCapacity));
            for (std::uint32_t i = 0; i < count; ++i) {
                std::uint32_t saved = 0;
                if (intfc->ReadRecordData(saved) != sizeof(saved)) break;
                RE::FormID resolved = 0;
                // A removed plugin drops the actor; FF runtime IDs pass through unchanged.
                if (intfc->ResolveFormID(saved, resolved)) managed.push_back(resolved);
            }
            continue;
        }
        if (type != kFutaRecord || version != kFutaRecordVersion) continue;
        std::uint32_t count = 0;
        if (intfc->ReadRecordData(count) != sizeof(count)) break;
        count = std::min(count, kMaxRemembered);
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint32_t saved = 0;
            if (intfc->ReadRecordData(saved) != sizeof(saved)) break;
            RE::FormID resolved = 0;
            // Load-order changes remap the ID; an actor whose plugin was removed is dropped.
            if (intfc->ResolveFormID(saved, resolved)) loaded.insert(resolved);
        }
    }
    std::scoped_lock guard(lock);
    remembered = std::move(loaded);
    SKSE::log::info("SexLab futa list loaded from this save: {} actor(s)", remembered.size());
    RestoreManagedNpcMembership(managed);
    SKSE::log::info("Managed NPC list loaded from this save: {} actor(s)", managed.size());
}

void OnRevert(SKSE::SerializationInterface*)
{
    std::scoped_lock guard(lock);
    remembered.clear();
    entries.clear();
    futaRequests.clear();
    ++generation;
    ClearManagedNpcMembership();
}

// SPS_IdentityBridge / SPS_SexLabBridge.ResolveIdentityActor. Game.GetForm returns None for FormIDs
// >= 0x80000000, so light-plugin (FE) and runtime-created (FF) actors such as a
// spawned follower could never be described or made futa from Papyrus.
RE::Actor* NativeResolveIdentityActor(RE::StaticFunctionTag*, std::int32_t formID)
{
    return RE::TESForm::LookupByID<RE::Actor>(static_cast<RE::FormID>(static_cast<std::uint32_t>(formID)));
}

}  // namespace

bool RegisterActorIdentityNatives(RE::BSScript::IVirtualMachine* vm)
{
    if (!vm) return false;
    vm->RegisterFunction("ResolveIdentityActor", "SPS_SexLabBridge", NativeResolveIdentityActor);
    vm->RegisterFunction("ResolveIdentityActor", "SPS_IdentityBridge", NativeResolveIdentityActor);
    return true;
}

bool FutaEligible(const ActorIdentity& identity)
{
    return identity.known && identity.sexLabRead && (identity.baseSex == 0 || identity.baseSex == 1) &&
        (!identity.sosRead || !identity.addon.empty());
}

const char* SexLabSexName(int sex)
{
    switch (sex) {
    case 0: return "Male";
    case 1: return "Female";
    case 2: return "Futa";
    case 3: return "Creature (male)";
    case 4: return "Creature (female)";
    default: return "Unknown";
    }
}

void TickActorIdentities(const std::vector<std::uint32_t>& watched, std::int64_t allowedAfterMs)
{
    const bool readSos = SosAeNativeLoaded();
    const bool readSexLab = ReadSexLab();
    if ((!readSos && !readSexLab) || !IdentityBridgeInstalled()) return;
    const auto now = NowMs();
    if (!PapyrusReady(allowedAfterMs, now) || !EnsureIdentityBridge(allowedAfterMs, now)) return;
    // The SexLab bridge (futa writes, SexLab sex reads) is only checked, and
    // only called, when SexLab is loaded.
    if (readSexLab && !EnsureBridge(allowedAfterMs, now)) return;

    std::vector<std::uint32_t> targets = watched;
    std::vector<std::pair<std::uint32_t, bool>> requests;
    std::uint64_t session = 0;
    {
        std::scoped_lock guard(lock);
        session = generation;
        targets.insert(targets.end(), remembered.begin(), remembered.end());
        requests.swap(futaRequests);
    }
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

    if (readSexLab) {
        for (const auto& [formID, futa] : requests) SendFuta(formID, futa, false, session, allowedAfterMs, now);
    }

    for (const auto formID : targets) {
        const auto actor = RE::TESForm::LookupByID<RE::Actor>(formID);
        if (!actor) continue;
        bool describe = false, reapply = false;
        {
            std::scoped_lock guard(lock);
            auto& entry = entries[formID];
            entry.identity.formID = formID;
            entry.identity.rememberedFuta = remembered.contains(formID);
            describe = !entry.identity.pending &&
                (entry.describedAt < 0 || now - entry.describedAt >= kDescribeEveryMs);
            // SexLab's override lives on the reference and is lost when the
            // reference is rebuilt. Re-apply only for loaded actors in this
            // save's list whose SexLab sex no longer reads futa.
            if (readSexLab && entry.identity.rememberedFuta && entry.identity.known &&
                entry.identity.sexLabRead &&
                entry.identity.sex != 2 && actor->Is3DLoaded() &&
                (entry.reappliedAt < 0 || now - entry.reappliedAt >= kReapplyEveryMs)) {
                entry.reappliedAt = now;
                reapply = true;
            }
        }
        if (reapply) SendFuta(formID, true, true, session, allowedAfterMs, now);
        if (describe) Describe(formID, readSos, readSexLab, session, allowedAfterMs, now);
    }
}

bool RequestFuta(std::uint32_t formID, bool futa)
{
    if (formID == 0) return false;
    std::scoped_lock guard(lock);
    futaRequests.emplace_back(formID, futa);
    return true;
}

std::optional<ActorIdentity> ReadActorIdentity(std::uint32_t formID)
{
    std::scoped_lock guard(lock);
    const auto found = entries.find(formID);
    if (found == entries.end()) return std::nullopt;
    auto identity = found->second.identity;
    identity.rememberedFuta = remembered.contains(formID);
    return identity;
}

void ResetActorIdentities()
{
    std::scoped_lock guard(lock);
    entries.clear();
    futaRequests.clear();
    ++generation;
    bridgeVersion.store(0);
    versionPending.store(false);
    identityVersion.store(0);
    identityVersionPending.store(false);
}

void RegisterActorIdentitySerialization()
{
    const auto* serialization = SKSE::GetSerializationInterface();
    if (!serialization) return;
    serialization->SetUniqueID(kSerializationID);
    serialization->SetSaveCallback(OnSave);
    serialization->SetLoadCallback(OnLoad);
    serialization->SetRevertCallback(OnRevert);
}

}
