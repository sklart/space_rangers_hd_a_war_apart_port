// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace srhd_awa::matrixgame {
class RangersScriptSession;
// Explicitly restore a complete detached snapshot through the original game
// mechanism. A partial/inconsistent snapshot is retained and rejected.
// Returns false when no snapshot exists.
bool RestorePendingRangersSnapshot();
// Terminal shutdown after the screen loop and all request producers have
// stopped. Keeps UI/host owners alive while the original workers finish.
// A live memory snapshot must first be restored by its original owner.
void ShutdownRangersWorld(RangersScriptSession& scripts);
}
