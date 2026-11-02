// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
namespace fGov {struct TfGov;}
namespace srhd_awa::matrixgame {
// Original government GI tree and layout. Borrows live Rangers data/cache and
// the caller's real UI pixel-format/renderer context through screen destruction.
// OnOpen still requires the real Galaxy/Player/planet session; not called here.
class RangersGovernmentSession {
public:
 RangersGovernmentSession();
 ~RangersGovernmentSession();
 RangersGovernmentSession(const RangersGovernmentSession&)=delete;
 RangersGovernmentSession& operator=(const RangersGovernmentSession&)=delete;
 fGov::TfGov& Screen()const noexcept;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
