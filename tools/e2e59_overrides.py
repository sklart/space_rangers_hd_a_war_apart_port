"""Follow-up fixes from the 1e47335 hardware report; pinned sources stay intact."""
import re
if __package__:
    from .e2e5_ui_width_overrides import widen_pointer_casts
else:
    from e2e5_ui_width_overrides import widen_pointer_casts


def once(text, old, new, count=1):
    if text.count(old) != count:
        raise RuntimeError(f"E2E59 boundary changed: {old[:90]!r}")
    return text.replace(old, new)


def width(path, text):
    action_names = r"(?:ScriptItemsAct|RunActionCode|RunItemConfigActionCode|RunCustomShipInfoActionCode)"
    signature = re.compile(r"std::int32_t ((?:\w+::)?" + action_names + r"\([^\n]*std::int32_t Param\))")
    # Only the transient mixed event parameter changes. Numeric event parameters
    # keep signed semantics; equipment-purchase events carry an object address.
    text = signature.sub(lambda m: "std::intptr_t " + m[1].replace("std::int32_t Param", "std::intptr_t Param"), text)
    if path in ("aShip.cpp", "aScript.cpp"):
        pattern = re.compile(r"    std::intptr_t (?:\w+::)?" + action_names + r"\([^\n]+\) \{")
        for match in reversed(list(pattern.finditer(text))):
            end = text.index("\n    }", match.end())
            body = text[match.start():end]
            body = body.replace("std::int32_t Result", "std::intptr_t Result")
            body = body.replace("static_cast<std::uint32_t>(Param)", "Param")
            body = body.replace("static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(pas::list_get(Globals::ScriptActionParamStack",
                                "static_cast<std::intptr_t>(reinterpret_cast<std::uintptr_t>(pas::list_get(Globals::ScriptActionParamStack")
            text = text[:match.start()] + body + text[end:]
    if path == "fEquipmentShop.cpp":
        text = once(text, "std::int32_t Destination{};", "std::intptr_t Destination{};")
        text = widen_pointer_casts(text)
    if path == "fScaner.cpp":
        text = once(text, "pas::WideString Help, std::int32_t Data)", "pas::WideString Help, std::intptr_t Data)")
        text = widen_pointer_casts(text)
    if path == "fScore.cpp":
        text = once(text,
            "reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Buffer->Data)) + static_cast<std::int32_t>(sizeof(EC_Buf::TEncodedTableHeaderEC)))))",
            "(static_cast<std::uint8_t*>(Buffer->Data) + sizeof(EC_Buf::TEncodedTableHeaderEC))", 2)
    # All occurrences are transient callback/segment metadata, never disk records.
    text = text.replace("std::int32_t UserData", "std::intptr_t UserData")
    if path in ("fCount1.cpp", "types/fCount1.hpp"):
        text = once(text, "GI_MessageLoop::PCallbackTimerGI Timer, std::int32_t Data",
                    "GI_MessageLoop::PCallbackTimerGI Timer, std::intptr_t Data")
    if path == "types/GI_MessageLoop.hpp":
        text = once(text, "pas::Method<void(PCallbackTimerGI, std::int32_t)>",
                    "pas::Method<void(PCallbackTimerGI, std::intptr_t)>")
    if path == "types/SE_Space.hpp":
        text = once(text, "pas::Method<void(PSpaceTimerSE, std::int32_t)>",
                    "pas::Method<void(PSpaceTimerSE, std::intptr_t)>")
    if path == "types/ab_StopLine.hpp":
        text = once(text, "std::int32_t UserValue;", "std::intptr_t UserValue;")
    if path == "types/EC_CacheFont.hpp":
        # Parsed inline-control metadata in TCFontEC.Objects, not the AFT file records.
        text = once(text, "std::int32_t ObjectId;", "std::uintptr_t ObjectId;")
    text = text.replace("static_cast<std::uintptr_t>(static_cast<std::uint32_t>(UserData))",
                        "static_cast<std::uintptr_t>(UserData)")
    if path in ("GI_AImage.cpp", "fStarMap.cpp"):
        text = widen_pointer_casts(text)
    if path == "fPlanetNO.cpp":
        # The translated flood-fill walks heap pointers using 32-bit address arithmetic.
        pattern = r'static_cast<std::uint32_t>\(static_cast<std::uint32_t>\(reinterpret_cast<std::uintptr_t>\((ReadNode|WriteNode|NeighborVisited|NeighborPixel|Pixel|CursorVisited)\)\)(.*?)\)\)\);'
        text, count = re.subn(pattern,
            lambda m: 'static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(' + m[1] + ')' + m[2] + ')));', text)
        if count != 14:
            raise RuntimeError(f"probe trajectory pointer arithmetic changed: {count}")
    if path == "types/fShip2.hpp":
        text = once(text, "std::int32_t DisplayedItemKey;", "std::intptr_t DisplayedItemKey;")
    if path == "fShip2.cpp":
        text = once(text, "pas::WideString Hint, std::int32_t Data)", "pas::WideString Hint, std::intptr_t Data)")
        text = widen_pointer_casts(text)
    if path == "types/fStarMap.hpp":
        declaration = "        void MapLeftButtonDown(GI_MessageLoop::TObjectGI* Sender, std::uint32_t KeyState, WindowsSdk::TPoint Point);"
        text = once(text, declaration, declaration + "\n" + declaration.replace("MapLeftButtonDown", "MapOriginalLeftButtonDown") +
                    "\n" + declaration.replace("MapLeftButtonDown", "MapTouchRelease"))
    if path == "types/aGalaxy.hpp":
        for kind, name in (("uint32_t", "GateFilmId"), ("uint32_t", "EffectFilmId"),
                           ("int32_t", "FilmObjectId")):
            text = once(text, f"std::{kind} {name};", f"std::uintptr_t {name};")
    if path == "aGalaxy.cpp":
        text = once(text, 'SysUtils::Int64ToStr(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Obj))), ",23,17,0>"',
                    'SysUtils::Int64ToStr(static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(Obj))), ",23,17,0>"')
        text = once(text, "static_cast<std::uintptr_t>(static_cast<std::uint32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Missile->OwnerShip))))",
                    "reinterpret_cast<std::uintptr_t>(Missile->OwnerShip)")
        text = once(text, "static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Missile->Target)) != static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Missile->OwnerShip))",
                    "Missile->Target != Missile->OwnerShip")
        for field, kind, value in (("GateFilmId", "uint32_t", "ObjectFilm"),
                                   ("EffectFilmId", "uint32_t", "ObjectFilm"),
                                   ("FilmObjectId", "int32_t", "EffectFilm")):
            text = once(text, f"->{field} = static_cast<std::{kind}>(reinterpret_cast<std::uintptr_t>({value}));",
                        f"->{field} = reinterpret_cast<std::uintptr_t>({value});",
                        2 if field == "FilmObjectId" else 1)
        for name, count in (("Hole->FilmObjectId", 2), ("GateEntry->GateFilmId", 1),
                            ("GateEntry->EffectFilmId", 1)):
            text = once(text, f"static_cast<std::uint32_t>({name})", name, count)
    if path == "aScriptFun.cpp":
        for screen in ("RuinsTalkScreen", "GovernmentScreen", "TalkScreen"):
            pattern = r'(Globals::' + screen + r'->AddChoice\([^\n]*?)static_cast<std::int32_t>\(reinterpret_cast<std::uintptr_t>\(Entry\)\)'
            text, count = re.subn(pattern, r'\1srhd_awa::platform::e2e5_pointer_payload::Register(Globals::' + screen + ', Entry)', text)
            if count != 2:
                raise RuntimeError(f"injected choice pointer producer changed: {screen}")
        text = once(text, "static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(Template)) + static_cast<std::int32_t>(sizeof(aConst::TMicroModuleInfo))",
                    "reinterpret_cast<std::uintptr_t>(Template) + sizeof(aConst::TMicroModuleInfo)")
        text = '#include "e2e5_pointer_payload.hpp"\n' + text
        # The shared script overlay has already widened the generic outer cast.
        for name, count in (("hole->FilmObjectId", 2), ("hole_2->FilmObjectId", 1)):
            text = once(text, f"static_cast<std::uintptr_t>({name})", name, count)
        begin = text.index("    void SF_CreateActCodeEvent(")
        end = text.index("\n    void ", begin + 1)
        body = text[begin:end].replace("std::int32_t Param{};", "std::intptr_t Param{};")
        body = re.sub(r"Param = (av\[\d+\])->GetInt\(\);",
            r"Param = srhd_awa::platform::action_param::Get(\1, srhd_awa::platform::action_param::IsPointer(ActionType));", body)
        for name in ("scriptItemsAct", "runActionCode", "runItemConfigActionCode", "runCustomShipInfoActionCode"):
            body = body.replace(f"std::int32_t {name}", f"std::intptr_t {name}")
            body = re.sub(r"(\w+)->SetInt\(" + name + r"\);",
                r"srhd_awa::platform::action_param::Set(\1, " + name + ", srhd_awa::platform::action_param::IsPointer(ActionType));", body)
        text = text[:begin] + body + text[end:]
        begin = text.index("    void SF_ScriptItemActParam(")
        end = text.index("\n    void ", begin + 1)
        body = text[begin:end]
        body = once(body, "av[0]->SetInt(static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(pas::list_get(Globals::ScriptActionParamStack, pas::list_count(Globals::ScriptActionParamStack) - 1))));",
            "srhd_awa::platform::action_param::Set(av[0], static_cast<std::intptr_t>(reinterpret_cast<std::uintptr_t>(pas::list_get(Globals::ScriptActionParamStack, pas::list_count(Globals::ScriptActionParamStack) - 1))), srhd_awa::platform::action_param::CurrentIsPointer());")
        body = once(body, "av[1]->GetInt()", "srhd_awa::platform::action_param::Get(av[1], srhd_awa::platform::action_param::CurrentIsPointer())")
        text = text[:begin] + body + text[end:]
        text = once(text, "std::int32_t cpp_left_2 = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(pas::list_get(Globals::ScriptActionParamStack, pas::list_count(Globals::ScriptActionParamStack) - 1)));\n                Matches = cpp_left_2 == av[2]->GetInt();",
            "std::intptr_t cpp_left_2 = static_cast<std::intptr_t>(reinterpret_cast<std::uintptr_t>(pas::list_get(Globals::ScriptActionParamStack, pas::list_count(Globals::ScriptActionParamStack) - 1)));\n                Matches = cpp_left_2 == srhd_awa::platform::action_param::Get(av[2], srhd_awa::platform::action_param::CurrentIsPointer());")
        text = '#include "e2e59_action_param.hpp"\n' + text
    return text


def star_map(text):
    text = '#include "e2e59_map_swipe.hpp"\n#include "e2e_stage.hpp"\n#include "win32_compat_messages.hpp"\n' + text
    text = once(text, "namespace fStarMap {", "namespace fStarMap {\n"
                "    static srhd_awa::platform::MapSwipe e2e_swipe;\n"
                "#if defined(E2E5_HOST_SDL_WINDOW)\n"
                "    static int e2e_touch_order;\n"
                "    static pas::Object* e2e_touch_target;\n"
                "    static EC_Struct::TPointF e2e_touch_destination;\n"
                "#endif\n")
    text = once(text, "        Mode = smmOrders;", '''        Mode = smmOrders;
#if defined(E2E5_HOST_SDL_WINDOW)
        srhd_awa::platform::e2e_stage::LogE2E3("HOST map orders ready");
#endif''')
    text = once(text, "void TfStarMap::OnClose() {", "void TfStarMap::OnClose() {\n"
                "        e2e_swipe.Cancel();\n"
                "        if (FocusedControl && FocusedControl == MapControls) SetFocusedControl(nullptr);")
    text = once(text, "void TfStarMap::StopOrderMode() {", '''void TfStarMap::StopOrderMode() {
        if (e2e_swipe.owner == this) {
            e2e_swipe.Cancel();
            if (FocusedControl == MapControls) SetFocusedControl(nullptr);
#if defined(E2E5_HOST_SDL_WINDOW)
            srhd_awa::platform::e2e_stage::LogE2E3("HOST native map touch canceled by mode change");
#endif
        }''')
    text = once(text, "void TfStarMap::OnOpen() {", "void TfStarMap::OnOpen() {\n        e2e_swipe.Cancel();")
    text = once(text, "MapControls->LeftButtonDownCallback = pas::bind_method<&TfStarMap::MapLeftButtonDown>(this);",
                "MapControls->LeftButtonDownCallback = pas::bind_method<&TfStarMap::MapLeftButtonDown>(this);\n"
                "        MapControls->LeftButtonUpCallback = pas::bind_method<&TfStarMap::MapTouchRelease>(this);")
    signature = "    void TfStarMap::MapLeftButtonDown(GI_MessageLoop::TObjectGI* Sender, std::uint32_t KeyState, WindowsSdk::TPoint Point) {"
    text = once(text, signature, '''    void TfStarMap::MapLeftButtonDown(GI_MessageLoop::TObjectGI* Sender, std::uint32_t KeyState, WindowsSdk::TPoint Point) {
        if (!srhd_awa::platform::win32_compat::CurrentMessageIsTouch()) {
            e2e_swipe.Cancel();
            MapOriginalLeftButtonDown(Sender, KeyState, Point);
            return;
        }
        if (MainPanel->NavigationLocked || Globals::ShipScreen->ReopenRequested || IsMapPointBlocked(Sender, Point)) return;
        const auto center = GetMapCenter();
        e2e_swipe.Begin(this, Point.X, Point.Y, center.X, center.Y, KeyState);
#if defined(E2E5_HOST_SDL_WINDOW)
        e2e_touch_order = aPlayer::GetPlayer()->Order;
        e2e_touch_target = aPlayer::GetPlayer()->OrderTarget;
        e2e_touch_destination = aPlayer::GetPlayer()->OrderDestination;
#endif
        MapControls->MessageLoop->SetFocusedControl(MapControls);
    }

    void TfStarMap::MapTouchRelease(GI_MessageLoop::TObjectGI* Sender, std::uint32_t KeyState, WindowsSdk::TPoint Point) {
        if (e2e_swipe.owner != this) {
#if defined(E2E5_HOST_SDL_WINDOW)
            if (srhd_awa::platform::win32_compat::CurrentMessageIsTouch())
                srhd_awa::platform::e2e_stage::LogE2E3("HOST native map touch release without capture");
#endif
            return;
        }
        e2e_swipe.Move(this, Point.X, Point.Y);
        const bool tap = Mode == smmOrders && !e2e_swipe.dragging && srhd_awa::platform::win32_compat::CurrentMessageIsTouch();
        const auto keys = e2e_swipe.keys;
#if defined(E2E5_HOST_SDL_WINDOW)
        if (e2e_swipe.dragging) {
            const auto center = GetMapCenter();
            const bool moved = center.X != e2e_swipe.center_x || center.Y != e2e_swipe.center_y;
            const auto* player = aPlayer::GetPlayer();
            const bool preserved = player->Order == e2e_touch_order && player->OrderTarget == e2e_touch_target &&
                player->OrderDestination.X == e2e_touch_destination.X && player->OrderDestination.Y == e2e_touch_destination.Y;
            srhd_awa::platform::e2e_stage::LogE2E3(moved && preserved ?
                "HOST native map swipe moved=1 ship_order_preserved=1" : "HOST native map swipe FAILED");
        }
#endif
        e2e_swipe.Cancel(); // Release may be delivered through focus and root in the same message.
        if (MapControls->MessageLoop->FocusedControl == MapControls)
            MapControls->MessageLoop->SetFocusedControl(nullptr);
        if (tap) MapOriginalLeftButtonDown(Sender, keys, Point);
    }

    void TfStarMap::MapOriginalLeftButtonDown(GI_MessageLoop::TObjectGI* Sender, std::uint32_t KeyState, WindowsSdk::TPoint Point) {
#if defined(E2E5_HOST_SDL_WINDOW)
        if (srhd_awa::platform::win32_compat::CurrentMessageIsTouch())
            srhd_awa::platform::e2e_stage::LogE2E3("HOST native map tap committed");
#endif''')
    signature = "    void TfStarMap::MapMouseMove(GI_MessageLoop::TObjectGI* Sender, std::uint32_t KeyState, WindowsSdk::TPoint Point) {"
    text = once(text, signature, signature + '''
        if (e2e_swipe.owner == this) {
            if (Mode != smmOrders || MainPanel->NavigationLocked || Globals::ShipScreen->ReopenRequested) {
                e2e_swipe.Cancel();
                if (MapControls->MessageLoop->FocusedControl == MapControls)
                    MapControls->MessageLoop->SetFocusedControl(nullptr);
            } else {
                if (!srhd_awa::platform::win32_compat::CurrentMessageIsTouch()) return;
                if (e2e_swipe.Move(this, Point.X, Point.Y)) {
                    const WindowsSdk::TPoint target{e2e_swipe.center_x + e2e_swipe.x - Point.X,
                                                   e2e_swipe.center_y + e2e_swipe.y - Point.Y};
                    const auto center = GetMapCenter();
                    if (center.X != target.X || center.Y != target.Y) SetMapCenterManually(target);
                }
                return;
            }
        }''')
    return text


def film_history(text):
    text = '#include "e2e58_file_lock.hpp"\n' + text
    text = text.replace("pas::critical_enter(Lock);",
                        "srhd_awa::platform::FileLockGuard e2e_lock(Lock);")
    text = text.replace("pas::critical_leave(Lock);", "e2e_lock.Unlock();")
    begin = text.index("        PFilmHistoryEntry Entry{};", text.index("void TFilmFile::AddFilm("))
    end = text.index("\n    }", begin)
    text = text[:begin] + '''        auto* Buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
        pas::ScopeExit free_buffer = [&]() noexcept { pas::free(Buffer); };
        Film->SaveToBuffer(Buffer);
        Buffer->SetPosition(0);
        if (Buffer->DataSize < 1 || GlobalsV::FilmHistoryLimit < 1)
            pas::raise(pas::make_exception<pas::Exception>("Error in TFilmFile.FilmAdd"_a));
        srhd_awa::platform::FileLockGuard e2e_lock(Lock);
        // Allocate before evicting; a failed serialization/allocation leaves history intact.
        auto* Entry = AppendEntry();
        Entry->Turn = Film->Turn;
        Entry->Buffer = Buffer;
        Buffer = nullptr;
        while (GetCount() > GlobalsV::FilmHistoryLimit) {
            auto* Oldest = FirstEntry;
            for (auto* Candidate = FirstEntry; Candidate != Entry; Candidate = Candidate->Next)
                if (Candidate->Turn < Oldest->Turn) Oldest = Candidate;
            RemoveEntry(Oldest);
        }''' + text[end:]
    text = once(text, "        Film->LoadFromBuffer(Entry->Buffer);\n        Entry->Buffer->SetPosition(0);",
                "        pas::ScopeExit rewind = [&]() noexcept { Entry->Buffer->SetPosition(0); };\n"
                "        Film->LoadFromBuffer(Entry->Buffer);")
    begin = text.index("        srhd_awa::platform::FileLockGuard", text.index("void TFilmFile::LoadEntryFromBuffer("))
    end = text.index("\n    }", begin)
    text = text[:begin] + '''        const auto Turn = EC_Buf::TBufEC_GetInt32(Buffer);
        auto* Payload = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
        pas::ScopeExit free_payload = [&]() noexcept { pas::free(Payload); };
        Buffer->ReadLengthPrefixedBuffer(Payload);
        srhd_awa::platform::FileLockGuard e2e_lock(Lock);
        auto* Entry = AppendEntry();
        Entry->Turn = Turn;
        Entry->Buffer = Payload;
        Payload = nullptr;''' + text[end:]
    return text


def film_index(text):
    begin = text.index("    void TEFilm::SaveToBuffer(")
    end = text.index("\n    }", begin)
    body = text[begin:end]
    if body.count("ObjToNom(") != 26:
        raise RuntimeError(f"film object ordinal lookups changed: {body.count('ObjToNom(')}")
    body = body.replace("ObjToNom(", "e2e_index.Find(")
    opening = body.index(" {\n") + len(" {\n")
    body = body[:opening] + '''        auto e2e_index = srhd_awa::platform::MakeFilmIndex<TEFilmObj*>(
            [this](TEFilmObj* object) { return ObjToNom(object); });
''' + body[opening:]
    return '#include "e2e59_film_index.hpp"\n' + text[:begin] + body + text[end:]


def turn_profile(path, text):
    methods = {
        "ThreadCalc.cpp": [("void ProcessPlayerStarTurn() {", "player-star-calc")],
        "aGalaxy.cpp": [("void TGalaxy::NextDay() {", "galaxy-calc")],
        "aCalc.cpp": [("void CalculatePlayerStarTurnAndWait() {", "player-star-wait"),
                      ("void CalculateGalaxyTurnAndWait() {", "galaxy-wait")],
        "fPanelMain.cpp": [("void TfPanelMain::EndTurnClicked(GI_MessageLoop::TObjectGI* Sender) {", "end-turn-ui")],
        "fFilmFile.cpp": [("void TFilmFile::AddFilm(aEFilm::TEFilm* Film) {", "history-serialize")],
    }
    if path not in methods:
        return text
    for marker, name in methods[path]:
        text = once(text, marker, marker + f'\n        srhd_awa::platform::TurnProfile e2e_turn_profile("{name}");')
    return '#include "e2e59_turn_profile.hpp"\n' + text
