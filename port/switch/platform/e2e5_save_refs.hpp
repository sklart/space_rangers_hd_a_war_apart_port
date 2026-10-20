#pragma once

#include <cstdint>

namespace aGalaxy { struct TGalaxy; }
namespace aScript { struct TScript; }
namespace EC_Expression { struct TVarEC; }
namespace EC_Buf { struct TBufEC; }

namespace srhd_awa::platform::e2e5_save_refs {

// The v168 RSG64 format uses the original scalar payload width and reserved
// variable-kind bytes to carry the object category. Old loaders reject RSG64.
inline constexpr std::uint8_t kStar = 11;
inline constexpr std::uint8_t kPlanet = 12;
inline constexpr std::uint8_t kShip = 13;
inline constexpr std::uint8_t kConstellation = 14;
inline constexpr std::uint8_t kItem = 15;
inline constexpr bool IsObjectKind(std::uint8_t kind) {
    return kind >= kStar && kind <= kItem;
}

struct SaveScope {
    explicit SaveScope(aGalaxy::TGalaxy* galaxy);
    ~SaveScope();
    SaveScope(const SaveScope&) = delete;
    SaveScope& operator=(const SaveScope&) = delete;
private:
    aGalaxy::TGalaxy* previous_{};
    friend struct ScriptScope;
};

struct ScriptScope {
    explicit ScriptScope(aScript::TScript* script);
    ~ScriptScope();
    ScriptScope(const ScriptScope&) = delete;
    ScriptScope& operator=(const ScriptScope&) = delete;
private:
    aScript::TScript* previous_{};
};

void WriteScriptDword(EC_Buf::TBufEC* buffer, aScript::TScript* script,
                      EC_Expression::TVarEC* cell);
std::uint8_t NestedDiskKind(EC_Expression::TVarEC* cell);
std::uint32_t NestedDiskValue(EC_Expression::TVarEC* cell);
void SetLoadFormat(bool v168);
bool LoadFormatIsV168();
void QueueLoadedPointer(EC_Expression::TVarEC* cell, std::uint8_t disk_kind,
                        std::uint32_t id);
void RebindPending(aGalaxy::TGalaxy* galaxy);

} // namespace srhd_awa::platform::e2e5_save_refs
