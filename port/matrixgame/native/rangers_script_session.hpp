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
 // Join the original request worker while its screens and campaign still
 // exist. Call only after producers have stopped scheduling new requests.
 // Idempotent; the script VM and turn worker remain owned by this session.
 void StopRequestWorker();
 RangersScriptSession(const RangersScriptSession&)=delete;
 RangersScriptSession& operator=(const RangersScriptSession&)=delete;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
