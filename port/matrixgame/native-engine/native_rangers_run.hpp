// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "original_game.hpp"
#include "runtime_support.hpp"
#include "types/Robot.hpp"
#include <memory>
namespace srhd_awa::matrixgame {
// Explicit PC reference entry; never treats D3D9 providers as a deko backend.
struct RangersLegacyReferenceTag {};
// Single-thread, scoped implementation of the Run slot consumed by Robot::FRun.
// Owns copied roots/settings/table; provider objects and callback host are borrowed
// and must outlive this binding and all calls. The caller installs the real
// native_loop::Scope before invoking Entry. No partial RobotInterface table is
// installed: Initialize/Finalize/Support remain the caller's separate integration.
class NativeRangersRunBinding {
public:
 explicit NativeRangersRunBinding(const NativeGameOptions&);
 NativeRangersRunBinding(const OriginalGameOptions&,RangersLegacyReferenceTag);
 ~NativeRangersRunBinding();
 NativeRangersRunBinding(const NativeRangersRunBinding&)=delete;
 NativeRangersRunBinding& operator=(const NativeRangersRunBinding&)=delete;
 Robot::TRobotRun Entry()const noexcept;
private:
 struct State;std::unique_ptr<State> state_;
 static thread_local State* active_;
 static std::int32_t PAS_STDCALL Run(std::uint32_t,std::uint32_t,char16_t*,Robot::PRobotDisplaySettings,
   char16_t*,char16_t*,char16_t*,char16_t*,char16_t*,aGalaxyStruct::PPlanetBattleStatistics);
};
}
