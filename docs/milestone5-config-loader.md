# Milestone 5 — configuration and package loader

The port retains the translated `EC_Buf`, `EC_BlockPar`, `EC_Str`, `EC_Mem`
and `aPacket` implementations from the `730bdf6` baseline. The Switch adapter
supplies the `EC_HsFile::TPackCollectionEC` backend used by real `EC_File`.

`aPacket::InitializePackageCollection()` first inserts the loose-file source.
`aPacket::LoadConfiguredPackages()` then appends package lists in the upstream
order: language mods, language, mods, base install. Later collection entries
are therefore lower-priority during `OpenEntryByPathAcrossPackages` traversal.

Historical note: M5 used `gr_main_package_config_shim.cpp` for the four
configuration globals and unreachable failure-path hooks. M6 removes that
shim from the active build: the real globals and configuration functions now
come from `GR_Main.cpp`, still without window, registry/Steam, audio or
renderer startup.

Validation:

- `host-blockpar-synthetic-test` parses loose config text, creates empty
  packages and asserts the complete language-mod/language/mod/base order.
- `host-configured-package-test` parses the supplied release `INSTALL.TXT` and
  `INSTALL_RUSSIAN.TXT`, opens 18 sources, and reads
  `DATA/Asteroid/00.gai` through the configured collection.
- CI runs the asset-free real `EC_Buf`/`EC_BlockPar`/`aPacket` synthetic test.

The host and ARM64 build evidence does not replace a physical Switch launch;
that gate remains `HARDWARE PENDING`.
