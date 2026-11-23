// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace srhd_awa::matrixgame {
enum class RangersScreenExit { Finished, RuntimeReloadRequested };
// First startup only. Uses the original loading screen and screen dispatcher;
// a return requesting runtime reload leaves the current campaign intact.
RangersScreenExit RunInitialRangersScreens();
}
