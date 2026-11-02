// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
namespace srhd_awa::matrixgame {
// Original achievement definitions/local service, scoped to the live data host.
// Initialization reads only the configured user directory; it never saves.
class RangersAchievementSession {
public:
 RangersAchievementSession();
 ~RangersAchievementSession();
 RangersAchievementSession(const RangersAchievementSession&)=delete;
 RangersAchievementSession& operator=(const RangersAchievementSession&)=delete;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
