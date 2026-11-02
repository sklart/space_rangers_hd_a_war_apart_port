// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_diagnostic_journal.hpp"
namespace srhd_awa::matrixgame::platform3g {class DekoDevice;}
namespace srhd_awa::matrixgame::diagnostic {
// Uses the same actual DekoDevice/resources/materials as the original engine.
void RunSmoke(platform3g::DekoDevice&,Journal&);
void RenderStatus(platform3g::DekoDevice&,std::span<const wchar_t* const> lines);
void SaveFrame(Journal&,std::string_view name,unsigned width,unsigned height,std::span<const std::byte> bgra);
}
