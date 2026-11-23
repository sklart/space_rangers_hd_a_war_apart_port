// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_world_shutdown.hpp"
#include "rangers_script_session.hpp"
#include "runtime_support.hpp"
#include "types/EC_Thread.hpp"
#include "types/ThreadCalc.hpp"
#include "types/aSaveLoad.hpp"
#include "types/fGameSettings.hpp"
#include "units/EC_Buf.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/WindowsSdk.hpp"
#include "units/aGalaxy.hpp"
#include "units/aRanger.hpp"
#include "units/aSaveLoad.hpp"
#include "units/aScript.hpp"
#include "win32_compat_sync.hpp"
#include <stdexcept>

namespace srhd_awa::matrixgame {
namespace {
void WaitForIdle(EC_Thread::TThreadEC* worker, const char* name) {
    if (!worker) return;
    for (;;) {
        const auto status = WindowsSdk::WaitForSingleObject(worker->IdleEvent, 1000);
        if (status == WindowsSdk::WAIT_OBJECT_0) return;
        if (status != WindowsSdk::WAIT_TIMEOUT ||
            !srhd_awa::platform::win32_compat::IsThreadUsable(worker->ThreadHandle))
            throw std::runtime_error(name);
    }
}
}
bool RestorePendingRangersSnapshot() {
    if (!GlobalsV::MemorySnapshotActive && !aSaveLoad::MemorySnapshotBuffer) return false;
    if (!GlobalsV::MemorySnapshotActive || !aSaveLoad::MemorySnapshotBuffer || aGalaxy::Galaxy)
        throw std::logic_error("cannot restore an incomplete or inconsistent campaign snapshot");
    aSaveLoad::RestoreGameFromMemorySnapshot();
    if (GlobalsV::MemorySnapshotActive || aSaveLoad::MemorySnapshotBuffer || !aGalaxy::Galaxy)
        throw std::runtime_error("original campaign snapshot restore did not finish");
    return true;
}
void ShutdownRangersWorld(RangersScriptSession& scripts) {
    // Do not discard the only detached copy of a campaign after a battle
    // failure. The original snapshot owner must restore it first.
    if (GlobalsV::MemorySnapshotActive || aSaveLoad::MemorySnapshotBuffer)
        throw std::logic_error("cannot shut down with an outstanding campaign snapshot");
    scripts.StopRequestWorker();
    // Match the original introduction cleanup: TThreadEC destruction waits
    // for an in-flight generation before its world can be released.
    pas::free(Globals::NewGameGenerationThread);
    Globals::NewGameGenerationThread = nullptr;
    WaitForIdle(Globals::TurnCalculationThread, "turn calculation worker failed during shutdown");
    WaitForIdle(aSaveLoad::SaveWriter, "save worker failed during shutdown");
    // CurrentScript is also the published context, not an execution flag.
    // A world-owned script is cleared by the original TScript destructor.
    // Preserve a foreign context instead of invalidating someone else's owner.
    if (aScript::CurrentScript &&
        (!aGalaxy::Galaxy || !aGalaxy::Galaxy->Scripts ||
         pas::list_indexof(aGalaxy::Galaxy->Scripts, aScript::CurrentScript) < 0))
        throw std::logic_error("cannot release a world with a foreign script context");
    pas::free(aGalaxy::Galaxy);
    aGalaxy::Galaxy = nullptr;
    pas::free(aRanger::PlayerOldQuests);
    aRanger::PlayerOldQuests = nullptr;
}
}
