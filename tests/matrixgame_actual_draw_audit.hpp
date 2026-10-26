// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
namespace srhd_awa::matrixgame::platform3g {class Device;}
namespace matrixgame_draw_audit {
int FinalResult(int hal_result)noexcept;
class Session {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 explicit Session(srhd_awa::matrixgame::platform3g::Device&);
 ~Session();
 void Finish();
};
}
