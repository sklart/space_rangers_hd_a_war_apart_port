# M22 — portable UI object/layout foundation

Статус: **IN PROGRESS — host/CI/clean ARM64 и symbol audit PASS; финальный
Switch test ещё не запускался.**

M22 реализует CPU-only подмножество observable-семантики `TObjectGI` и
минимального `TPanelGI`, нужное для последующей сборки UI. Это не перенос
оригинского object layout, Delphi RTTI или `GI_MessageLoop`.

## Входит

- `UiObject`/`UiPanel`/`UiTree`: строгая ownership-модель, attach/detach,
  reparent с защитой от циклов, local/absolute coordinates, origin, half-open
  bounds, active/hit-test/name и dirty geometry;
- exact child insertion по depth (включая newer-first для equal depth),
  recursive traversal и `PositionModeW`/scroll geometry;
- recursive clip propagation и `UiImageLeaf`/`UiGILeaf` поверх существующих
  `PortableImageObject` и `GIObject`;
- adapter для `EC_BlockPar`: Style, Pos, PosZ, Size, Sme, Name, Active,
  supported factory `Panel`/SimpleImage/TransImage/AlphaImage/generic Image;
- synthetic C++ + независимый Python oracle: tree serialization и RGB565
  frame A/B с nested panel, origin, equal depth, clip, ModeW, inactive leaf,
  Simple/Trans/Alpha/GI;
- read-only release inventory `tests/probe_m22_ui_release.py`.

## Явно не входит

Mouse/keyboard/focus/hover, callbacks/scripts, Forms/Window/Button/Label/Edit,
шрифты, audio, full incremental redraw optimization, `GI_MessageLoop`,
`TGraphBufGR`, `GR_DX` и Direct3D остаются вне M22.

## Release inventory

На неизменяемой Windows-копии игры probe нашёл 599 control blocks: 267
поддерживаются M22 factory, 332 требуют будущих milestones. Максимальная
глубина control hierarchy — 9, максимум direct children — 13. Детерминированный
поиск полностью поддерживаемого real subtree с минимум двумя уровнями и тремя
visual leaves вернул `NOT FOUND`; поэтому M22 runtime использует synthetic
upstream-shaped tree с реальными M20/M21 resources, а не выдаёт частичный
release screen за поддержанный.

## Runtime boundary

Runtime `UiTree` заменяет ручное размещение entries в `PresentationScene` и
сохраняет M12 callback boundary. Внутри есть nested panels, cloned M20 GI
leaves, M21 Simple leaf, active hidden ModeW scroll leaf и inactive leaf.
GitHub Actions run `37783935697` прошёл retained M12–M21 gates, M22 regression,
tree structural oracle и syntax check release probes. Чистая ARM64-сборка
`de807a7` дала ELF64/AArch64 без undefined symbols; новые M22 objects не
содержат запрещённых GUI/Direct3D символов. Полученный NRO имеет 7,559,472
bytes, SHA-256 `ED08444B2B41056C214D8A809E4FA2A5B72C31E372844119191D68863FCC3716`
и embedded `build_git=de807a7`. Это не hardware PASS: следующий и единственный
физический Switch-запуск остаётся обязательным.

`tests/probe_m22_runtime_oracle.py` независим от production C++ и читает
оригинальный release package. Он фиксирует первый tree: CRC32 `caab2979`,
FNV64 `0e5557af167f16ec`, 691 bytes; выбраны `DATA/Asteroid/00.gai`, `01.gai`,
`02.gai`. Его framebuffer остаётся release-backed
`4f915772`/`52449ae8f8f56c6c`, 1,843,200 bytes. Runtime отвергает mismatch
tree/frame до перехода к dynamic scroll phase.
