#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "e2e4_script_value.hpp"
#include "e2e5_save_refs.hpp"

// Diagnostic preflight for the disposable original-game source overlay. It
// observes object identity before any 32-bit script value is written to disk.
namespace srhd_awa::platform::e2e5_save_inventory {

struct Classification {
    const char* category = "UNKNOWN";
    const char* object_type = "unresolved";
    std::uint32_t object_id = 0;
};

inline std::string ascii_name(const pas::WideString& value) {
    std::string text;
    for (std::int32_t i = 0; i < value.length(); ++i) {
        const auto c = value.c_str()[i];
        text.push_back(c >= 32 && c < 127 && c != '|' ? static_cast<char>(c) : '?');
    }
    return text;
}

inline Classification classify(aGalaxy::TGalaxy* galaxy, aScript::TScript* script,
                               const EC_Expression::TVarEC* cell) {
    const bool signed_value = cell->Kind == EC_Expression::vkInt;
    const auto value = signed_value ? static_cast<std::uintptr_t>(cell->IntValue)
                              : cell->DwordValue;
    if (value == 0) return {"NUMERIC", "zero", 0};
    if (script != nullptr) {
        for (std::int32_t si = 0; si < pas::list_count(script->Stars); ++si) {
            auto* binding = pas::list_at<aScript::TScriptStar>(script->Stars, si);
            if (binding->Star != nullptr && binding->Name == cell->Name &&
                value == reinterpret_cast<std::uintptr_t>(binding->Star))
                return {"REBIND_FROM_EXISTING_METADATA", "script_star", binding->Star->Id};
            if (binding->Planets == nullptr) continue;
            for (std::int32_t pi = 0; pi < binding->Planets.length(); ++pi) {
                const auto& planet = binding->Planets[pi];
                if (planet.Planet != nullptr && planet.Name == cell->Name &&
                    value == reinterpret_cast<std::uintptr_t>(planet.Planet))
                    return {"REBIND_FROM_EXISTING_METADATA", "script_planet", planet.Planet->Id};
            }
        }
    }
    for (std::int32_t ci = 0; ci < pas::list_count(galaxy->Constellations); ++ci) {
        auto* constellation = pas::list_at<aGalaxy::TConstellation>(galaxy->Constellations, ci);
        if (value == reinterpret_cast<std::uintptr_t>(constellation))
            return {"STABLE_OBJECT_ID", "constellation", constellation->Id};
    }
    for (std::int32_t si = 0; si < pas::list_count(galaxy->Stars); ++si) {
        auto* star = pas::list_at<aGalaxy::TStar>(galaxy->Stars, si);
        if (value == reinterpret_cast<std::uintptr_t>(star))
            return {"STABLE_OBJECT_ID", "star", star->Id};
        for (std::int32_t pi = 0; pi < pas::list_count(star->Planets); ++pi) {
            auto* planet = pas::list_at<aPlanet::TPlanet>(star->Planets, pi);
            if (value == reinterpret_cast<std::uintptr_t>(planet))
                return {"STABLE_OBJECT_ID", "planet", planet->Id};
        }
    }
    for (std::int32_t pi = 0; pi < pas::list_count(galaxy->Planets); ++pi) {
        auto* planet = pas::list_at<aPlanet::TPlanet>(galaxy->Planets, pi);
        if (value == reinterpret_cast<std::uintptr_t>(planet))
            return {"STABLE_OBJECT_ID", "planet", planet->Id};
    }
    if (script != nullptr) {
        if (script->CurrentShip != nullptr &&
            value == reinterpret_cast<std::uintptr_t>(script->CurrentShip))
            return {"STABLE_OBJECT_ID", "ship", script->CurrentShip->Id};
        for (std::int32_t i = 0; i < pas::list_count(script->Ships); ++i) {
            auto* binding = pas::list_at<aScript::TScriptShip>(script->Ships, i);
            if (binding->Ship != nullptr &&
                value == reinterpret_cast<std::uintptr_t>(binding->Ship))
                return {"STABLE_OBJECT_ID", "ship", binding->Ship->Id};
        }
        for (std::int32_t i = 0; i < pas::list_count(script->Places); ++i) {
            auto* binding = pas::list_at<aScript::TScriptPlace>(script->Places, i);
            if (value == reinterpret_cast<std::uintptr_t>(binding) &&
                binding->Name == cell->Name)
                return {"REBIND_FROM_EXISTING_METADATA", "script_place", 0};
        }
        for (std::int32_t i = 0; i < pas::list_count(script->Items); ++i) {
            auto* binding = pas::list_at<aScript::TScriptItem>(script->Items, i);
            if (value == reinterpret_cast<std::uintptr_t>(binding) &&
                binding->Name == cell->Name)
                return {"REBIND_FROM_EXISTING_METADATA", "script_item", 0};
        }
    }
    if (signed_value ? srhd_awa::platform::e2e4_script_value::FitsDiskInt(cell->IntValue)
                     : srhd_awa::platform::e2e4_script_value::FitsDiskDword(value))
        return {"NUMERIC", "scalar", 0};
    return {};
}

inline bool inspect_scope(aGalaxy::TGalaxy* galaxy, aScript::TScript* script,
                          EC_Expression::TVarArrayEC* variables,
                          const char* scope, const std::string& script_name,
                          std::int32_t& inspected, std::int32_t& unknown,
                          const std::string& prefix = {}, std::int32_t depth = 0) {
    if (variables == nullptr) return true;
    if (depth > 32) {
        ++inspected;
        ++unknown;
        std::fprintf(stderr, "E2E5_SAVE_INV|%s|%s|%s|vkArray|UNKNOWN|nesting_limit|0\n",
                     script_name.c_str(), scope, prefix.c_str());
        return false;
    }
    for (std::int32_t i = 0; i < variables->Count; ++i) {
        auto* cell = EC_Expression::TVarArrayEC_GetItem(variables, i);
        const auto name = prefix + ascii_name(cell->Name);
        if (cell->Kind == EC_Expression::vkArray) {
            if (cell->GetArray() == nullptr) {
                ++inspected;
                ++unknown;
                std::fprintf(stderr, "E2E5_SAVE_INV|%s|%s|%s|vkArray|UNKNOWN|null_array|0\n",
                             script_name.c_str(), scope, name.c_str());
            } else {
                inspect_scope(galaxy, script, cell->GetArray(), scope, script_name,
                              inspected, unknown, name + ".", depth + 1);
            }
            continue;
        }
        if (cell->Kind == EC_Expression::vkRef) {
            ++inspected;
            std::fprintf(stderr, "E2E5_SAVE_INV|%s|%s|%s|vkRef|TRANSIENT|definition_link|0\n",
                         script_name.c_str(), scope, name.c_str());
            continue;
        }
        if (cell->Kind != EC_Expression::vkInt &&
            cell->Kind != EC_Expression::vkDword) continue;
        const auto classification = classify(galaxy, prefix.empty() ? script : nullptr, cell);
        ++inspected;
        if (classification.category[0] == 'U') {
            ++unknown;
            char detail[256]{};
            std::snprintf(detail, sizeof(detail),
                         "E2E5 save unknown script=%.64s scope=%.16s variable=%.80s kind=%s value=%llx",
                         script_name.c_str(), scope, name.c_str(),
                         cell->Kind == EC_Expression::vkInt ? "int" : "dword",
                         static_cast<unsigned long long>(cell->Kind == EC_Expression::vkInt
                             ? static_cast<std::uintptr_t>(cell->IntValue) : cell->DwordValue));
            GR_Main::AppendLogLineThreadSafe(pas::AnsiString(detail));
        }
        std::fprintf(stderr, "E2E5_SAVE_INV|%s|%s|%s|%s|%s|%s|%u\n",
                     script_name.c_str(), scope, name.c_str(),
                     cell->Kind == EC_Expression::vkInt ? "vkInt" : "vkDword",
                     classification.category, classification.object_type,
                     classification.object_id);
    }
    return unknown == 0;
}

inline bool preflight(aGalaxy::TGalaxy* galaxy) {
    std::int32_t inspected = 0;
    std::int32_t unknown = 0;
    inspect_scope(galaxy, nullptr, Globals::SharedScriptVariables, "global", "global",
                  inspected, unknown);
    for (std::int32_t i = 0; i < pas::list_count(galaxy->Scripts); ++i) {
        auto* script = pas::list_at<aScript::TScript>(galaxy->Scripts, i);
        const auto name = ascii_name(script->ScriptFileName);
        inspect_scope(galaxy, script, script->InitCode->LocalVar, "init", name,
                      inspected, unknown);
        inspect_scope(galaxy, script, script->TurnCode->LocalVar, "turn", name,
                      inspected, unknown);
    }
    std::fprintf(stderr, "E2E5_SAVE_INV_SUMMARY|%d|%d\n", inspected, unknown);
    std::fflush(stderr);
    return unknown == 0;
}

inline void log_game_state(aGalaxy::TGalaxy* galaxy, const char* phase) {
    auto* player = aPlayer::GetPlayer();
    if (player == nullptr) {
        std::fprintf(stderr, "E2E5_STATE|%s|missing_player\n", phase);
        return;
    }
    std::fprintf(stderr, "E2E5_STATE|%s|%u|%d|%u|%u|%u|%d\n",
                 phase, static_cast<unsigned>(galaxy->CurrentTurn), player->Money,
                 player->Id, player->CurrentStar ? player->CurrentStar->Id : 0u,
                 player->CurrentPlanet ? player->CurrentPlanet->Id : 0u,
                 pas::list_count(galaxy->Scripts));
    std::fflush(stderr);
}

} // namespace srhd_awa::platform::e2e5_save_inventory

namespace srhd_awa::platform::e2e5_save_refs {
namespace {
thread_local aGalaxy::TGalaxy* current_save_galaxy = nullptr;
thread_local aScript::TScript* current_save_script = nullptr;
thread_local bool current_load_v168 = false;
struct PendingPointer {
    EC_Expression::TVarEC* cell;
    std::uint8_t kind;
    std::uint32_t id;
};
thread_local std::vector<PendingPointer> pending_pointers;

[[noreturn]] void bad_reference(const char* message) {
    pas::raise(pas::make_exception<pas::Exception>(pas::AnsiString(message)));
}

std::uint8_t object_kind(const e2e5_save_inventory::Classification& binding) {
    const std::string type(binding.object_type);
    if (type == "star") return kStar;
    if (type == "planet") return kPlanet;
    if (type == "ship") return kShip;
    if (type == "constellation") return kConstellation;
    bad_reference("E2E5 script object type has no stable disk representation");
}

std::uint32_t disk_value(const e2e5_save_inventory::Classification& binding,
                         EC_Expression::TVarEC* cell) {
    const std::string category(binding.category);
    if (category == "REBIND_FROM_EXISTING_METADATA") return 0;
    if (category == "STABLE_OBJECT_ID") {
        if (binding.object_id == 0)
            bad_reference("E2E5 non-null script object has no stable ID");
        return binding.object_id;
    }
    if (category == "NUMERIC" &&
        e2e4_script_value::FitsDiskDword(cell->GetDword()))
        return static_cast<std::uint32_t>(cell->GetDword());
    bad_reference("E2E5 script value cannot be serialized safely");
}
} // namespace

SaveScope::SaveScope(aGalaxy::TGalaxy* galaxy) : previous_(current_save_galaxy) {
    current_save_galaxy = galaxy;
}
SaveScope::~SaveScope() { current_save_galaxy = previous_; }
ScriptScope::ScriptScope(aScript::TScript* script) : previous_(current_save_script) {
    current_save_script = script;
}
ScriptScope::~ScriptScope() { current_save_script = previous_; }

void WriteScriptDword(EC_Buf::TBufEC* buffer, aScript::TScript* script,
                      EC_Expression::TVarEC* cell) {
    if (current_save_galaxy == nullptr)
        bad_reference("E2E5 script save context missing");
    const auto binding = e2e5_save_inventory::classify(current_save_galaxy, script, cell);
    std::uint8_t kind = static_cast<std::uint8_t>(EC_Expression::vkDword);
    if (std::string(binding.category) == "STABLE_OBJECT_ID") kind = object_kind(binding);
    buffer->AddWideStringZ(cell->Name);
    buffer->AddAnsiChar(kind);
    buffer->AddDWord(disk_value(binding, cell));
}

std::uint8_t NestedDiskKind(EC_Expression::TVarEC* cell) {
    if (current_save_galaxy == nullptr || current_save_script == nullptr)
        return static_cast<std::uint8_t>(cell->Kind);
    const auto binding = e2e5_save_inventory::classify(current_save_galaxy, nullptr, cell);
    if (std::string(binding.category) == "STABLE_OBJECT_ID") return object_kind(binding);
    if (std::string(binding.category) == "UNKNOWN")
        bad_reference("E2E5 nested script value is unknown");
    return static_cast<std::uint8_t>(cell->Kind);
}

std::uint32_t NestedDiskValue(EC_Expression::TVarEC* cell) {
    if (current_save_galaxy == nullptr || current_save_script == nullptr) {
        if (!e2e4_script_value::FitsDiskDword(cell->GetDword()))
            bad_reference("E2E5 nested script pointer has no save context");
        return static_cast<std::uint32_t>(cell->GetDword());
    }
    return disk_value(e2e5_save_inventory::classify(current_save_galaxy, nullptr, cell), cell);
}

void SetLoadFormat(bool v168) {
    current_load_v168 = v168;
    pending_pointers.clear();
}
bool LoadFormatIsV168() { return current_load_v168; }
void QueueLoadedPointer(EC_Expression::TVarEC* cell, std::uint8_t disk_kind,
                        std::uint32_t id) {
    if (!current_load_v168 || !IsObjectKind(disk_kind) || cell == nullptr)
        bad_reference("E2E5 typed script reference in unsupported save format");
    pending_pointers.push_back({cell, disk_kind, id});
}

void RebindPending(aGalaxy::TGalaxy* galaxy) {
    std::vector<void*> resolved;
    resolved.reserve(pending_pointers.size());
    for (const auto& pending : pending_pointers) {
        void* object = nullptr;
        switch (pending.kind) {
        case kStar: object = galaxy->IdToStar(pending.id); break;
        case kPlanet: object = galaxy->IdToPlanet(pending.id, false); break;
        case kShip: object = galaxy->IdToShip(pending.id, false); break;
        case kConstellation: object = galaxy->IdToConstellation(pending.id); break;
        default: bad_reference("E2E5 invalid typed script reference");
        }
        if (pending.id != 0 && object == nullptr)
            bad_reference("E2E5 script object ID has no loaded object");
        resolved.push_back(object);
    }
    // A corrupt late ID must not leave earlier script cells rebound.
    for (std::size_t i = 0; i < pending_pointers.size(); ++i) {
        pending_pointers[i].cell->SetDword(reinterpret_cast<std::uintptr_t>(resolved[i]));
    }
    if (current_load_v168) {
        for (std::int32_t i = 0; i < pas::list_count(galaxy->Scripts); ++i) {
            auto* script = pas::list_at<aScript::TScript>(galaxy->Scripts, i);
            auto* cell = script->InitCode->LocalVar->GetVarNE(pas::WideString(u"CurShip"));
            if (cell == nullptr || cell->Kind != EC_Expression::vkDword) continue;
            for (std::size_t i = 0; i < pending_pointers.size(); ++i) {
                const auto& pending = pending_pointers[i];
                if (pending.cell != cell || pending.kind != kShip) continue;
                script->CurrentShip = static_cast<aShip::TShip*>(resolved[i]);
                break;
            }
        }
    }
    pending_pointers.clear();
}

} // namespace srhd_awa::platform::e2e5_save_refs
