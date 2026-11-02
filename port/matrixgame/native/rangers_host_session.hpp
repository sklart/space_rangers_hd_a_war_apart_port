// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rangers_data_session.hpp"
#include <memory>
struct SMGDRangersInterface;
namespace srhd_awa::matrixgame {
struct RangersHostOptions {
    RangersDataOptions data;
    bool sound=true;
    bool music=true;
};
struct RangersDeferredCallbacksTag {};
// Original standalone Rangers services. Select ec_file roots before construction.
// Keep this owner alive through NativeInitRuntime::Deinit; its table/context
// services original sound/text/map destruction as well as normal Init/frame calls.
class RangersHostSession {
public:
    explicit RangersHostSession(const RangersHostOptions& options={});
    // Real data/audio/music only; a native Robot interface initializes callbacks.
    RangersHostSession(const RangersHostOptions&,RangersDeferredCallbacksTag);
    ~RangersHostSession();
    RangersHostSession(const RangersHostSession&)=delete;
    RangersHostSession& operator=(const RangersHostSession&)=delete;
    const SMGDRangersInterface& Table()const noexcept;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
