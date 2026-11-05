# MatrixGame E610 r36 — supplied RGBA mip rows и выравнивание BC

При расширении supplied RGBA уровня до степени двойки FillRGBA обрабатывал
каждый канал отдельно, включая bounds/clear выбор. Теперь valid width/height
вычисляются один раз, исходные row bytes копируются memcpy, остальная POT
граница заполняется прежними четырьмя clear_pixel bytes. Padding источника
не становится пикселями. Target — новый private result buffer PrepareMips,
поэтому source/output не перекрываются; source rows и размеры уже проверены.

Это только !box путь. Оригинальное тело BOX, его 1-D float rounding, gamma
LUT/application, размеры/число mips, ownership budget и move-after-success
остаются прежними. Нет новых allocations/cache/ABI, GPU ownership не меняется.
Source reference r35: 7ab2c995772cc338b1ffc86c9d79f9dc4469a895.
Реальный путь NRO: native_texture::Decode → TextureAsset::PrepareMips.
DecodeBitmap присутствует в исходнике, но удалён linker GC из этих двух NRO;
его entry не выдаётся за аппаратно проверяемый путь пакета.
Ресурсы и сохранения не редактируются.

## Выявленный baseline UB и его проверка

Первый новый corpus использовал BC source pitch=row+3. Host ASan/UBSan и
ARM64/QEMU UBSan остановились уже на frozen r35: DecodeBC1/DirectXMath
XMU565 (и ARM uint16_t) typed load с нечётного адреса. Candidate тогда ещё
не исполнялся. Windows O2 без sanitizer выполнил initial corpus, но это
не отменяет baseline UB. Результаты /1, stack traces и точные test/source/
runner bytes сохранены в build/matrixgame/e610-mip-copy-r36-checks/
before-alignment-fix. Ошибка не замаскирована отключением alignment sanitizer.

В DecodeBlock введён alignas(16) local block, в который memcpy копирует ровно
8 bytes BC1 либо 16 BC2/3 из уже проверенной строки. Настоящий pinned BC codec
получает эти bytes; его алгоритмы не заменяются. BC2/3 дополнительный BC1 RGB
compatibility decode из bytes+8 также сохранён. Новых heap allocations нет.

Для полного frozen/current сравнения BC control cases используют aligned
pitch=row+16: так frozen baseline исполняется без UB. Дополнительно candidate
выполняет исходное требование odd pitch: 864 cases BC1/2/3 × размеры × политики
× padding 1/3/7 × четыре rounding modes. Каждая odd row сравнивается с настоящим
aligned decode тех же активных block bytes, включая все output levels и FPU
flags. Сравниваются активные row bytes, а не разные исходные padding bytes.
Таким образом odd inputs не исключены из acceptance, а проверены отдельно.
Это локальный source defect, не доказательство причины прежних Switch сбоев.

## CPU проверки

Frozen — полный Git archive port/matrixgame r35, с собственными includes.
Оба варианта линкуют настоящий подготовленный BC.cpp из r35; SDK и CPU bytes
сверяются с текущим pinned prepare_bc adapter/BC.h и vendor hashes.
BC.cpp не правился. GCC O2/UNDEBUG, -ffp-contract=off, -frounding-math,
-fno-tree-sink; host ASan/UBSan/LSan, ARM64 UBSan/QEMU (vptr сохранён),
Windows O2 /fp:strict. Тесты не заменяют GPU вызовы: они исполняют CPU
PrepareMips напрямую на созданных в памяти входах. GPU здесь не требуется.

На каждой платформе: 1928 frozen/current cases, 92 одинаковых отказа,
4265600 RGBA oracle channels. Все output bytes и FPU flags входят в digest;
RGBA supplied L0 проверяется независимой попиксельной моделью clear/copy/gamma.
Четыре rounding modes, 1-D/2-D BOX, padding, POT/non-POT, real BC1/2/3,
неверные размеры/format/count/pitch/short rows и budgets. На каждом отказе
исходные metadata/bytes/data pointer остаются прежними. Дополнительные
864 odd-BC cases — PASS. Полные frozen/current stdout совпадают попарно.

| Платформа | Основной digest | Odd BC digest |
| --- | ---: | ---: |
| host | 17810332396854180323 | 8182386970932145308 |
| ARM64/QEMU | 9269151499617152931 | 1105666068241857020 |
| Windows | 10738186078716421795 | 16344224928648219632 |

Воспроизведение: `python tools/matrixgame_mip_copy_checks.py host`
(также arm64/windows/benchmark). Runner создаёт новые numbered directories.
Final semantic outputs — host/2, arm64/2, windows/2. Большие Windows battles
и полные regression suites не перезапускались.

## Отклонённый uniform-copy эксперимент

Исследован FixedVertexState::Prepare: вместо contiguous copy 1328 bytes
инициализировались только сохраняемые члены. Host unlit median 34185 → 50805
мкс (+48,62%); восемь lights 22288 → 24384 (+9,41%). NOT SELECTED / NOT SHIPPED;
semantic gates для этого эксперимента не запускались. CPP/runner/benchmark/
подготовленный test сохранены в build/matrixgame/e610-vertex-uniform-r36-checks/
not-selected; result benchmark/1. FixedVertex CPP восстановлен byte-exact r35.

## CPU benchmark

128 реальных PrepareMips OneLevel: RGBA 1023×767 → 1024×1024 с clear border.
Копирование input fixture выполняется вне measured interval; actual output
allocation, conversion и освобождение прежнего storage внутри PrepareMips
входят в замер. Control: 64 настоящие BOX chains из supplied POT 512×256.
Это CPU cached memory, не Switch uncached memory/GPU/полный Init.
O2, assertions включены, без sanitizer/LTO/fast-math; ABBAAB, warmup.

Initial /1 до alignment fix: copy median 832780 → 41236 мкс (20,19×),
BOX 30271 → 27928 (быстрее). Все checksums равны 10909591624186716767.
Окончательный /2 выполнен после обоих изменений и semantic gates.


| Итоговый /2, мкс | Frozen r35 | R36 |
| --- | ---: | ---: |
| 128 RGBA expansions | 837446 | 41008 |
| 64 BOX chains | 30698 | 29942 |

RGBA expansion: 20.42× / −95.10%.
Все 12 benchmark checksums /1+/2 одинаковы; исходные результаты сохранены.
BOX контроль не замедлился. BC alignment fix не выдаётся за ускорение BC:
его отдельного performance коэффициента нет, введено 16 bytes stack staging.
CPU runner применяет строгие BC flags ко всем тестовым TUs; SDK сохраняет
свой прежний per-file profile. Коэффициент CPU не переносится на SDK/железо.
Частота non-POT supplied RGBA и его доля в настоящем Init неизвестны.

## SDK/package

Source commit: `c32f35b8bc4fc8977095a22c34dbf2813f9c14a0`.
Build ID: `c32f35b8bc4f-e04e4c568b7ff2f0ca0d`.
Стандартная сборка: `python tools/matrixgame_current_diagnostic_sdk.py
--output-root build/matrixgame/e610-mip-copy-r36`; текущий output /1.
Две стандартные libnx/deko3d devkitA64 compile/link — PASS; strong undefined=0.
321 configured TUs: O2/UNDEBUG, exceptions, no fast-math. Link maps подтверждают
23 debug deko3dd object members / 0 release, mixed=false. Prebuilt Rangers
archives unchanged, новые оптимизации к ним не приписываются.
NRO structure/assets, fonts/shaders, NACP titles и ELF→NRO IDs проверены.
Generated originals exact r35; три прежних changes vs E601 сохранены.
727 own source hashes verified. Production delta — только texture_mips.cpp:
RGBA non-box row copy и aligned BC input. Дополнительно r36 titles/runner.
388 защищённых прежних артефактов SHA unchanged; game/frozen/main NRO gate PASS.
Старые packages/evidence не перезаписывались.

Actual SDK PrepareMips: 5388 → 5552 bytes, новый row memcpy call присутствует.
DecodeBlock: 136 → 156 bytes. Memcpy пишет stack+0x30 (16-byte alignment),
после чего этот же адрес подаётся настоящему BC1/2/3 decoder. Дополнительный
BC1 RGB decode для BC2/3 использует stack+0x38. Все codec calls сохранены.
Подтверждён BL PrepareMips из actual native_texture::Decode(std::shared_ptr,...).
DecodeBitmap отсутствует в обеих final ELF после GC, поэтому этот entry пакетом
не охвачен. Это отдельная граница проверки, а не недостающая заглушка.
Полные disassembly и static call chain сохранены в evidence.
RenderLookup 630 bytes, LogicalRules 103, SamplerLookup 28, Matrix r25 и
Resource::Initialized r34 machine bytes exact r35. Неизменённые исходники,
включая FixedVertexState::Prepare и NativeRenderState::PrepareDraw, hash verified.
Test/benchmark TUs не входят в SDK compile_commands. Это compile/link/static
proof, не аппаратный запуск и не приёмка физических GPU/HID/audio.

ZIP: `dist\matrixgame-e610-switch-test-r36-mip-copy.zip` (10160723 bytes).
SHA256: `c74dc80a372a109996f45d1d820f32e16f546ba770cc9d8ec5d660c37c85a192`.
Ровно 6 файлов: две NRO, README.ru.md, manifest.json, SHA256SUMS.txt, build-info.json.
Ресурсов и сохранений нет; ZIP и распакованные файлы сверены по SHA.

| NRO | SHA256 | ELF Build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `30b8d7b3a16ed60d5a4b8a965973fd9687e4bd5973c30e05eeac043853f658d9` | `6a4e7bd531eb8d90562c10556b8d098fda4d0bb7` |
| matrixgame-e610-interactive.nro | `d341f66a4a78fbcc57dccff91fcc1bbe5a8fcc4f94ff08e1e1695cbce93d57a5` | `92d7a760f5c12a6edb03aa25236e5418325a2bde` |

Package command: `python tools/matrixgame_e610_freeze.py package
--build build/matrixgame/e610-mip-copy-r36/1
--source-commit c32f35b8bc4fc8977095a22c34dbf2813f9c14a0
--output dist/matrixgame-e610-switch-test-r36-mip-copy.zip
--readme docs/matrixgame-e610-mip-copy-r36-readme.ru.md
--rangers-root sdmc:/switch/space-rangers-hd-a-war-apart/game`.
При повторении использовать новое имя вывода. Delivery gate:
`python build/matrixgame/e610-mip-copy-r36-checks/verify_delivery.py
c32f35b8bc4fc8977095a22c34dbf2813f9c14a0`.
[Полный evidence](matrixgame-e610-mip-copy-r36-evidence.json).
Evidence SHA256: `0041ccb274f79b65d30094592e794ea9c915f7d67c7998c722570225bef0eac6`.

SOFTWARE PASS / HARDWARE PENDING. Разработка на этом шаге остановлена.
Два ручных запуска: Quick Smoke до четырёх настоящих map frames, затем Interactive
до 120 секунд после Init; выход Minus+Plus. При раннем FAIL не пропускать этап.
NRO копировать отдельно в /switch/matrixgame-e610-r36-test/; существующие ресурсы
повторно копировать не требуется. Логи: /switch/matrixgame-diag/logs/<run-id>.
[Полная инструкция](../docs/matrixgame-e610-mip-copy-r36-readme.ru.md).
Последние реальные timings относятся к r9; вклад r36 в Draw/Init/FPS неизвестен.
Unified Campaign/Gov, естественная победа и возврат в Galaxy standalone entry
не охвачены. Переноса файлов и аппаратного запуска не выполнялось.
