# E2E-5.77 — Windows reference реальных hosted сервисов

Статус: **Windows x64 compile/link/runtime PASS; four audio modes PASS;
AFT pixels exact PASS; native sync ABI and clock execution PASS**.
Windows native MatrixGame `CGame::Init` в этом этапе не запускался. Полный
Init/map и аппаратный запуск остаются открытыми. Commit/push не выполнялись.

Evidence: `reports/matrixgame-e2e577-rangers-windows-evidence.json`.
Все команды, effective source/header/object hashes и stdout сохранены в
`build/matrixgame/e2e577-rangers-windows/` и в evidence. Это самостоятельный
MSVC LLP64 reference, без подмены Windows исполнением Linux/WSL.

## Сборка и исполнение

MSVC 19.44.35229, x64, C++20, `/O2 /MT /UNDEBUG`; assertions включены.
Скомпилированы 331 effective original/platform unit, семь текущих native
service/adapter units и текущий host-session probe: **339 objects**.
Использованы исходные algorithms и реальные данные установки Rangers.

Из уже проверенных официальных SDL2 2.32.10/Ogg 1.3.6/Vorbis 1.3.7 sources
собраны собственные Windows static libraries в private workspace prefix.
SDL использует настоящий dummy audio backend. Старые Windows PNG/JPEG/zlib
использованы read-only; CRT согласован на `/MT`. Системных установок не было.
OKGF использует NATIVE math, как прежний host. Это не проверка полной
численной эквивалентности x87 или всей игровой симуляции.

Тот же реальный host-session probe выполнил:

- sound/music = 00, 01, 10, 11, full16 callbacks и полный teardown;
- original fully-muted controller без открытия SDL device;
- настоящее потребление music frames с original workers;
- ошибку отсутствующего audio driver, rollback, retry и восстановление flags;
- реальные original AFT/text и sound services через owning host.

Отдельный AFT probe переиспользовал весь корпус объектов и пересобрал только
entry point. 24 пары с прямым original renderer, crop, automatic dimensions и
error cleanup PASS. Размер auto-raster **156×16**. Все **1228800 bytes** совпали
с Linux E570 (и ранее ARM64 E576):
`2e133fbeebc054d999e24a67aaa09a56b7ae4da3f34124084f787fbe3fe73495`.

## Найденные и исправленные portability-дефекты

1. MSVC ищет quoted includes также по активному include stack. Частичный runtime
   overlay подключал одновременно original и generated `windows.hpp`.
   Добавлен явный `portable_windows_host=True`: полный private runtime/header
   tree и portable real handle/heap resolver для reference. Обычный режим
   генератора сохранил прежние Linux/SDK bytes по всем пяти overlay files.
2. Исходный Win32 `CriticalSection` хранит 24 bytes native section. Windows x64
   API записывает 40 bytes. CDB воспроизвёл heap overwrite при освобождении
   sound lock. Generated private Windows header теперь выделяет 40 bytes с
   alignment 8 для x64; 32-bit storage остаётся 24 bytes. Отдельные TUs сверили
   размер/выравнивание с настоящим типом установленного Windows SDK. Четыре
   цикла native recursive/contended locking и cleanup прошли исполнение.
3. Original runtime запрашивает locale до main. Windows reference resolver
   отклонял GetLocaleInfoA как optional-disabled. Windows-ветка теперь напрямую
   импортирует настоящие GetLocaleInfoA/GetThreadLocale. Linux/SDK ветки прежние.
4. Windows SDK 10.0.26100 `dxdiag.h` объявляет Initialize с одним параметром после
   this, но его convenience macro ошибочно требует два. Только private OKGF
   copy вызывает ту же реальную COM-функцию напрямую через vtable. Upstream и
   SDK headers не изменялись; операция не отключена.
5. Эффективный GR_Main содержит GNU noinline attribute. Private MSVC source
   copy сохраняет эту границу через `__declspec(noinline)`.
6. Четыре signed ship Id в unsigned диагностических полях получили явные
   uint32 conversions. Сохраняется прежний 32-bit результат; save format,
   serializer и файлы сохранений не менялись.
7. Диагностика времени использовала недоступный MSVC `clock_gettime`.
   Windows использует настоящие system_clock и GetThreadTimes. Исполненный
   probe подтвердил рост thread CPU под нагрузкой, epoch anchors внутри
   интервала процесса и writer shutdown/join.
8. File clock conversion имеет отдельную MSVC clock_cast ветку. AFT probe
   передаёт mutable char16_t path в соответствующий original API вместо const
   literal; assertions и входной текст сохранены.

Начальные CMake checks MSVC падали C1902 при `/Zi`. Release reference dependency
checks явно используют Release, без PDB. Для C11 atomic операций OKGF включён
реальный MSVC `/experimental:c11atomics`. Failed configure/build logs и attempt
results сохранены. Системный Visual Studio не ремонтировался и не менялся.

## Ограниченные проверки после изменений

Большие ранее пройденные host/ARM64/SDK suites не повторялись. Windows был новым
gate; после выявленных ошибок повторно компилировались необходимые units с
проверкой source/header/flags/object cache. Изменение native lock ABI потребовало
согласованной пересборки всех его Windows consumers.

После последних portable изменений отдельно скомпилированы e2e515_perf,
win32_compat, win32_compat_files и effective aGalaxy (consumer двух diagnostic
headers), по четырём units в трёх профилях: host ASan/UBSan, ARM64 GCC и
devkitA64 — **12 compile PASS**. Небольшой реальный clock/writer probe выполнен
на host с ASan/UBSan и leak detection — PASS. Это не полная instrumented engine
проверка, не новое ARM64 исполнение, не SDK relink и не новый запуск NRO.

Первый запуск delta recipe в обычной песочнице вернул Wsl/E_ACCESSDENIED.
Авторизованный повтор через доступный escalated runner прошёл; среда WSL и её
настройки не менялись. Logs первого запуска сохранены.

## Сохранность и воспроизведение

Аудит заново сверил 78 игровых входов, доступные frozen fixtures/artifacts и
защищённые saves по прежним hashes. Primary/cumulative NRO, clean pinned
matrixgame/cpp/okgf upstream и пользовательский `D:/logs2.7z` сохранены.
Другие dirty/untracked изменения checkout не удалялись и не сбрасывались.

Последняя отдельная diagnostic NRO E575 не пересобиралась:
`build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`
— **14891205 bytes**, SHA256
`9dcd90d5175bc0ae7cbfe6db75b180b1fca406b01655880cf64d6f4f31cab356`.
Это сохранённый SDK build E575; новые Windows-only generation/test inputs не
выдаются за его исходный build fingerprint. Основная NRO не изменялась.

Последовательность Windows проверки из корня проекта, с установленными Python
и Visual Studio C++:

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_rangers_windows_deps.py
python tools/matrixgame_rangers_windows_checks.py
python tools/matrixgame_windows_sync_checks.py
python tools/matrixgame_windows_perf_checks.py
python tools/matrixgame_rangers_windows_text.py
python tools/matrixgame_windows_portability_delta.py
python tools/matrixgame_rangers_windows_audit.py --output reports/matrixgame-e2e577-rangers-windows-recheck.json
```

Dependency recipe использует сохранённые source archives E576 с проверкой
SHA256. Delta требует установленный WSL GCC/ARM64 toolchain и devkitA64.
Audit создаёт новый файл exclusively; существующий evidence не перезаписывает.
Source copies и outputs находятся только в собственном build subtree. MSVC
environment захватывается в памяти; полный environment в logs не выводится.

## Точная Init-цепочка и оставшиеся компоненты/gates

| Часть цепочки | Состояние доказательств |
| --- | --- |
| Output folder/minimap, original CFile, ownership/private output | E527 software gates завершены; без нового большого rerun |
| Original CDeviceState / StoredStateBackend → DekoDevice | E529 software gates завершены; scalar state не доказывает весь Init |
| Data/packages/config/language/mod/CaseConv | Реальные owning services; host/ARM64/Windows execution |
| Cache/sound/AFT/music/full16/callback lifetime | Host и ARM64 E574/E576; Windows hosted services E577 PASS |
| Original native CGame::Init, early unsupported-caps rollback | Host и ARM64 два цикла PASS; Windows native prefix ещё NOT RUN |
| Joint native/deko + owning Rangers entry | E575 SDK link/NRO/package/audit PASS, combined NRO NOT RUN |
| Последняя реальная аппаратная граница | ReadParams → ApplyGammaRamp, до map constructor; не продвинута этим этапом |
| Полный Init, карта и первые четыре frames | PARTIAL / NOT RUN |

Следующий программный gate — hosted native Init/rollback prefix под Windows
с original MatrixGame objects, затем дальнейшая CPU интеграция по реальным
зависимостям Init. Полный проход map ctor/ресурсов/render path остаётся
неподтверждённым; конкретный новый exception после последней аппаратной границы
этим этапом не измерен. Нельзя объявлять отсутствующий exception исправленным
или считать сам build доказательством готовности всех map operations.

Для пользователя сохранены [инструкция отдельной NRO](../docs/matrixgame-e2e575-hosted-diagnostic.md)
и read-only ресурсный manifest E575. Её hardware execution, graphics/readback,
слышимый audio output, HID/focus/F9/recovery выполняются вручную. Агент со Switch
не взаимодействовал. No GPU emulation, full Init success или hardware PASS
не заявляются; обязательные операции заглушками не заменялись.
