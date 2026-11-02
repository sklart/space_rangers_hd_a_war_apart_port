// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
namespace fHangar {struct TfHangar;}
namespace srhd_awa::matrixgame {
// Owns the real Hangar UI and TCCInterface required by original TGalaxy_Create.
// The host, pixel format and script runtime must outlive this owner. Destroy
// every Galaxy consumer before it. Campaign generation remains a separate step.
class RangersGalaxyPrerequisites {
public:
 RangersGalaxyPrerequisites();
 ~RangersGalaxyPrerequisites();
 RangersGalaxyPrerequisites(const RangersGalaxyPrerequisites&)=delete;
 RangersGalaxyPrerequisites& operator=(const RangersGalaxyPrerequisites&)=delete;
 fHangar::TfHangar& Hangar()const noexcept;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
