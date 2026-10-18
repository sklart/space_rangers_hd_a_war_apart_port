# MatrixGame: ресурсный планировщик SRemindCore — 18.10.2026

Статус: оригинальный планировщик освобождения ресурсов включён в native core.
Это зависимость VO/Texture/MapGroup/MapStatic/PointLight, а не завершённый кэш,
GPU lifecycle, CGame::Init/Run или планетарный бой. Причина аппаратного freeze
после «Готово» остаётся ROOT CAUSE UNCONFIRMED.

## Исходники и изменения

matrixgame_reminder_overrides.py проверяет LF-normalized SHA pinned originals:

- CReminder.hpp: c02ad3ecad355dd1daa170faa384773cf924a13be27fb83d41855fd1f3a5b0c5
- CReminder.cpp: 12294822e2c4bbed8dcef7dbc648151b0244ef550addaef82a71137b349f1a22

Всего core preparation проверяет 25 inputs. CReminder.cpp действительно компилируется
в CPU library; full-engine preparation накладывает подготовленные header/implementation
на Base source view. Pinned submodules не меняются.

Сохранены intrusive list, round-robin обход, шаг CHECK_TIME=10, строгая проверка
gtime > deadline, отсрочка Use и остановка часов при пустом списке. Numeric time
переведён в int64; REMIND_HANDLER param остаётся uintptr_t, без кодирования адреса
в 32-битный ID. Изменение касается внутренних resource clocks, не файловых форматов.

Подтверждённый отдельным контрольным запуском дефект оригинала: callback вызывает
Down собственного core и возвращает false; Down уже перемещает global current
на соседний core, после чего исходный Takt вызывает current->Down и снимает соседа.
Native Takt запоминает callback owner и отслеживает его самостоятельное снятие.
После Down/delete не обращается к прежнему владельцу и не снимает следующего.
Повторная регистрация callback после собственного Down сохраняется.

Этот дефект воспроизведён на original x86, но не доказан как причина аппаратного
зависания или как достигнутый сценарий конкретного игрового callback.

Рекурсивный Takt и reset живого списка отвергаются. При исключении handler guard
восстанавливает состояние dispatch, исключение выходит вызывающему коду, cleanup
не объявляется успешным; живой core остаётся зарегистрированным для явного решения
вызывающего кода. Отрицательные интервалы, null handler и переполнение time отвергаются.
Копирование intrusive core запрещено. Это по-прежнему scheduler одного engine/render
thread: конкурентные Use/Down/Takt не поддерживаются и не объявляются thread-safe.

## Проверка семантики

Неизменённые CReminder.cpp/CReminder.hpp из pinned submodule собраны MSVC для x86.
Общий тест исполняет 5000 детерминированных Use/Down/Takt операций над 12 cores,
включая короткие и длинные периоды, обновления сроков и удаление неактивных cores.
Сравниваются callbacks, gtime/ctime/deadlines, current и порядок всего списка.

Original x86 и native MSVC совпали с независимым Python oracle:

- 5000 операций, 1419 cleanup callbacks;
- 792840 байт канонической трассы;
- SHA-256 8b5358278c1c8ed9b694ee077624e9507b95f97436decfe93a812104ebc82d46.

Оба отрицательных original controls (--unlink-self и --self-delete-false)
завершаются exit 3: соседний core ошибочно снят. Native controls проходят;
дополнительно проверяются self-delete с обоими результатами, rearm, исключение,
placement-new reuse того же адреса, рекурсивный tick, high 64-bit callback address
и переход через INT32_MAX.

Тест освобождения настоящего VectorGeometry использует контролируемый валидный STRG:
после Takt(30000) модель ещё жива, после Takt(1) callback освобождает её, core снят.
Это настоящие CStorage/VO allocations/destructors, но не GPU resources или игровая сцена.

```sh
cmake -S port/matrixgame/native -B build/mg-reminder-asan -DBRIDGE_SANITIZER=address -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/mg-reminder-asan --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir build/mg-reminder-asan --output-on-failure
bash tools/matrixgame_file_arm64.sh --reminder
```

MSVC и ASan/UBSan: 12/12 CTest PASS. ARM64/QEMU: PASS, тот же trace SHA.
SDK build native CPU/API targets PASS, nm --undefined-only для reminder ELF пусто.
Логи находятся в build/matrixgame/native-core. SDK ELF не является NRO и не запускается
на консоли этим этапом. Ни CPU trace, ни сборка не подтверждают Switch FPS/battery.

Локальный build/mg-engine-boundary-switch/core/matrixgame-native-reminder-link-probe.elf:
SHA-256 8966d698577bd4f32257a9dce6e19abe2323ad254556a1abc2cc75f8147d67a0;
GNU Build ID 3ec258ea3c737a012e98d0b326a91923360420bd.

Core source preparation теперь записывает только изменившиеся bytes. Два одинаковых
prepare подряд сохранили содержимое и mtime всех 29 generated files (включая manifest):
повторная конфигурация не заставляет пересобирать неизменённые CPU translation units.

## Остающаяся интеграция

Нужны actual CVectorObject/native cache/buffer lifecycle, texture/skin данные,
полный engine compile/link и Init/Run/campaign handoff. D3D9-зависимости полного
движка сохраняются. В просмотренной текстурной цепочке также обнаружен uint32_t id
в CBitmap::LoadFromPNG, принимающий uintptr_t из FilePNG::ReadStart_Buf; это отдельное
место потери адреса, которое ещё требует исправления и runtime-проверки при переносе PNG.
Изменения этого этапа не закрывают P0.1, MG-2…MG-5 и аппаратный прогон.
