# Current status — M12 runtime loop ready for hardware validation

Baseline SHA-256: `83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98`.

- M7 startup/platform: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M8 RGB565 + SDL presentation: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M9 DAT, CFG and derived runtime state: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.
- M11 GlobalCache and cached real resource: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PASS**.

The physical Switch run has confirmed: packages → INSTALL/language → DAT → CFG → GlobalCache → cached resource → RGB565 → SDL present → clean shutdown. It does not confirm gameplay or UI.

## Milestone 12

M12 replaces the diagnostic five-second frame hold with a persistent, portable runtime loop. It preserves M7–M11 selftests, loads scalar release settings without Windows probing, keeps audio/music backends deferred, initializes the RGB565 renderer and interface blend palette as runtime state, and exits cleanly on Switch `PLUS`. M13 UI initialization remains deferred pending its dependency analysis.

Status: **HOST PASS / CI PASS / ARM64 BUILD PASS / HARDWARE PENDING**. M12 has not yet been accepted on physical Switch hardware.