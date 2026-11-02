// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
namespace EC_Data {struct TDataEC;}
namespace srhd_awa::matrixgame {
class RangersDataSession;
// Audio-only Rangers host dependency owner. Borrows the real, already loaded
// cache resource tree. Outlive every bridge/controller using these services.
class RangersAudioSession {
public:
    explicit RangersAudioSession(EC_Data::TDataEC&);
    explicit RangersAudioSession(RangersDataSession&);
    ~RangersAudioSession();
    RangersAudioSession(const RangersAudioSession&)=delete;
    RangersAudioSession& operator=(const RangersAudioSession&)=delete;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
