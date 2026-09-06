#include "PapyrusGateway.h"
#include "controllers/ExecutionClock.h"
#include <chrono>

namespace SPS::Runtime {

namespace {

Controllers::ExecutionClock executionClock;
std::int64_t RealNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

class DiscardPapyrusResultCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    void operator()(RE::BSScript::Variable) override {}
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
};

}

RE::BSScript::Internal::VirtualMachine* VM()
{
    return RE::BSScript::Internal::VirtualMachine::GetSingleton();
}

std::int64_t NowMs() { return executionClock.Now(RealNowMs()); }
bool ExecutionPaused() { return executionClock.Paused(); }
void PauseExecutionClock() { executionClock.SetPaused(RealNowMs(), true); }

void UpdateExecutionClock()
{
    const auto* main = RE::Main::GetSingleton();
    auto* ui = RE::UI::GetSingleton();
    const auto* vm = VM();
    const bool paused = !main || !main->GetRuntimeData().gameActive ||
        (ui && (ui->GameIsPaused() || ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) ||
            ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))) ||
        (vm && vm->IsCompletelyFrozen());
    executionClock.SetPaused(RealNowMs(), paused);
}

bool JournalMenuOpen()
{
    auto* ui = RE::UI::GetSingleton();
    return ui && ui->IsMenuOpen(RE::JournalMenu::MENU_NAME);
}

const char* PapyrusWaitReason(std::int64_t allowedAfterMs, std::int64_t nowMs)
{
    if (ExecutionPaused()) return "game execution paused";
    if (nowMs < allowedAfterMs) {
        return "post-load delay";
    }
    const auto* main = RE::Main::GetSingleton();
    if (!main || !main->GetRuntimeData().gameActive) {
        return "game inactive";
    }
    if (auto* ui = RE::UI::GetSingleton(); ui &&
        (ui->GameIsPaused() || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || JournalMenuOpen())) {
        return "loading or Journal menu open";
    }
    const auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !player->GetParentCell() || !player->Is3DLoaded()) {
        return "player 3D unavailable";
    }
    const auto* vm = VM();
    if (!vm || !vm->initialized || !vm->handlePolicy || !vm->objectBindPolicy) {
        return "script VM unavailable";
    }
    if (vm->IsCompletelyFrozen()) {
        return "script VM frozen";
    }
    // Overstressed is a workload signal, not a frozen/unavailable VM. Blocking
    // on it prevents even the existing bounded ownership and scene queries
    // from recovering while unrelated scripts keep the suspended-stack count
    // high. Let DispatchStaticCall accept or reject those requests normally.
    return nullptr;
}

bool PapyrusReady(std::int64_t allowedAfterMs, std::int64_t nowMs)
{
    return PapyrusWaitReason(allowedAfterMs, nowMs) == nullptr;
}

RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> MakeDiscardCallback()
{
    return RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>{
        new DiscardPapyrusResultCallback()
    };
}

}
