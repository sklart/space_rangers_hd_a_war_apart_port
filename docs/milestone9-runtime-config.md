# Milestone 9 — DAT configuration and portable user settings

Release oracle remains `SpaceRangersHD_decomp` build `2025-10-13`, commit
`730bdf6`; the translated C++ oracle is
`57fa689c630193a66fdea6ca4c79a188814991cd`.

## Reached path

The portable startup creates the translated `GR_Main::TCCInterface`, calls the
unmodified translated `GR_Main::LoadDatConfigAndModOverrides()`, and loads:

```text
CFG/Main.dat
CFG/<LanguageInstallConfig.Lang>/Lang.dat
CFG/CacheData.dat
```

The original merge path is retained: base data is loaded first and selected
`Mods/<mod>/CFG/...` files merge through `TBlockParEC::MergeFrom` and
`TDataEC::MergeFrom`. `runtime_settings_slice` then borrows real `ML`, `Data`
and `ZPos` blocks for `UiStyleConfig`, `GameDataConfig` and `UiDepthConfig`,
calls `LoadInformationColorTags`, builds `WideCaseTable` from `CaseConv`, and
executes the release `PlanetQuest` cache-root merge.

## Encrypted-DAT and user-file boundaries

The real `EC_BlockPar` and `EC_Data` encrypted loaders are linked. Their
asset-free regression constructs the actual outer-count/outer-CRC/inner-CRC,
seed/XOR and zlib wire format. It covers valid block/data fixtures, base/mod
override order, bad outer size, outer CRC, inner CRC, truncated payload and an
invalid compressed stream. A checksum failure leaves the already loaded block
unchanged.

The writable root is explicit: Switch uses
`sdmc:/switch/space-rangers-hd-a-war-apart/`, while host tests supply a
temporary root. Its `config/`, `save/`, `logs/` and `runtime/` layout is
created on demand. `ResolveConfigPath` rejects traversal and arbitrary write
destinations. `TFileEC::CreateNew` uses a side-table loose-file handle only
inside `config/`; package entries remain read-only.

On first start, the game-root `cfg.txt` template is loaded and materialized as
`<user-root>/config/CFG.TXT`, then release-required migrations add
`CurrentVersion`, `HardwareRender`, `MultiThread` and `VideoMemSizeLimit` when
absent. `newgame.txt` is optional. Registry, CPU probing, GlobalCache,
audio/music backends and global UI remain deferred.

## Validation

`host-milestone9-regression` includes the M8 immutable renderer golden
`3a9dfdc6db5aacd7`, DAT/config runtime state, user writable files and
cleanup/re-init. CI run `37362302574` passed the asset-free suite.

A clean ARM64 build produced an ELF64 AArch64 NRO:

```text
SpaceRangersHDAWarApart.nro
size: 7078083
SHA-256: 668CD8317245DBFB08269653D35CAEB039B2F5DA77CFE31D3A3980A7EB3ED84B
```

The ELF retains `LoadDatConfigAndModOverrides` and both encrypted loader
symbols. It does not retain `Direct3DCreate9`, `DirectSoundCreate`,
`RegOpenKeyEx`, `SteamAPI`, `Rangers::ProgramMain` or
`InitializeGlobalUiRuntime`.

Release-file semantic fingerprints and actual Switch `port.log` evidence are
not substituted by these asset-free checks; hardware validation remains
pending.
