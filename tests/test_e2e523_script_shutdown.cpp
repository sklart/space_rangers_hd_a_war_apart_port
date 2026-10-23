#include "units/aScript.hpp"
#include "units/EC_Thread.hpp"
#include "types/aScript.hpp"
#include "win32_handles.hpp"
#include <array>
#include <cassert>
#include <cstdio>

// Link the real effective game objects. No thread/event or script methods are
// replaced. Empty request queues let the original Execute complete naturally.
int main() {
  namespace api = srhd_awa::platform::win32_compat;
  aScript::QueuedArcadeBattles = pas::make_object<pas::List>();
  aScript::QueuedTextQuests = pas::make_object<pas::List>();
  aScript::QueuedPlanetaryBattles = pas::make_object<pas::List>();
  aScript::QueuedVideos = pas::make_object<pas::List>();
  for (int cycle = 0; cycle < 3; ++cycle) {
    aScript::ScriptRequestThread = pas::construct_call<aScript::TScriptThread>(aScript::TScriptThread_Create);
    const auto* worker = aScript::ScriptRequestThread;
    const auto thread = worker->ThreadHandle;
    const std::array events{worker->StopEvent, worker->ShutdownEvent,
                            worker->StartEvent, worker->RunningEvent, worker->IdleEvent};
    assert(api::Handles().TryLookup(thread, api::HandleType::Thread));
    if (cycle) aScript::StartScriptRequestThread();
    aScript::FinalizeScriptEngine();
    assert(aScript::ScriptRequestThread == nullptr);
    assert(!api::Handles().TryLookup(thread, api::HandleType::Thread));
    for (auto event : events)
      assert(!api::Handles().TryLookup(event, api::HandleType::Event));
    aScript::FinalizeScriptEngine(); // Repeated finalization is harmless.
  }
  pas::free(aScript::QueuedArcadeBattles); aScript::QueuedArcadeBattles = nullptr;
  pas::free(aScript::QueuedTextQuests); aScript::QueuedTextQuests = nullptr;
  pas::free(aScript::QueuedPlanetaryBattles); aScript::QueuedPlanetaryBattles = nullptr;
  pas::free(aScript::QueuedVideos); aScript::QueuedVideos = nullptr;
  std::puts("original ScriptThread create/start/execute/finalize/reentry: PASS");
}
