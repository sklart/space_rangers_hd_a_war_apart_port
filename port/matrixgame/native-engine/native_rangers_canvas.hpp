// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
namespace srhd_awa::matrixgame {
namespace platform3g {class DekoDevice;}
// Owns a managed RGB565 texture and a scoped renderer_platform registration.
// The real DekoDevice must outlive this object and every synchronous UI present.
class NativeRangersCanvas {
public:
 explicit NativeRangersCanvas(platform3g::DekoDevice&);
 ~NativeRangersCanvas();
 NativeRangersCanvas(const NativeRangersCanvas&)=delete;
 NativeRangersCanvas& operator=(const NativeRangersCanvas&)=delete;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
