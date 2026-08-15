#include "SPSAPI.h"

#include <SKSE/SKSE.h>

#include <spdlog/sinks/basic_file_sink.h>

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
namespace logger = SKSE::log;

namespace {
const SPS::API::InterfaceV1* api{ nullptr };
SPS::API::ListenerHandle listener{ 0 };
std::atomic<unsigned> callbackCount{ 0 };
std::jthread smokeTestThread;

const char* StateName(SPS::API::PhysicsState state)
{
    switch (state) {
    case SPS::API::PhysicsState::SMP:
        return "SMP";
    case SPS::API::PhysicsState::CBPC:
        return "CBPC";
    default:
        return "unknown";
    }
}

void __cdecl OnStateChanged(const SPS::API::StateChangedEvent* event, void*)
{
    if (!event) {
        return;
    }

    ++callbackCount;
    logger::info("Callback: {} -> {}, source {}, request {}",
        StateName(event->previousState), StateName(event->currentState),
        static_cast<std::uint32_t>(event->source), event->activeRequest);
}

bool Connect()
{
    const auto module = ::GetModuleHandleW(SPS::API::kPluginDLL);
    if (!module) {
        logger::error("SPS DLL was not loaded");
        return false;
    }

    const auto getAPI = reinterpret_cast<SPS::API::GetAPIFn>(
        ::GetProcAddress(module, SPS::API::kGetAPIFunctionNameV1));
    if (!getAPI) {
        logger::error("SPS V1 API export was not found");
        return false;
    }

    api = getAPI();
    if (!api || api->version < SPS::API::kVersion ||
        api->size < sizeof(SPS::API::InterfaceV1)) {
        logger::error("SPS returned an incompatible V1 interface");
        api = nullptr;
        return false;
    }

    listener = api->RegisterStateListener(OnStateChanged, nullptr);
    logger::info("Connected to SPS API V{}; capabilities 0x{:X}; listener {}",
        api->version, api->GetCapabilities(), listener);
    return listener != 0;
}

void RunSmokeTest(std::stop_token stop)
{
    auto Wait = [&stop](std::chrono::milliseconds duration) {
        constexpr auto slice = 100ms;
        while (duration > 0ms && !stop.stop_requested()) {
            const auto step = (std::min)(duration, slice);
            std::this_thread::sleep_for(step);
            duration -= step;
        }
        return !stop.stop_requested();
    };

    if (!Wait(3s) || !api) {
        return;
    }

    SPS::API::StateSnapshot before{};
    if (api->GetState(SPS::API::kPlayerFormID, &before) != SPS::API::Result::Success) {
        logger::warn("Smoke test skipped: SPS does not have a player state yet");
        return;
    }

    const auto requested = before.state == SPS::API::PhysicsState::SMP ?
        SPS::API::PhysicsState::CBPC : SPS::API::PhysicsState::SMP;
    SPS::API::PhysicsRequest request{};
    request.state = requested;
    request.durationMilliseconds = 2500;
    request.requesterName = "SPS API smoke test";

    const auto handle = api->RequestPhysics(&request);
    if (!handle) {
        logger::error("FAIL: SPS rejected the temporary physics request");
        return;
    }
    logger::info("Requested {} for 2.5 seconds using handle {}", StateName(requested), handle);

    if (!Wait(1500ms)) {
        api->ReleasePhysics(handle);
        return;
    }

    SPS::API::StateSnapshot during{};
    const bool tookControl = api->GetState(SPS::API::kPlayerFormID, &during) == SPS::API::Result::Success &&
        during.state == requested && during.source == SPS::API::ControlSource::ExternalAPI;

    if (!Wait(2500ms)) {
        api->ReleasePhysics(handle);
        return;
    }

    SPS::API::StateSnapshot after{};
    const bool expired = api->GetState(SPS::API::kPlayerFormID, &after) == SPS::API::Result::Success &&
        after.activeRequest == 0;
    const bool resetNoticeAccepted = api->NotifyPhysicsReset(SPS::API::kPlayerFormID) == SPS::API::Result::Success;

    if (tookControl && expired && resetNoticeAccepted && callbackCount.load() > 0) {
        logger::info("PASS: request, callback, expiry and reset notification all worked");
    } else {
        logger::warn("NEEDS REVIEW: control={}, expiry={}, reset notice={}, callbacks={}",
            tookControl, expired, resetNoticeAccepted, callbackCount.load());
    }
}

void StartSmokeTest()
{
    smokeTestThread.request_stop();
    if (smokeTestThread.joinable()) {
        smokeTestThread.join();
    }
    callbackCount.store(0);
    smokeTestThread = std::jthread(RunSmokeTest);
}

void OnMessage(SKSE::MessagingInterface::Message* message)
{
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Connect();
    } else if ((message->type == SKSE::MessagingInterface::kPostLoadGame ||
                    message->type == SKSE::MessagingInterface::kNewGame) &&
               api) {
        StartSmokeTest();
    }
}
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    if (const auto dir = SKSE::log::log_directory()) {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            (*dir / "SPSAPITestPlugin.log").string(), true);
        spdlog::set_default_logger(std::make_shared<spdlog::logger>("global", std::move(sink)));
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
    }

    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    logger::info("SPS API smoke-test plugin loaded");
    return true;
}
