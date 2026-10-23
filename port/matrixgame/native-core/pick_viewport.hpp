// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace srhd_awa::matrixgame {
struct PickViewport {int width,height;};
// Supplied by the owning application. No cached dimensions or GPU requirement.
PickViewport ReadPickViewport() noexcept;
}
