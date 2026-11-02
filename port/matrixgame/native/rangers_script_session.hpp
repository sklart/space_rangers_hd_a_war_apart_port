// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
namespace srhd_awa::matrixgame {
// Owns the original script VM/functions/variables, events/calculation worker,
// film history and path-node pool. The real host must outlive this owner;
// Galaxy and every path/script consumer must be destroyed before it.
class RangersScriptSession {
public:
 RangersScriptSession();
 ~RangersScriptSession();
 RangersScriptSession(const RangersScriptSession&)=delete;
 RangersScriptSession& operator=(const RangersScriptSession&)=delete;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
