// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <string>
namespace EC_BlockPar {struct TBlockParEC;}
namespace EC_Data {struct TDataEC;}
namespace srhd_awa::matrixgame {
struct RangersDataOptions {
    std::u16string requested_language;
    std::u16string mod_selection_path=u"Mods\\ModCFG.txt";
    bool skip_mods=false;
};
// Owns original package/config globals. The caller selects the existing ec_file
// game/private-user roots first. No user settings, resources or saves are written.
// Audio, music, EngineCallbacks and native Init/Deinit must finish before this owner.
class RangersDataSession {
public:
    explicit RangersDataSession(const RangersDataOptions& options={});
    ~RangersDataSession();
    RangersDataSession(const RangersDataSession&)=delete;
    RangersDataSession& operator=(const RangersDataSession&)=delete;
    EC_BlockPar::TBlockParEC& MainConfig()const noexcept;
    EC_BlockPar::TBlockParEC& LanguageConfig()const noexcept;
    EC_Data::TDataEC& CacheData()const noexcept;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
