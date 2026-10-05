# C++ port scope

## A. Required now

- `CrcUnit.cpp` and runtime helpers: compiled and linked into the NRO.
- `EC_HsFile` package-open semantics: root offset and folder-header disk read are in `platform/filesystem.cpp`; remaining tree/entry I/O awaits a handle adapter.
- `runtime_support.hpp` portability surface needed by these units.

## B. Required later

`Rangers.cpp`, `GR_Main`, Forms/GI UI, `EC_File`, full `EC_HsFile` collection, settings, audio, scripts and renderer bindings.

## C. Optional/platform-specific

Steam, Windows shell/registry/clipboard/cursor, Wine detection, DirectSound, AVI/Xvid and dynamic codec DLLs.

## D. Currently irrelevant

`MatrixGame.dll`, planetary battle ABI, late gameplay screens, achievements and controller UX polish.

C++ source is the pinned submodule `57fa689c630193a66fdea6ca4c79a188814991cd`; generated source is not copied into the port.
