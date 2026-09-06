#pragma once

#include <RE/Skyrim.h>

#include <cstdint>
#include <utility>

namespace SPS::Runtime {

RE::BSScript::Internal::VirtualMachine* VM();
std::int64_t NowMs();
void UpdateExecutionClock();
void PauseExecutionClock();
bool ExecutionPaused();
bool JournalMenuOpen();
// Null means ready; a non-null result describes the actual dispatch blocker.
const char* PapyrusWaitReason(std::int64_t allowedAfterMs, std::int64_t nowMs);
bool PapyrusReady(std::int64_t allowedAfterMs, std::int64_t nowMs);
RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> MakeDiscardCallback();

template <class... Args>
bool DispatchStaticWithCallback(
    const char* script,
    const char* function,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs,
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback,
    Args... values)
{
    auto* vm = VM();
    if (!vm || !callback || !PapyrusReady(allowedAfterMs, nowMs)) {
        return false;
    }
    auto* args = RE::MakeFunctionArguments(std::move(values)...);
    return vm->DispatchStaticCall(script, function, args, callback);
}

template <class... Args>
bool DispatchStatic(
    const char* script,
    const char* function,
    std::int64_t allowedAfterMs,
    std::int64_t nowMs,
    Args... values)
{
    auto* vm = VM();
    if (!vm || !PapyrusReady(allowedAfterMs, nowMs)) {
        return false;
    }
    auto* args = RE::MakeFunctionArguments(std::move(values)...);
    auto callback = MakeDiscardCallback();
    return vm->DispatchStaticCall(script, function, args, callback);
}

}
