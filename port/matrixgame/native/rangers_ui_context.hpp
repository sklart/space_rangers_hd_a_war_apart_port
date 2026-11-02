// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <string>
namespace EC_BlockPar {struct TBlockParEC;}
namespace srhd_awa::matrixgame {
struct RangersUiContextOptions {
 std::u16string settings_path=u"CFG.TXT";
 // Original newgame.txt is optional. Supply its real path to load one read-only.
 std::u16string new_game_settings_path;
 int width=1024,height=768,presentation_width=1024,presentation_height=768;
};
// UI infrastructure, not the Windows process-wide startup routine. Owns real
// read-only config/TCC/canvas/palette/cursors and borrows live host data/cache.
// Construct before Script/GlobalUi/Campaign; destroy after those owners/workers.
// Does not create or update settings, game resources or save files.
class RangersUiContext {
public:
 explicit RangersUiContext(const RangersUiContextOptions& options={});
 ~RangersUiContext();
 RangersUiContext(const RangersUiContext&)=delete;
 RangersUiContext& operator=(const RangersUiContext&)=delete;
 EC_BlockPar::TBlockParEC& Settings()const noexcept;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
