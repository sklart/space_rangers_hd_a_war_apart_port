# Space Rangers HD: A War Apart — Nintendo Switch

Неофициальный исходный homebrew-порт **Space Rangers HD: A War Apart** для Nintendo Switch (ARM64, libnx, SDL2). Это проект переноса технических подсистем игры, а не готовый игровой релиз.

> **Текущий аппаратный статус:** M14P–M22 проверены на Switch. M23 portable AFT/text/Label — SOFTWARE COMPLETE / HARDWARE PENDING. M24 GraphButton/Window/Zone проходит программную валидацию; Switch пока не запускался. Это не означает готовность игры к прохождению: полный UI, audio/music и gameplay ещё не подключены.

## Что уже работает

| Область | Состояние |
| --- | --- |
| Сборка ARM64/NRO | PASS |
| CI `package-host` | PASS |
| Иконка, NACP и метаданные NRO | PASS — имя, JPEG в ASET |
| Package filesystem и `common.pkg` | host PASS, ARM64 build PASS |
| Переведённый `EC_File` | host PASS, ARM64 build PASS |
| Загрузка INSTALL.TXT, language/mod packages | host/CI PASS, ARM64 build PASS |
| CPU RGB565 / SDL presentation | host/CI PASS, ARM64 build PASS |
| DAT/runtime configuration, GlobalCache и M12 persistent loop | host/CI PASS, ARM64 build PASS, hardware PASS |
| M13 bitmap metadata diagnostic | host/CI PASS, ARM64 build PASS, hardware PASS (17/17) |
| M14P portable GI Format-0 CPU decode | host/CI/ARM64/hardware PASS; M14 COMPLETE |
| M15 portable GAI container + one Format-0 frame | release baseline + CI + ARM64 build + Switch hardware PASS; M15 COMPLETE |
| M16 portable GAI Format-2 decode | host/CI/ARM64/Switch hardware PASS; M16 COMPLETE |
| M17 portable GAI sequence playback | host/CI/ARM64/Switch hardware PASS; one `Flags == 0` cycle matches oracle; M17 COMPLETE |
| M18 portable software compositor | host/CI/ARM64/Switch hardware PASS; M18 COMPLETE |
| M19 portable scene compositor | host/CI/ARM64/Switch hardware/Python oracle PASS; M19 COMPLETE |
| M20 portable GI object layer | host/CI/ARM64/Switch hardware PASS; M20 COMPLETE |
| M21 portable UI image foundation | host/CI/ARM64/Switch hardware PASS; real Simple oracle matches; Trans/Alpha release baselines are genuinely NOT PRESENT; M21 COMPLETE |
| M22 portable UI object/layout foundation | host mixed-tree/Python oracle, CI `37783935697`, clean ARM64, symbol audit и Switch hardware PASS; M22 COMPLETE |
| M23 portable AFT/text/Label foundation | host/Python oracle PASS, CI `37803515979` PASS, clean ARM64/symbol audit PASS; RC NRO зафиксирован, hardware PENDING |
| M24 portable GraphButton/Window/Zone foundation | host/Python oracle и локальный symbol audit PASS; terminal CI и clean ARM64 ожидаются; hardware PENDING |
| Полноценный игровой UI, audio/music, EC_Cache и gameplay | не подключены |

Аппаратные проверки — отдельный gate: результаты host, CI и кросс-сборки не считаются доказательством работоспособности на консоли.

## Baseline и границы проекта

Эталон — установленный Steam/GOG build от **2025-10-13**, версия игры `2.1.2500`:

```text
Rangers.exe SHA-256
83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98
```

В репозиторий не входят игровые ресурсы, оригинальные бинарные файлы и коммерческий контент. Для запуска нужна собственная легально установленная копия игры. Не публикуйте эти файлы вместе с NRO или в issues.

## Получение исходников

Подмодули обязательны:

```powershell
git clone --recursive https://github.com/sklart/space_rangers_hd_a_war_apart_port.git
cd space_rangers_hd_a_war_apart_port
```

Если checkout уже существует:

```powershell
git submodule update --init --recursive
```

## Требования к сборке

- devkitPro MSYS2;
- devkitA64 и libnx;
- Switch portlibs: SDL2, JPEG, PNG и zlib;
- CMake и Python 3;
- Git.

Linux host-регрессии выполняет GitHub Actions `ubuntu-24.04`; локальный WSL для этого проекта не является validation environment. Windows working copy допустима как источник WIP, а ARM64-сборку нужно выполнять в отдельном clean clone через devkitPro MSYS.

Сборку Switch ARM64 нужно запускать из оболочки devkitPro MSYS, а не из обычного `cmd.exe`.

```bash
cd port/switch
make clean
make
```

Результат ровно один:

```text
port/switch/Space Rangers HD - A War Apart.nro
```

`make` перед созданием NRO проверяет `assets/icon.jpg`: это должен быть RGB JPEG размером ровно 256×256 и не больше 128 KiB. Метаданные NRO: имя `Space Rangers HD: A War Apart`, автор `sklart`, версия `0.0.1`.

## Структура SD-карты

```text
<SD>/switch/space-rangers-hd-a-war-apart/
  Space Rangers HD - A War Apart.nro
  game/                    # ваша полная установленная копия игры
    Rangers.exe
    DATA/
    CFG/
    ...
  config/                  # создаётся портом
  save/                    # создаётся портом
  logs/
    port.log               # диагностический лог порта
    port-prev.log          # предыдущий диагностический запуск
    gr-main.log            # отдельный лог GR_Main
  runtime/
    deployment.txt
```

Для первого теста копируйте установленную игру целиком в `game/`; не заменяйте и не удаляйте отдельные папки выборочно. При последующих обновлениях заменяйте только NRO, сохраняя `game/`, `config/`, `save/` и `logs/`.

## Безопасное развёртывание

`tools/deploy-switch.ps1` проверяет baseline по хэшу `Rangers.exe`, не использует `/MIR`, `/PURGE` или `/MOVE` и не трогает дерево игры при режиме обновления.

Первое копирование на SD-карту:

```powershell
.\tools\deploy-switch.ps1 `
  -SdRoot 'E:\' `
  -NroPath '.\port\switch\Space Rangers HD - A War Apart.nro' `
  -GameSource 'D:\Games\Space Rangers HD A War Apart' `
  -InitialGameCopy
```

Следующая сборка:

```powershell
.\tools\deploy-switch.ps1 `
  -SdRoot 'E:\' `
  -NroPath '.\port\switch\Space Rangers HD - A War Apart.nro' `
  -UpdateOnly
```

Сбор журналов без изменения SD-карты:

```powershell
.\tools\deploy-switch.ps1 -SdRoot 'E:\' -CollectLogs
```

При подключении по DBI/MTP/Wi-Fi скопируйте NRO и каталог `game/` в ту же структуру вручную. После запуска сохраняйте `port.log` и `gr-main.log` до следующего запуска: они нужны для аппаратной диагностики.

## Границы подтверждённого аппаратного пути

M7, M8, M9, M11, M12, M14P–M22 имеют hardware PASS. M13 завершён как metadata diagnostic и не требует отдельного hardware gate. M18 CPU-only композитит единственный decoded Format-2 BGRA кадр поверх M12 heartbeat в RGB565 framebuffer; screenshot подтвердил реальную видимость кадра. M14 direct upstream path остаётся заблокированным широким `GR_DX` fan-out, а M14R — Direct3D COM state в layout `TGraphBufGR`.

Последний аппаратно протестированный NRO (M21) имел размер 7 547 184 bytes, SHA-256 `6D387610DAD7DA57C1D9F21F00ABF84C67F9AA266421CF54C98567656C8FA3F1` и embedded `build_git=fc8adfc`. Он проверил real Simple `DATA/Planet/Spu00.png` (128×60), корректно зафиксировал отсутствие release Trans/Alpha без подстановки ресурсов и сопоставил integrated RGB565 scene с независимым oracle: `CRC32=4f915772`, `FNV64=52449ae8f8f56c6c`. M12 представил 3 824 кадра за 172 747 ms, вышел через `PLUS` и достиг `[BOOT] COMPLETE`; M17 также завершил полный цикл. CI `37770073920` успешно прошёл M21 regression и symbol audit. M21 COMPLETE.

```text
[M9] FAIL runtime config=... DAT zlib decompression failed
```

Это диагностический отказ, а не успешный игровой старт. Если экран перестал отвечать либо Home не открывает системное меню, удерживайте кнопку питания для безопасного перезапуска и приложите оба лога к отчёту.

## Проверки

GitHub Actions workflow `package-host` выполняет asset-free host-регрессии, включая corrupt package, циклы каталогов, `ZL02`, portable `EC_File`, synthetic DAT/runtime settings, M8 golden, M12, M13, M14P Format-0, M15 GAI container, M16 Format-2, M17 pure playback, M18 software compositor и M19 scene compositor. M18/M19 symbol audit запрещает графические/upstream GAI runtime типы в чистых CPU-композиторных binary.

M20 добавляет `host-gi-object-test` и отдельный symbol audit: он покрывает resource → GIObject → animation → Scene → RGB565 framebuffer и запрещает возврат к UI/Direct3D/`TGraphBufGR`-пути в новом object layer.

Локальная ARM64-сборка проверяет создание NRO. Дополнительно для финального артефакта следует подтвердить:

- `ELF64 AArch64` у `build/SpaceRangersHDAWarApart.elf`;
- наличие NRO;
- наличие ASET и совпадение embedded JPEG с `assets/icon.jpg`;
- SHA-256 перед копированием на SD-карту.

## Документация

- [Текущий статус и границы milestone](docs/current-status.md)
- [Сборка Switch](docs/build-switch.md)
- [Развёртывание на SD-карту](docs/switch-deployment.md)
- [Milestone 9: runtime configuration](docs/milestone9-runtime-config.md)
- [Milestone 10: build и hardware baseline](docs/milestone10-build.md)
- [Milestone 11: GlobalCache diagnostic slice](docs/milestone11-global-cache.md)
- [Цепочка зависимостей startup](docs/startup-dependency-chain.md)
- [M14 portability boundary](docs/milestone14-gi-portability.md)
- [M14P portable GI Format-0](docs/milestone14p-gi-format0.md)
- [M15 portable GAI CPU](docs/milestone15-gai-cpu.md)
- [M16 portable GI Format-2 CPU](docs/milestone16-gi-format2.md)
- [M17 portable GAI playback](docs/milestone17-gai-playback.md)
- [M18 portable software compositor](docs/milestone18-software-compositor.md)
- [M19 portable scene compositor](docs/milestone19-scene-compositor.md)
- [M20 portable GI object layer](docs/milestone20-gi-object-layer.md)
- [M21 portable UI image foundation](docs/milestone21-ui-image-foundation.md) — COMPLETE
- [M22 portable UI object/layout foundation](docs/milestone22-ui-object-layout.md) — COMPLETE
- [M23 portable AFT/text/Label foundation](docs/milestone23-font-label.md) — SOFTWARE COMPLETE / HARDWARE PENDING
- [M24 portable GraphButton/Window/Zone foundation](docs/milestone24-ui-controls.md) — software validation in progress / HARDWARE PENDING

## Лицензирование и обратная связь

Код порта распространяется в рамках лицензий данного репозитория и его подмодулей. Торговые марки, игровой контент и оригинальные assets принадлежат соответствующим правообладателям.

В отчёте об ошибке укажите commit, SHA-256 NRO, способ развёртывания, структуру папки на SD (без игровых файлов) и оба лога. Не прикладывайте коммерческие assets.
