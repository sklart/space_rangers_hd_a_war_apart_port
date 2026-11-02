# E2E-5.76 — реальное ARM64/QEMU исполнение hosted цепочки

Статус: **ARM64 compile/link/runtime PASS; four audio modes PASS; AFT pixel
comparison PASS; hosted native Init rollback PASS**. Это Linux AArch64/QEMU,
не Switch execution. Новая NRO не выпускалась; E575 сохранена.

## Cross-зависимости и сохранность окружения

Установленные ARM64 compiler/QEMU и прежние cross PNG/JPEG/zlib обнаружены и
использованы read-only. Готовых ARM64 SDL/Ogg/Vorbis не было. Они собраны в
`build/matrixgame/e2e576-rangers-arm64/deps/prefix`, без system install или
изменения devkitPro.

Фиксированные официальные исходники:

- [SDL2 2.32.10 release](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10),
  source archive SHA256
  `5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165`
  сверён с digest официального GitHub release asset.
- libogg 1.3.6 и libvorbis 1.3.7; SHA256 из
  [официального Xiph списка](https://xiph.org/downloads/) проверены до extraction.

SDL содержит настоящий dummy audio backend, pthread/timer и software/dummy video
components. Hardware audio/video backends в этом cross-test prefix отключены;
это не замена SDK SDL и не доказательство слышимого аудиовыхода. Source libraries
не менялись. Подготовлены static archives, headers и их hashes.

OKGF собран из pinned upstream в отдельной port-копии. Единственное изменение
его CMakeLists — SHARED → STATIC, чтобы линковать существующие cross image libs.
Все C sources совпали SHA256 с upstream; выбран NATIVE math, как на x86 host.
Первый recipe failure из-за отсутствующего generated jconfig.h исправлен добавлением
публичного build include path прежнего JPEG; logs/attempt result сохранены.

## Первая effective original ARM64 сборка

Для нового gate впервые скомпилированы все 331 effective original unit той же
Rangers host цепочки, семь native service/adapter units и current host-session
probe: **339 ARM64 objects**. Старые x86/SDK objects не использовались вместо
ARM64 исполнения. Две DAT TUs содержат тот же проверенный exception cleanup
overlay E573. Большие ранее пройденные x86/SDK suites не повторялись.

Compiler flags и effective source/header hashes сохранены. Per-object cache
проверяет source, flags, object bytes и все записанные headers; повторный запуск
не пересобирает неизменённый corpus. Link staging находится в отдельном Linux
`/tmp`, чтобы избежать random NTFS seeks. Target — настоящий Linux AArch64 ELF;
QEMU использует установленный cross glibc/libstdc++ sysroot.

## Исполнение

QEMU выполнил тот же probe и те же assertions, что x86 E574:

```text
HOST_SESSION effects=0 music=0 full16=PASS teardown=PASS
HOST_SESSION effects=0 music=1 full16=PASS teardown=PASS
HOST_SESSION effects=1 music=0 full16=PASS teardown=PASS
HOST_SESSION effects=1 music=1 full16=PASS teardown=PASS
HOST_SESSION modes=4 original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS SDL_DUMMY_ONLY NO_NATIVE_INIT NO_SWITCH
```

Работают настоящие original sound/AFT/music callbacks и data/cache owners.
Включённая музыка потреблена реальным SDL dummy callback до 1024 frames;
fully-muted режим не открывает backend, в том числе с invalid driver. Error
rollback/retry, duplicate guard и восстановление flags прошли.

Затем без повторной сборки корпуса выполнены два дополнительных probes:

1. Original AFT direct-reference/bridge comparison: 24 pairs, crop, automatic
   sizing, error cleanup. Размер 156×16 совпал. Все 1228800 bytes pixels.bin
   побайтно совпали с сохранённым x86 E570 результатом. SHA256:
   `2e133fbeebc054d999e24a67aaa09a56b7ae4da3f34124084f787fbe3fe73495`.
   Это тот же Font.2Normal и mixed ASCII/Russian fixture с align/Wrap flags,
   не полный GUI/multiline/tagged/all-font coverage.
2. Реальный early native CGame::Init с owning host и copied table: original
   progress consumer, real AFT text/free и looping sound, затем явный Unsupported
   Device::ReadCaps. Original rollback оставил host живым, sound был доступен
   после rollback, Deinit повторён, затем host очистил свои globals/resources.
   Два полных cycles прошли с actual ARM64 Matrix objects/archives.

```text
HOSTED_NATIVE original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS NO_GPU NO_FULL_INIT NO_SWITCH
```

Старый E568 fixture TU в joint link не входит: проверены reused production
sources/headers, а current real-host fixture скомпилирован отдельно. Различие
старого неиспользуемого test source не скрывалось и не было причиной изменять
runtime assertions.

## Проверки и пределы

ARM ASan/UBSan **NOT RUN**. Ранее выполненный x86 ASan — отдельное доказательство;
QEMU PASS не является hardware/audio/GPU PASS. Совместная diagnostic NRO E575
под libnx/deko3d здесь не исполнялась. Этот Linux ELF не является NRO и не
предназначен для переноса на Switch.

Аудит подтвердил source/header/object/library hashes, pinned clean cpp/matrixgame/
okgf, protected frozen/primary/cumulative artifacts и неизменность actual game
inputs. NRO E575 сохранена, SHA256
`9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356`.
System packages, SDK, upstream, ресурсы и сейвы не изменены. Commit/push нет.

## Повторение

Из PowerShell, с установленными WSL cross GCC/QEMU:

```powershell
wsl --exec python3 /mnt/d/repos/space_rangers_hd_a_war_apart_port/tools/matrixgame_rangers_arm64_deps.py
wsl --exec python3 /mnt/d/repos/space_rangers_hd_a_war_apart_port/tools/matrixgame_rangers_arm64_checks.py
wsl --exec python3 /mnt/d/repos/space_rangers_hd_a_war_apart_port/tools/matrixgame_rangers_arm64_extra.py
python tools/matrixgame_rangers_arm64_audit.py --output reports/matrixgame-e2e576-recheck.json
```

Deps recipe использует сохранённые source archives с SHA256, при первом запуске
скачивает их с официальных URL. Нужны прежние PNG/JPEG/zlib ARM libraries,
effective source map и native ARM archives E568; это инкрементальная проверка
текущего checkout, не bootstrap из пустой системы.

Outputs: `build/matrixgame/e2e576-rangers-arm64/`.
Evidence: `reports/matrixgame-e2e576-rangers-arm64-evidence.json`.

## Init-цепочка и оставшаяся работа

Новая hosted цепочка теперь имеет исполнение на x86-64 и AArch64, exact AFT
cross-architecture pixels и реальную early native Init rollback проверку на
двух архитектурах. Совместная E575 SDK/NRO остаётся готовым отдельным артефактом.

Windows reference execution новой hosted цепочки ещё открыт. Полный native
Init/map и первые map frames не доказаны. Аппаратная граница logs2 по-прежнему
ReadParams → ApplyGammaRamp, до map constructor; нужны будущие пользовательские
graphics/audio/HID/focus/F9/recovery проверки новой NRO. Switch агентом не
использовался.
