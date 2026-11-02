// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
namespace srhd_awa::matrixgame {
// Full original global UI/gameplay initialization. Requires a live host/script
// session, real pixel/scratch buffers, user settings and ML/Data/ZPos roots.
// All Galaxy consumers must be destroyed before this owner.
class RangersGlobalUiSession {
public:
 RangersGlobalUiSession();
 ~RangersGlobalUiSession();
 RangersGlobalUiSession(const RangersGlobalUiSession&)=delete;
 RangersGlobalUiSession& operator=(const RangersGlobalUiSession&)=delete;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
