# Space Rangers HD: A War Apart — Nintendo Switch

Неофициальный исходный homebrew-порт **Space Rangers HD: A War Apart** для Nintendo Switch (ARM64, libnx, SDL2). Это проект переноса технических подсистем игры, а не готовый игровой релиз.

> **Текущий аппаратный статус:** M14P, M15, M16 и M17 проверены на Switch. M17 portable CPU-only воспроизводит embedded sequence 0 из `DATA/Asteroid/00.gai`, декодируя выбранные Format-2 кадры без Direct3D и upstream GAI runtime; первый полный цикл совпал с независимым oracle. Это не означает готовность игры к прохождению: UI, audio/music, `EC_Cache` и gameplay ещё не подключены.

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

M7, M8, M9, M11, M12, M14P, M15, M16 и M17 имеют hardware PASS. M13 завершён как metadata diagnostic и не требует отдельного hardware gate. M14 direct upstream path остаётся заблокированным широким `GR_DX` fan-out, а M14R — Direct3D COM state в layout `TGraphBufGR`; M14P решает Format-0 без этих типов. M17 подтверждает CPU-only playback sequence 0 из `DATA/Asteroid/00.gai`: один полный цикл совпал с независимым oracle, после чего M12 продолжил работу до `PLUS` и чистого shutdown.

Последний аппаратно протестированный NRO (M17) имел размер 7 194 928 bytes, SHA-256 `E753BFEAA6BF71C48086C98BB8A27307A0929ECB05B362D343CFA7B8777DCD0D` и embedded `build_git=13ad513`. `port.log` подтвердил sequence CRC32/FNV `4b1c6ebf`/`47cdc8c73fc1ce61`, cycle CRC32/FNV `5b7bc7e9`/`f70813ac799a25b3`, первый цикл 5033 ms при допуске 134 ms, затем `PLUS` и `[BOOT] COMPLETE`. Deploy preflight/manifest подтверждает SHA-256.

```text
[M9] FAIL runtime config=... DAT zlib decompression failed
```

Это диагностический отказ, а не успешный игровой старт. Если экран перестал отвечать либо Home не открывает системное меню, удерживайте кнопку питания для безопасного перезапуска и приложите оба лога к отчёту.

## Проверки

GitHub Actions workflow `package-host` выполняет asset-free host-регрессии, включая corrupt package, циклы каталогов, `ZL02`, portable `EC_File`, synthetic DAT/runtime settings, M8 golden, M12, M13, M14P Format-0, M15 GAI container, M16 Format-2 и M17 pure playback. M17 sequence/playback checks запрещают графические, upstream GAI runtime и sound-loop symbols.

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

## Лицензирование и обратная связь

Код порта распространяется в рамках лицензий данного репозитория и его подмодулей. Торговые марки, игровой контент и оригинальные assets принадлежат соответствующим правообладателям.

В отчёте об ошибке укажите commit, SHA-256 NRO, способ развёртывания, структуру папки на SD (без игровых файлов) и оба лога. Не прикладывайте коммерческие assets.
