// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <memory>
#include <string>
namespace aGalaxy{struct TGalaxy;}
namespace srhd_awa::matrixgame {
struct RangersCampaignOptions {
 std::u16string player_name=u"PortDiagnostic";
 unsigned char race=2;
 std::array<unsigned char,8> difficulty{2,2,2,2,2,2,2,2};
 int portrait=0,character=0;
 std::array<unsigned char,2> items{43,44},skills{0,1};
 bool iron_will=false;
};
// Owns a newly generated original campaign. Requires the full global UI, data,
// script and TCCInterface owners. Uses the real generation worker and warmup;
// does not load or save files. Generation can take substantial time.
class RangersCampaignSession {
public:
 explicit RangersCampaignSession(const RangersCampaignOptions& options={});
 ~RangersCampaignSession();
 RangersCampaignSession(const RangersCampaignSession&)=delete;
 RangersCampaignSession& operator=(const RangersCampaignSession&)=delete;
 // Original battle snapshots replace the world. Reacquire references after
 // restore; access while the world is detached throws std::logic_error.
 aGalaxy::TGalaxy& Galaxy()const;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
