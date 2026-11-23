// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_integrated_session.hpp"
#include "native_loop_switch.hpp"
#include "native_rangers_interface.hpp"
#include "rangers_achievement_session.hpp"
#include "rangers_global_ui_session.hpp"
#include "rangers_host_callbacks.hpp"
#include "rangers_native_input.hpp"
#include "rangers_script_session.hpp"
#include "rangers_world_shutdown.hpp"
#include "runtime_support.hpp"
#include "units/MessageText.hpp"
#include <stdexcept>

namespace srhd_awa::matrixgame {
namespace {
// Original platform startup creates this before the screen loop and campaign.
// Do not borrow or replace a foreign publisher.
class QuestMessagesOwner {
public:
    QuestMessagesOwner() {
        if (MessageText::QuestMessages)
            throw std::logic_error("quest messages already owned");
        messages_ = pas::construct_call<MessageText::TQuestMessages>(MessageText::TQuestMessages_Create);
        if (!messages_) throw std::bad_alloc();
        MessageText::QuestMessages = messages_;
    }
    ~QuestMessagesOwner() {
        if (MessageText::QuestMessages != messages_) std::terminate();
        pas::free(messages_);
        MessageText::QuestMessages = nullptr;
    }
    QuestMessagesOwner(const QuestMessagesOwner&) = delete;
    QuestMessagesOwner& operator=(const QuestMessagesOwner&) = delete;
private:
    MessageText::TQuestMessages* messages_{};
};
}
struct RangersIntegratedSession::Impl {
    // Reverse destruction is intentional: screen/script consumers stop before
    // input/UI, Robot callbacks before the data/audio host, and all before the
    // externally owned window and device. Unique pointers also unwind correctly
    // if any intermediate constructor or Install throws.
    std::unique_ptr<RangersHostSession> host;
    std::unique_ptr<QuestMessagesOwner> quest_messages;
    std::unique_ptr<NativeRangersInterface> robot;
    std::unique_ptr<RangersUiContext> ui;
    // Local achievements read GetGameUserDirectory, which UiContext binds to
    // the selected user root. Destroy them before that binding is restored.
    std::unique_ptr<RangersAchievementSession> achievements;
    std::unique_ptr<RangersNativeInput> input;
    std::unique_ptr<RangersScriptSession> scripts;
    std::unique_ptr<RangersGlobalUiSession> global_ui;
    std::unique_ptr<native_loop::SwitchBackend> battle_backend;
    std::unique_ptr<native_loop::Scope> battle_loop;
    bool initial_screens_started{};

    Impl(platform3g::SwitchWindow& window, platform3g::DekoDevice& device,
         const NativeGameOptions& options, const RangersHostOptions& host_options,
         const RangersUiContextOptions& ui_options) {
        if (&options.window != &window || &options.device != &device)
            throw std::invalid_argument("integrated Rangers requires one shared window and device");
        if (options.draw_sink != static_cast<NativeDrawSink*>(&device))
            throw std::invalid_argument("integrated Rangers requires the shared native draw sink");
        if (!options.resource_root || !*options.resource_root || !options.output_root || !*options.output_root)
            throw std::invalid_argument("integrated Rangers requires explicit resource and output roots");
        if (options.services)
            throw std::invalid_argument("integrated Rangers owns the real callback binding");
        host = std::make_unique<RangersHostSession>(host_options, RangersDeferredCallbacksTag{});
        quest_messages = std::make_unique<QuestMessagesOwner>();
        robot = std::make_unique<NativeRangersInterface>(options);
        robot->Install(MakeRangersHostCallbacks());
        ui = std::make_unique<RangersUiContext>(ui_options);
        achievements = std::make_unique<RangersAchievementSession>();
        input = std::make_unique<RangersNativeInput>(MakeSwitchRangersInputBackend(window));
        scripts = std::make_unique<RangersScriptSession>();
        global_ui = std::make_unique<RangersGlobalUiSession>();
        battle_backend = std::make_unique<native_loop::SwitchBackend>(window, device);
        battle_loop = std::make_unique<native_loop::Scope>(*battle_backend);
    }
    ~Impl() {
        // An exception can leave a live campaign even though the screen stack
        // has unwound. Never destroy its UI and callback owners first. A
        // detached snapshot or failed worker cannot be discarded safely here.
        if (scripts) {
            try { ShutdownRangersWorld(*scripts); }
            catch (...) { std::terminate(); }
        }
    }
};

RangersIntegratedSession::RangersIntegratedSession(
    platform3g::SwitchWindow& window, platform3g::DekoDevice& device,
    const NativeGameOptions& options, const RangersHostOptions& host_options,
    const RangersUiContextOptions& ui_options)
    : impl_(std::make_unique<Impl>(window, device, options, host_options, ui_options)) {}
RangersIntegratedSession::~RangersIntegratedSession() = default;
RangersScreenExit RangersIntegratedSession::RunInitialScreens() {
    if (impl_->initial_screens_started) throw std::logic_error("initial screen loop already started");
    impl_->initial_screens_started = true;
    const auto result = RunInitialRangersScreens();
    if (result == RangersScreenExit::Finished) ShutdownRangersWorld(*impl_->scripts);
    return result;
}
void RangersIntegratedSession::StopRequestWorker() { impl_->scripts->StopRequestWorker(); }
bool RangersIntegratedSession::RestorePendingSnapshot() { return RestorePendingRangersSnapshot(); }
void RangersIntegratedSession::ShutdownWorld() { ShutdownRangersWorld(*impl_->scripts); }
}
