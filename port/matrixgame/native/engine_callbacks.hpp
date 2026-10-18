// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "MatrixGameDll.hpp"
#include "rangers_bridge.hpp"
#include <memory>
namespace srhd_awa::matrixgame {
struct EngineCallbackContext;
// Bind while the single global MatrixGame instance is stopped; services must
// outlive callbacks. In-flight calls pin the context; the table has static lifetime.
class EngineCallbacks {
 public:
  explicit EngineCallbacks(const Robot::TRobotCallbacks&);
  ~EngineCallbacks();
  EngineCallbacks(const EngineCallbacks&)=delete;
  EngineCallbacks& operator=(const EngineCallbacks&)=delete;
  static SMGDRangersInterface* Table() noexcept;
 private:
  std::shared_ptr<EngineCallbackContext> context_;
};
SRobotsSettings ToEngineSettings(const DisplaySettings&);
BattleStatistics ReadEngineStatistics(const SRobotGameState&);
}
