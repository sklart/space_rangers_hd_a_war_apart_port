// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
namespace EC_BlockPar {struct TBlockParEC;}
namespace srhd_awa::matrixgame {
class RangersDataSession;
// Borrow real main/language configs and a live RangersAudioSession. Destroy this owner
// before audio/cache/config; original music destruction joins every worker.
class RangersMusicSession {
public:
    RangersMusicSession(EC_BlockPar::TBlockParEC& main_config,EC_BlockPar::TBlockParEC& language_config);
    explicit RangersMusicSession(RangersDataSession&);
    ~RangersMusicSession();
    RangersMusicSession(const RangersMusicSession&)=delete;
    RangersMusicSession& operator=(const RangersMusicSession&)=delete;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
