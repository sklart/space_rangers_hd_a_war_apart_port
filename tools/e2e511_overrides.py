"""Switch arcade commands and non-blocking touch scroll invalidation."""


def once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f'E2E-5.11 boundary changed: {old[:100]!r}')
    return text.replace(old, new, 1)


def apply(path, text):
    if path == 'fCfgSettings.cpp':
        # The requested preference is persisted, but the effective renderer is
        # RGB565. Applying an original quality preset must not enable D3D9.
        lines = [line for line in text.splitlines()
                 if 'GlobalsV::HardwareRenderingEnabled = GlobalsV::HardwareRenderingRequested &&' in line]
        if len(lines) != 1:
            raise RuntimeError('Options effective renderer boundary changed')
        text = once(text, lines[0], '            GlobalsV::HardwareRenderingEnabled = false;')
        text = '#include <cstdlib>\n' + text
        marker = '    void TfCfgSettings::OnOpen() {'
        text = once(text, marker, marker + '''
#if defined(E2E5_HOST_SDL_WINDOW)
        if (std::getenv("E2E511_HOST_TOGGLE_HARDWARE")) {
            GR_Main::AlternateViewportEnabled = false;
            GlobalsV::HardwareRenderingRequested = false;
        }
#endif''')
        preference = next(line for line in text.splitlines()
                          if 'GetOptionValue(u"HardwareRender"sv)) != GlobalsV::HardwareRenderingRequested)' in line)
        text = once(text, preference, '''
#if defined(E2E5_HOST_SDL_WINDOW)
        if (std::getenv("E2E511_HOST_TOGGLE_HARDWARE"))
            // The original getter/setter searches the currently active group.
            SetOptionValue(u"HardwareRender"sv, 1);
#endif
''' + preference)
        marker = '    void TfCfgSettings::ApplyClicked(GI_MessageLoop::TObjectGI* Sender) {'
        start = text.index(marker)
        end = text.index('\n    }', start)
        body = text[start:end]
        marker = '        RequestClose(1);'
        body = once(body, marker, '''#if defined(E2E5_HOST_SDL_WINDOW)
        if (std::getenv("E2E511_HOST_TOGGLE_HARDWARE"))
            srhd_awa::platform::e2e_stage::Log(GlobalsV::HardwareRenderingRequested && !GlobalsV::HardwareRenderingEnabled ?
                "HOST hardware preference=1 effective renderer=rgb565 PASS" :
                "HOST hardware renderer policy FAILED");
#endif
''' + marker)
        text = text[:start] + body + text[end:]
    if path == 'fGalaxy2.cpp':
        marker = '    void TfGalaxy2::OnOpen() {'
        text = once(text, marker, marker + '''
        srhd_awa::platform::e2e_stage::Log(CapturePreviewOnOpen ?
            "galaxy open mode=preview" : "galaxy open mode=interactive");''')
        text = '#include "e2e_stage.hpp"\n' + text
        # Preserve both original thumbnails. Do not pump input into a preview
        # that already requested close during OnOpen; restore flags on failure.
        marker = '    void CaptureGalaxyPreview(GI_MessageLoop::TMessageLoopGI* ParentLoop) {'
        text = once(text, marker, marker + """
        const auto previous_parent = Globals::GalaxyScreen->ParentLoop;
        const bool previous_sounds = Globals::GalaxyScreen->PlayTransitionSounds;
        const bool previous_capture = Globals::GalaxyScreen->CapturePreviewOnOpen;
        pas::ScopeExit restore_preview = [&]() noexcept {
            Globals::GalaxyScreen->PlayTransitionSounds = previous_sounds;
            Globals::GalaxyScreen->CapturePreviewOnOpen = previous_capture;
            Globals::GalaxyScreen->ParentLoop = previous_parent;
        };
        srhd_awa::platform::e2e_stage::Log("save preview galaxy begin");""")
        text = once(text, '        ParentLoop->Present();',
                    '        ParentLoop->Present();\n        srhd_awa::platform::e2e_stage::Log("save preview galaxy complete");')
    if path == 'GI_MessageLoop.cpp':
        text = '#include "types/GI_Image.hpp"\n' + text
        # OnOpen can close a one-shot preview. Checking ExitCode first avoids
        # another input/audio pump and reentrant callbacks after that close.
        old = 'while (GR_Main::GR_WinMessage(this->bind_ProcessWindowMessage()) != 0 && ExitCode == 0)'
        if text.count(old) != 2:
            raise RuntimeError('screen loop close gate boundary changed')
        text = text.replace(old, 'while (ExitCode == 0 && GR_Main::GR_WinMessage(this->bind_ProcessWindowMessage()) != 0)')
        # Nested loops keep CurrentScreenId at their parent's ID. Name them,
        # then record each startup boundary so a hang is attributable.
        for kind in ('Run', 'RunContinuous'):
            marker = f'    std::int32_t TMessageLoopGI::{kind}() {{'
            begin = text.index(marker)
            end = text.index('\n    }', begin)
            body = text[begin:end]
            for statement, phase in [
                ('            RootUiObject->UpdateAbsolutePosition();', 'geometry'),
                ('            RootUiObject->UpdateSubtreeHitBounds();', 'bounds'),
                ('            RootUiObject->OnActivate();', 'activate')]:
                log = '''
            {
                const pas::AnsiString name = static_cast<pas::AnsiString>(RegisteredLoopName);
                char detail[192]{};
                std::snprintf(detail, sizeof(detail), "loop name=%.120s phase=PHASE ready exit=%d requested=%u", name.c_str(), ExitCode, static_cast<unsigned>(GlobalsV::RequestedScreenId));
                srhd_awa::platform::e2e_stage::Log(detail);
            }'''.replace('PHASE', phase)
                if phase == 'bounds':
                    log += '''
            for (const auto key : {u"ImageBG"sv, u"BGCity"sv, u"ImageBG2"sv}) {
                auto* control = ContentPanel->FindByNameRecursive(key);
                if (auto* image = pas::class_cast_if<GI_Image::TImageGI*>(control)) {
                    const pas::AnsiString path = static_cast<pas::AnsiString>(image->GetImagePath());
                    const pas::AnsiString name = static_cast<pas::AnsiString>(RegisteredLoopName);
                    char detail[320]{};
                    std::snprintf(detail, sizeof(detail),
                        "background state loop=%.60s path=%.140s active=%u parent_active=%u bounds=%d,%d,%d,%d hardware=%u",
                        name.c_str(), path.c_str(), static_cast<unsigned>(image->Active),
                        static_cast<unsigned>(image->Parent && image->Parent->Active),
                        image->HitTestBounds.Left, image->HitTestBounds.Top,
                        image->HitTestBounds.Right, image->HitTestBounds.Bottom,
                        static_cast<unsigned>(GlobalsV::HardwareRenderingEnabled));
                    srhd_awa::platform::e2e_stage::Log(detail);
                }
            }'''
                body = once(body, statement, statement + log)
            # Flush only the three startup boundaries; no per-frame SD flush.
            body = body.replace('                srhd_awa::platform::e2e_stage::Log(detail);\n            }', '''                srhd_awa::platform::e2e_stage::Log(detail);
                std::lock_guard lock(srhd_awa::platform::e2e_stage::g_stage_mutex);
                if (srhd_awa::platform::e2e_stage::g_log.file)
                    std::fflush(srhd_awa::platform::e2e_stage::g_log.file);
            }''')
            text = text[:begin] + body + text[end:]
    if path == 'ab_MainForm.cpp':
        text = '#include "e2e511_arcade_commands.hpp"\n' + text
        text = once(text, '                    if (e2e_steering) {\n                        auto* ship = ab_Ship::PlayerArcadeShip;', '''                    if (e2e_steering) {
                        ab_Global::ArcadeLastInputTick = ab_Global::ArcadeTickCount;
                        if (ab_Global::ArcadeAutopilotEnabled) {
                            ab_Global::ArcadeAutopilotEnabled = false;
                            UpdateAutopilotButtons();
                        }
                        auto* ship = ab_Ship::PlayerArcadeShip;''')
        marker = '    void TfAB::BattleKeyDown(GI_MessageLoop::TObjectGI* Sender, std::uint32_t VirtualKey) {'
        text = once(text, marker, marker + '''
        if (VirtualKey == 0xdb || VirtualKey == 0xdd) {
            auto* ship = ab_Ship::PlayerArcadeShip;
            if (ship != nullptr && !VictoryPanel->Active && !DefeatPanel->Active) {
                const bool secondary = VirtualKey == 0xdd;
                const auto next = srhd_awa::platform::NextArcadeWeapon(
                    secondary ? ship->SecondaryWeapon : ship->PrimaryWeapon,
                    ship->WeaponCount, secondary, [&](int index) {
                        return (ship->Weapons[index].SlotData & aItem::EquipmentSecondaryFireFlag) != 0;
                    });
                if (next >= 0) {
                    ship->SelectWeapon(next);
                    UpdateWeaponHighlights(true);
                }
            }
            return;
        }''')
        # Keep the weapon name, replace keyboard-only select/group instructions.
        begin = text.index('                if ((ab_Ship::PlayerArcadeShip->Weapons[Value].SlotData & aItem::EquipmentSecondaryFireFlag) != 0) {', text.index('    void TfAB::UpdateWeaponPanel()'))
        end = text.index('\n                }', text.index('                    }());', text.index('                } else {', begin))) + len('\n                }')
        text = text[:begin] + '''                WeaponButtons[SlotIndex]->HelpText = WeaponName +
                    u" (L: основное; R: дополнительное; ZR/ZL: огонь; A по значку: группа)"_w;''' + text[end:]
        marker = '    void TfAB::UpdateAutopilotButtons() {'
        text = once(text, marker, marker + '''
        AutoButton->HelpText = u"[-] Автопилот / ручное управление"_w;
        ManualButton->HelpText = AutoButton->HelpText;''')
        text = once(text, '            BattleHelpLabel->SetText(Sender->HelpText);', '''            auto help = EC_Str::ReplaceAllWideString(Sender->HelpText, u">Enter</color>"_wref.get(), u">X</color>"sv);
            help = EC_Str::ReplaceAllWideString(help, u">Space</color>"_wref.get(), u">A: нажать кнопку курсором</color>"sv);
            BattleHelpLabel->SetText(help);''')
        text = once(text, 'aConst::LocalizedColorText(u"Help.ABSphere"_wref.get())',
            'EC_Str::ReplaceAllWideString(aConst::LocalizedColorText(u"Help.ABSphere"_wref.get()), u">Enter</color>"_wref.get(), u">X</color>"sv)')
    if path == 'fStarMap.cpp':
        # pstView flushes queued draws synchronously for each move before doing
        # a framebuffer blit. Touch moves should invalidate for the next frame.
        marker = 'if (center.X != target.X || center.Y != target.Y) SetMapCenterManually(target);'
        text = once(text, marker, '''if (center.X != target.X || center.Y != target.Y) {
                        const auto previous = MapControls->ScrollType;
                        pas::ScopeExit restore_scroll = [&]() noexcept { MapControls->ScrollType = previous; };
                        MapControls->ScrollType = GI_Panel::pstAll;
                        SetMapCenterManually(target);
                    }''')
    return text
