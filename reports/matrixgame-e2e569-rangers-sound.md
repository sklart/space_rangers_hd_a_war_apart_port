# E2E-5.69 — реальные Rangers sound callbacks и audio host owner

Статус: **host real-resource sound PASS; ASan/UBSan новых TUs PASS;
SDK archive compile и existing-bridge link PASS**. Полный standalone host,
native Init/map и Switch не исполнены. Новая NRO в этом этапе не создавалась.

## Реализация

`MakeRangersHostCallbacks` строит полную `Robot::TRobotCallbacks` из 16
настоящих функций оригинального Robot.cpp. Mapping всех 16 fields/type/function
сверен с `InitializeRobotRuntime`. Factory не загружает Windows DLL и не вызывает
старый launcher; функции не заменены локальными no-op implementations.
Готовность их реальных host dependencies остаётся обязанностью вызывающего кода.

`RangersAudioSession` — владелец звуковой части standalone Rangers host:

- принимает **уже загруженный настоящий** TDataEC resource tree по borrowed reference;
- создаёт оригинальные TCacheEC и TSoundControl, публикует их в ожидаемых globals;
- отвергает занятые GlobalCache/SoundManager/MusicManager/CacheDataRoot,
  renderer cache eviction path и изначально отключённый sound;
- проверяет, что оригинальный backend не вернулся после отключения звука при
  неудачном Open: такой исход становится явной ошибкой, а не audio-ready;
- освобождает sound/buffers до cache, снимает borrowed root binding,
  восстанавливает сохранённые SoundEnabled/MusicEnabled; rollback действует
  также при отказе backend;
- требует сохранения ownership invariant и отсутствия живого MusicManager
  при teardown. Контекст должен жить дольше всех использующих его bridge/controllers.

Это audio-only owner: `GlobalsV::HardwareRenderingEnabled` относится к renderer
Rangers host и должен быть false. Он не является флагом отдельного MatrixGame
deko3d device. Для готового полного Rangers host с собственными globals этот
owner не нужен: можно использовать factory с уже существующими сервисами.

Стандартная библиотечная CMake target `matrixgame-engine-callbacks` теперь
компилирует оба новых source. Требования к настоящим Robot/GR_Main/GR_Sound
definitions сохраняются; они не заменены заглушками для закрытия linker.

## Что реально исполнено на host

Probe не запускает ProgramMain, GUI, gameplay и сохранения. Использованы
существующие original-game objects по проверенным hashes и текущие переносимые
audio adapters. В отдельном каталоге исполнена цепочка:

```text
original aPacket loose/package collection + lock
  → original TDataEC reader: CFG/CacheData.dat
  → Sound.FormShipClose lookup
  → new real callback producer
  → RangersBridge: original Robot sound callbacks
  → original TSoundBufferControl / TSoundControl / GlobalCache
  → original TCSoundEC WAV reader
  → existing SDL PCM adapter / SDL dummy output
  → completion, loop, volume-zero stop, cleanup
```

Обязательная package/loose-file инфраструктура настоящая. Data reader вызывается
через исходный штатный API; проверки ресурсов не отключались, ключи не
извлекались и защита не обходилась. Ресурсы только читаются.

Исполнены все 9 sound callbacks: PlaySound, Create/Free/Start/IsPlaying,
Set/GetVolume и Set/GetPan. Проверены exact getter values, one-shot completion,
оригинальный lazy loop start изменением volume, pan и остановка loop при volume=0,
запрет второго owner и освобождение globals. Это **Rangers host sound sample**,
не исполнение MatrixGame CSound, позиционного battle audio или полного Init.

Получен реальный PCM: mono, 44100 Hz, signed 16-bit, 3437 frames, 6874 bytes.
Независимый Python `wave` прочитал исходный WAV, полученный TDataEC, и подтвердил
побайтное совпадение с данными настоящего TCSoundEC:

`f8996df960a9f49649215b55281a82e6fb914d19ef285e4e2afd41ad81a73195`.

SDL driver `dummy` исполняет backend/mixer/callback path без физического вывода.
Слышимый звук и устройство этим не проверены. Второй процесс с несуществующим
SDL audio driver подтвердил **настоящий Open failure**, exception и rollback
globals/flags; успех не имитируется отключением sound.

## Проверки и пределы

| Проверка | Результат | Предел |
| --- | --- | --- |
| Host optimized | Все перечисленные реальные sound/resource cases PASS | SDL dummy, без GUI/Matrix Init/Switch |
| ASan/UBSan | Новые probe/owner/producer/bridge и два refreshed platform TU PASS; detect_leaks=1, halt_on_error=1 | 329 reused original TUs не инструментированы; это не full-stack ASan/UBSan |
| Независимый WAV/PCM | Exact bytes и параметры PASS в normal и sanitizer профилях | Один реальный sample, не весь sound catalog |
| SDK | Новые owner/producer compiled в archive; старый bridge probe linked, strong undefined = 0 | Новые host symbols не достижимы в этом probe; их полный SDK executable link/runtime не доказан |
| ARM64/QEMU sound | NOT RUN | Предыдущие ARM64 Init/callback-ownership gates не подменяются новым audio runtime proof |

Переиспользованы 329 original objects без повторной большой сборки. Provenance
проверка обнаружила два изменившихся platform source — quicksave transaction и
perf logger; они пересобраны **в новом каталоге**, исходные objects и executable
не изменены. Их присутствие в link не означает запуск save/gameplay.

Первые ошибки probe сохранены: отсутствовал package file lock, затем отсутствовал
loose-file provider для CFG. Backtrace указал TFileEC::AcquireReadHandle;
исправление — оригинальный aPacket bootstrap, не ослабление loader. Сохранены
первые result/log/executable, первое успешное evidence и финальное evidence.

Первое evidence предшествует добавлению sanitizer profile. Для актуального
состояния используется [final evidence](matrixgame-e2e569-rangers-sound-final-evidence.json).

## Воспроизведение и артефакты

Требуются сохранённый проверенный original-game host build, его headers,
установленные SDL2/zlib/png/jpeg/vorbis/OKGF и devkitPro SDK. Из корня checkout:

```powershell
python tools/matrixgame_rangers_sound_checks.py --reuse-objects
python tools/matrixgame_rangers_sound_checks.py --asan --reuse-objects
python tools/matrixgame_rangers_sound_sdk.py
python tools/matrixgame_rangers_sound_audit.py --output build/matrixgame/e2e569-rangers-sound/audit-repeat.json
```

При первом запуске профиля опустите `--reuse-objects`. Аудит создаёт только новый
файл; при повторении выбирайте другое имя. Старые original objects должны
совпасть по hashes: произвольный build не принимается вместо reference inputs.
WSL link staging записан в `link-report.json`, чтобы не выполнять случайные
NTFS seeks через большой набор objects. Старые большие suites не запускаются.

SDK archive: `build/matrixgame/e2e569-rangers-sound/sdk/libmatrixgame-engine-callbacks.a`

SHA256: `e85fba25c2e350de3e2388e557d188656b0854e27ce986e1a48448a86d5a62cf`.

SDK bridge probe: `build/matrixgame/e2e569-rangers-sound/sdk/matrixgame-engine-api-link-probe.elf`

SHA256: `e57c43663f032381622d3726cc05dc19b1af20dc5d1156f2c43e7e5c7181986e`.
Он не является NRO и не запускает новый audio host на Switch.

## Достигнутая цепочка и оставшиеся компоненты

Теперь существуют producer настоящих host callbacks и проверенный real-resource
audio/cache owner. **В standalone diagnostic не установлена частично готовая
таблица**: original text/music/progress/render-release callbacks имеют собственные
неинициализированные зависимости. Их ещё нужно подготовить, затем связать реальные
Rangers SDK units с native entry и пройти Init/PrepareMap.

Последний аппаратный Init остаётся ReadParams → ApplyGammaRamp до map constructor.
Последняя отдельная диагностическая NRO — E568, без изменений:
`build/matrixgame/e2e568-native-services/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`,
SHA256 `d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750`.
Новую NRO с объявленной полной host integration строить пока преждевременно;
archive compile и старый bridge ELF не доказывают её dependency closure.

Открыты real text/music/progress/render-release context, full SDK host link,
подключение к native session, native Init/map и аппаратные image/VRAM/HID/focus/
presentation/recovery/F9/audio gates. Доступ к Switch не использовался.

Оба upstream clean: MatrixGame `218115a72fa9c5d43269f4ca84a8596a0bcb82f1`,
Rangers CPP `57fa689c630193a66fdea6ca4c79a188814991cd`. Protected inputs,
CacheData.dat/Sound.pkg, старые host executable/report, frozen, игровые ресурсы,
сохранения и NRO проверены и сохранены. Commit/push нет. В отчёте E568 исправлена
неточная формулировка: RobotSound — булев флаг, не отдельный configuration object.
