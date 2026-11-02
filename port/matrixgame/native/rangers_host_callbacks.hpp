// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace Robot {struct TRobotCallbacks;}
namespace srhd_awa::matrixgame {
// Original Rangers callback functions without InitializeRobotRuntime's Windows
// DLL loader. The caller still owns/initializes every real host dependency.
Robot::TRobotCallbacks MakeRangersHostCallbacks();
}
