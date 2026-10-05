# Milestone 6 — real GR_Main configuration slice

## Scope and release comparison

The authoritative release source is `SpaceRangersHD_decomp` build
`2025-10-13`, commit `730bdf6`. The following translated functions were
compared against its `GR_Main.pas` counterparts before being reached:

| Item | Result | Observable behaviour used here |
|---|---|---|
| `GR_Main::LoadLanguageAndPackages` | RELEASE_EQUIVALENT | requested-language check, fallback to Russian, language install block load, mod-list allocation, selected-mod load, configured packages |
| `GR_Main::LoadSelectedModInstallBlocks` | RELEASE_EQUIVALENT | `Mods\\ModCFG.txt`, `CurrentMod`, `SkipModsOnReload`, base/language install blocks for each selected mod |
| `SelectedLanguage`, `RequestedLanguage`, `SelectedMods`, `SkipModsOnReload`, install globals, `ModSelectionConfigPath` | RELEASE_EQUIVALENT | real definitions from `GR_Main.cpp`; no parallel shim state |
| Portable `FileExists` and path resolution | PORTABILITY_DELTA | safe game-root containment, slash normalization and ASCII case-insensitive physical lookup; no config semantic change |

## Evidence

- `test_gr_main_no_mod_startup.cpp` covers the synthetic no-mod route.
- `test_gr_main_config_startup.cpp` creates `INSTALL.TXT`, Russian language
  configuration, `Mods/ModCFG.txt`, and a selected mod on a case-sensitive
  filesystem. It verifies both separator forms, the real selected-mod path,
  a logging branch, and loose > language-mod > language > mod > base
  precedence.
- `test_gr_main_release_config.cpp` loads the user-owned baseline installation
  through real `GR_Main::LoadLanguageAndPackages`, requires 18 package sources,
  then opens `DATA/Asteroid/00.gai` through `TFileEC`.
- The NRO map retains `AppendLogLineThreadSafe`,
  `LoadSelectedModInstallBlocks`, and `LoadLanguageAndPackages`. It does not
  retain `InitializePlatformRuntimeAndMainWindow`; no obligatory
  `CreateWindowExW`, DirectSound, registry-startup, Direct3D-startup, or Steam
  dependency is introduced by the reached slice.

Host GNU ELF uses `-ffunction-sections -fdata-sections -Wl,--gc-sections`.
The local MSYS COFF linker retains `GR_Main` refptrs despite GC, so the host
regression target is intended for Linux CI/WSL; this is a linker-format
limitation, not a reached portable dependency.

## Deliberately excluded

`GR_Main::InitializePlatformRuntimeAndMainWindow`, `Rangers::ProgramMain`,
window setup, Direct3D, DirectSound, registry behaviour and Steam remain out
of scope. Hardware execution is a separate pending gate.
