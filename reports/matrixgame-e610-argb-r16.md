# MatrixGame E610 r16 — ARGB8 roundtrip getters

После r15 в validation остаются восемь lround calls: четыре для texture factor
(NativeRenderState::Render(60)) и четыре для global ambient (FixedVertexState::Render(139)).
Они восстанавливают uint32 ARGB из private float colors. Обе величины создаются
только ARGB8 setters: Combiner::Factor и FixedVertexState::Render(139,value),
либо defaults 0/1. Невозможны произвольные material/lighting float colors:
для них другие поля; mutable getter к factor/global ambient отсутствует.
Следующий измеряемый CPU расход — обратная упаковка этих квантованных каналов.
Аппаратных профилей r10–r16 нет, крупнейший bottleneck/FPS Switch не объявляется.

## Изменение и отклонённый вариант

Общий PackQuantizedArgb сохраняет float multiply v*255 и его FP flags.
На AArch64 SDK lroundf — FCVTAS x0,s0; RET (проверен actual r15 ELF).
Та же FCVTAS встроена через volatile asm, сохраняя исходные FP effects и
обязательное исполнение даже при отброшенном logical word result. Удаляются
только call/return и их overhead. На x64 округление делается целочисленно по
IEEE754 exponent/mantissa: для nonzero scaled ARGB8 values near 1..255 shift
16..24, zero обработан до shift. Helper имеет узкий private-color domain.
Никаких новых полей/layout/кеша, изменений setter/float uniform bytes,
COW/snapshot/restore, validation, shader/GPU/ownership/лимитов нет.

Первая попытка float +0.5 cast дала выигрыш CPU и совпала по цветам, но
изолированный host probe показал old=255 flags=0, fast=255 flags=32
(FE_INEXACT). Она **отклонена**. Header snapshot/decision, probe и результаты
первой серии сохранены в e610-argb-r16-checks. Windows comparison этого варианта
не воспроизвёл различие flags; это не отменяет host negative evidence.
Итоговая NRO использует inline FCVTAS на AArch64, x64 — integer-bit rounding. Отчёт не выдаёт timing первого
варианта за ускорение финального.

## CPU проверки

Отдельно компилируются оба полных frozen CPP из r15 source
c1d27c2684c7c273c419e28b9c79999d051af7aa и оба candidate CPP с одинаковыми
неизменёнными headers/layouts. 12437 cases: прежние 4245 validation/word/missing/
unsupported/alpha-target/stencil cases, 4096 полных ARGB8 channel cases при
FE_TONEAREST/DOWNWARD/UPWARD/TOWARDZERO, 64 граничных color/mode cases и 4032 pseudorandom ARGB. Независимый
oracle требует GetFactor==input и GetAmbient==input; дополнительно сравниваются
sticky FP flags getter после очистки, полные 4096 bytes пакета и категории/what().
Каждая платформа сравнивается со своей frozen; FP bits/std messages платформ
не смешиваются. COW/OOM/restore/frame/owner тест отдельно. Host ASan/UBSan,
ARM64/QEMU UBSan и Windows O2, assertions on. Большие бои не повторяются.

Timing — full production CPU PrepareDraw+vertex.Prepare+uniform_packet::Write,
100000 synthetic valid draws, host O2 ABBAAB без sanitizers/LTO/fast-math,
ffp-contract=off. Это не GPU execution, бой, Init или оценка FPS Switch.
Итоговые результаты приводятся ниже.

## Границы

HARDWARE PENDING: Init→transition→4 map frames и физические GPU/HID/audio/FPS.
Standalone entry не вызывает единую Campaign/Gov, естественную победу и возврат
Galaxy. R9 аппаратный Draw около 0.56 с не является измерением r16.
Upstream, frozen, ресурсы, saves, основная NRO и старые пакеты не меняются.
[Два ручных запуска](../docs/matrixgame-e610-argb-r16-readme.ru.md).

Дополнительные boundary тесты выявили ARM64 FP mismatch у универсального
integer-bit варианта (arm64/3 INCOMPLETE). Он отклонён для ARM; failure evidence
и header snapshot сохранены в rejected-arm-bit-round. Проверки не ослаблялись.
Финальные frozen/current runs уже включают все boundary colors и sticky flags.

## Итоговые CPU результаты

Окончательные runs: host/arm64 /4, Windows /5; ownership /3 на каждой платформе.
Все source bytes точно текущие. 12437 cases, 1822 expected rejections; полные
stdout совпали с frozen отдельно на каждой платформе, включая sticky FP flags.
Digest: host 2918462819154618968, ARM64 3447866941513478328,
Windows 18001663596211091568. Различающиеся FP bits/std messages не смешиваются.
Host ASan/UBSan/LSan O2, ARM64/QEMU UBSan O2, Windows O2 — PASS.
Native state: restores=9 capture_OOM=2 COW_OOM=1 restore_allocations=0;
frame/expired-owner/held resources guards retained. ARM64 type_info version
script сохраняет UBSan/vptr. Проверки не ослаблялись после negative evidence.

Final benchmark/3 без параллельной нагрузки: frozen 75758/75889/75483 мкс,
candidate 70095/67235/66886 мкс / 100000 draws. Медиана 75758 → 67235,
−11.3% времени CPU-подготовки. Checksum 4000058935132545121 совпал во всех runs.
Benchmark/1 относится к отклонённому float-cast; /2 — intermediate bit-round.
Их timing не выдаётся за результат финальной NRO. Изменений основной NRO,
ресурсов, saves, upstream/frozen и прежних пакетов нет.
Runner: tools/matrixgame_argb_checks.py; build/matrixgame/e610-argb-r16-checks.

## Проверенная доставка r16

Source commit: `2ff779a1b60c1b0623063e9846938129197d3b30`.
Build ID: `2ff779a1b60c-f09d63169b5bd7920a00`.

Свежие стандартные Quick Smoke/Interactive targets: build/matrixgame/e610-argb-r16/1.
DevkitA64 compile/link, отсутствие unresolved strong symbols, ELF/NRO build IDs,
segments, icon/NACP и встроенные fonts/shaders — PASS. В обеих actual NRO
два ARGB getters больше не вызывают lroundf: old r15 по 4 BL lroundf на getter,
new r16 по 4 FCVTAS и 0 BL lroundf. Old SDK lroundf проверен как 8 bytes
FCVTAS x0,s0; RET. Машинный код/адреса/размеры записаны в evidence. Это
реальное target instruction proof, а не измерение скорости GPU/консоли.

Compiler provenance: 321 configured TUs, RelWithDebInfo/O2/UNDEBUG, assertions/exceptions on, fast-math off.
Неизменённый prebuilt Rangers/UI archive не пересобирался и не объявляется
оптимизированным. Link maps: по 23 объекта deko3dd, без deko3d; SDK debug
SetContext/RaiseError retained. Generated originals совпадают с r15;
относительно E601 остаются те же три прежние overlay delta.

706 own source hashes совпали точно, как и все финальные CPU inputs.
248 прежних защищённых файлов, основная NRO, старые ZIP/NRO, ресурсы, saves,
frozen и upstream pins не изменены. [Машинное evidence](matrixgame-e610-argb-r16-evidence.json).

ZIP: `dist/matrixgame-e610-switch-test-r16-argb.zip`, 10163058 bytes.
SHA256: `8a4669bf9e88755d4cb060542132c3c0676c8eb5ed98dc208b11a3644d113e39`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `99611da6589ab006d459ebb9849c3d18e6bcee0d819e074be353620f3b09c46e` | `ca9adcd57414ff1f98e68530c00a7eed8ae4e605` |
| matrixgame-e610-interactive.nro | `4ca469bcab3d2eacfc1cfae5a381e081339ab9398ecc82e91ec4b14dbe0652ab` | `4ce0352a56478ca26136767f25b9266751e48561` |

В ZIP только две NRO, README.ru.md, manifest.json, SHA256SUMS.txt и build-info.json.
Ресурсов/сохранений нет. SD-папка `/switch/matrixgame-e610-r16-test/`; прежние
DATA/CFG roots остаются. В hbmenu **MatrixGame r16 Quick Smoke** /
**MatrixGame r16 Interactive**. Application Mode, сначала Quick, затем
Interactive; штатный выход Minus+Plus. Передать два новых каталога
`/switch/matrixgame-diag/logs/<run-id>/` и фото фактического результата.

**SOFTWARE PASS / HARDWARE PENDING.** Адресная оптимизация завершена;
дальнейшие изменения — по новому аппаратному профилю.
