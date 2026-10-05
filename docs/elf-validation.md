# ELF/NRO pre-hardware validation

Проверено для `port/switch/build/SpaceRangersHDAWarApart.elf` devkitA64 tools:

- ELF class: `ELF64`;
- machine: `AArch64`;
- dynamic section не содержит `NEEDED` Win32 DLL/PE imports;
- linked symbols содержат `Package::ReadPayload`, `uncompress`/`uncompress2`
  (zlib) и `OKGR_Fill_WORD` (OKGF).

Это статическая pre-hardware проверка, а не доказательство запуска на Switch.

Проверенный NRO: `SpaceRangersHDAWarApart.nro`, 6 115 523 bytes,
SHA-256 `E32516ACF3F6F3E5A3CA69DCCBC539BE559C9405C6F58C11BB05A3ED8EEEB3A6`.
