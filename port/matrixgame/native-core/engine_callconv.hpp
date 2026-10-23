// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#if defined(_MSC_VER) && defined(_M_IX86)
#define MATRIXGAME_NATIVE_CALLBACK __stdcall
#elif defined(__i386__) && defined(__GNUC__)
#define MATRIXGAME_NATIVE_CALLBACK __attribute__((stdcall))
#else
#define MATRIXGAME_NATIVE_CALLBACK
#endif
