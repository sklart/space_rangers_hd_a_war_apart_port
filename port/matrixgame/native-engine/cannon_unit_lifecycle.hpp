// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
struct SMatrixCannonUnit;
namespace srhd_awa::matrixgame::cannon_units {
// Reset actual units after their graph/shadow cleanup. Detach backlinks;
// smoke remains with the original map owner, including the DIP path.
void Reset(SMatrixCannonUnit*,std::size_t count);
}
