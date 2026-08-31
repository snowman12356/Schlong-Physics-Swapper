#include "PapyrusGateway.h"

namespace SPS::Runtime {

namespace {

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

bool PapyrusReady(std::int64_t allowedAfterMs, std::int64_t nowMs)
{
    if (nowMs < allowedAfterMs) {
        return false;
    }
    const auto* main = RE::Main::GetSingleton();
    if (!main || !main->GetRuntimeData().gameActive) {
        return false;
    }
    if (auto* ui = RE::UI::GetSingleton();
        ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
        return false;
    }
    const auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || !player->GetParentCell() || !player->Is3DLoaded()) {
        return false;
    }
    const auto* vm = VM();
    return vm && vm->initialized && !vm->overstressed && vm->handlePolicy &&
        vm->objectBindPolicy && !vm->IsCompletelyFrozen();
}

RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> MakeDiscardCallback()
{
    return RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>{
        new DiscardPapyrusResultCallback()
    };
}

}
