// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_rangers_run.hpp"
namespace srhd_awa::matrixgame {
// Complete scoped native Robot interface. Callback host and providers remain
// borrowed. Install initializes real callback adapters and publishes only a
// supported, ready table; Finalize may be called by original Rangers code.
// On Switch, NativeGameOptions owns a scoped RGB565 canvas presenter for the
// real DekoDevice. Legacy reference options retain their existing presentation.
class NativeRangersInterface {
public:
 explicit NativeRangersInterface(const NativeGameOptions&);
 NativeRangersInterface(const OriginalGameOptions&,RangersLegacyReferenceTag);
 ~NativeRangersInterface();
 NativeRangersInterface(const NativeRangersInterface&)=delete;
 NativeRangersInterface& operator=(const NativeRangersInterface&)=delete;
 const Robot::TRobotInterfacePrefix& Table()const noexcept;
 void Install(const Robot::TRobotCallbacks&);
private:
 struct State;std::unique_ptr<State> state_;
 static thread_local State* active_;
 static State& Current();
 static void PAS_STDCALL Initialize(Robot::PRobotCallbacks);
 static void PAS_STDCALL Finalize();
 static std::int32_t PAS_STDCALL Support();
 static std::int32_t PAS_STDCALL Run(std::uint32_t,std::uint32_t,char16_t*,Robot::PRobotDisplaySettings,
   char16_t*,char16_t*,char16_t*,char16_t*,char16_t*,aGalaxyStruct::PPlanetBattleStatistics);
};
}
