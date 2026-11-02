// SPDX-License-Identifier: GPL-2.0-or-later
// MSVC resolves references from the full config TU before discarding unused
// functions. These are original global storage slots for those unexecuted
// ReadParams/SetDefaults paths. No parser, heap or callback function is replaced.
#include "MatrixConfig.hpp"
Base::CHeap* g_MatrixHeap;
Base::CHeap* g_CacheHeap;
Base::CBlockPar* g_MatrixData;
int g_MaxFPS;
