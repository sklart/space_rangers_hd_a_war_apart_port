# E2E-5.92: полный native RobotInterface и исходный Finalize

**PARTIAL по возврату в Galaxy.** Реальные Initialize, Support, Run и Finalize
подключены в `NativeRangersInterface`. Windows HAL reference исполнил через
установленную таблицу полный бой до естественного поражения, затем настоящий
`Robot::FinalizeRobotRuntime`. FRun, экран правительства и галактический
переход ещё не выполнялись. Победа и native/deko runtime остаются NOT RUN.
[Машинный evidence](matrixgame-e2e592-native-rangers-interface-evidence.json).

## Владение и обязательные операции

Initialize проверяет и копирует все 16 настоящих Rangers callbacks, создаёт
EngineCallbacks и NativeRangersServicesScope. Install публикует RobotInterface
только после успешной инициализации и реального Support. Неудача оставляет
callbacks/services/global interface несвязанными. Вложенный Run запрещён.

Support на Windows читает GetDeviceCaps настоящего D3D9Device по полному
указателю. На Switch проверяет состояние очереди настоящего DekoDevice и его
ReadCaps, требующий загруженной vertex program. Texture limit 8192 и один
vertex stream соответствуют ограничениям native_texture и BindStagingVertex.
Это ограничения реализации, не измерение VRAM или обещание успешной аллокации.
Коды и порядок отказов исходные: query 1, texture hardware 3, streams 2, OK 0.
Неизвестный renderer вызывает Unsupported.

Run вызывает реализованный E591 TRobotRun: оригинальные Init/form/L3GRun,
SaveResult, проверенный Deinit, затем output commit и исходный код. Finalize
снимает публикацию интерфейса, освобождает services перед callback adapter;
отклоняет вызов при живой карте/Run. Повторный Finalize допустим. Реальные
data/audio/music host и renderer остаются borrowed и должны пережить интерфейс.

RangersHostSession получил отдельный deferred-callback constructor, сохранив
обычное поведение существующего конструктора и layout публичных options.
Обычный host проверен отдельно реальным roundtrip громкости музыки. Исходный
Windows DLL loader InitializeRobotRuntime не вызывается: его platform-loading
роль выполняет native Install. Исходный FinalizeRobotRuntime действительно
вызван; RobotModule остаётся нулевым, DLL не загружается.

## Завершённые runtime проверки

- Windows HAL: настоящие caps 8 textures, width 16384, streams 16.
- Все четыре callable slots; invalid Initialize rollback; readonly callbacks;
  публикация глобального интерфейса только после успешного Install.
- Полный бой: natural loss code 2, signed STAT_TIME 221677, 10 robot kills,
  5 turret kills, 30 transition frames, 450 draws, 82528 дополнительных
  оригинальных Takt(25). Это ускоренная симуляция, не FPS/realtime доказательство.
- Все шесть полей результата совпали с исходным SaveResult snapshot после
  cleanup карты при живом host. Nonnull caller settings сохранились побайтно.
- Failed Init/retry, sentinel output, оригинальный FinalizeRobotRuntime,
  отказ Run после Finalize, повторный Install и отказ duplicate Initialize.
- Исходный SDK main проверил восстановление gamma/window/target/depth/viewport
  и освобождение device/timer leases. Новый image oracle здесь не запускался.

MSVC ASan частичный: новые interface/Run/host/test TUs инструментированы;
большие исходные archives переиспользованы. Это не full-engine UBSan/LSan.
CPU Support policy отдельно исполнена Windows, host ASan/UBSan и ARM64/QEMU:
восемь boundary/query/code-order cases. Полные interface/support/host TUs
скомпилированы host sanitizer и ARM64; эти два gates compile-only.

## SDK и оставшаяся цепочка

Стандартная CMake target `matrixgame-native-rangers-interface` собрана через
devkitA64 с `MATRIXGAME_BUILD_RANGERS_RUN=ON`; default targets не меняются.
Отдельный AArch64 API ELF удерживает настоящие методы interface и Run binding,
проверен nm: сильных undefined symbols нет. Native/deko Support branch
скомпилирована и слинкована, на Switch не исполнялась.

Артефакты в `build/matrixgame/e2e592-native-rangers-interface/sdk`:

| Артефакт | Байты | SHA256 |
| --- | ---: | --- |
| libmatrixgame-native-rangers-interface.a | 271222 | dba45a95a9c259b86ae4aac551bdf5b298818b721c4434cb2ff7a7cfa7207676 |
| matrixgame-native-rangers-interface-link.elf | 53859312 | 4f5a2d49977e4afd21c30feba7c8c788f532f93e822ed8898084478d1560c517 |

SDK own-source, generated-original, link-input и artifact hashes сверены.
Generated originals совпадают с E591, закреплённые upstream чисты; игровые и
frozen inputs, logs2, основная/root NRO и оба E590 пакета сохранили хеши.
Новая NRO этим этапом не создаётся. Последние пользовательские quick/manual
NRO остаются отдельными артефактами E590. Commit/push не выполнялись.

Воспроизведение (существующие SDK/toolchain и проверенные E590/E591 inputs):

```text
python tools/matrixgame_native_rangers_interface_windows.py
python tools/matrixgame_rangers_support_checks.py windows
python tools/matrixgame_rangers_support_checks.py host
python tools/matrixgame_rangers_support_checks.py arm64
python tools/matrixgame_native_rangers_interface_cross_compile.py
python tools/matrixgame_native_rangers_interface_sdk.py
python tools/matrixgame_native_rangers_interface_audit.py --output reports/matrixgame-e2e592-native-rangers-interface-evidence-new.json
```

Прошедший полный бой в текущем ходе повторно не запускался: использованы
сохранённые runtime и результаты с проверенными хешами. Начальные неуспешные
попытки fixture/SDK script сохранены; проверки не ослаблялись.

Достигнутая цепочка: real host data/audio/music → Initialize callbacks/services
→ Support → опубликованный RobotInterface → original Init/бой/SaveResult
→ Deinit/restore → caller statistics/code → original FinalizeRobotRuntime.

Для полной исходной цепочки ещё необходимы настоящий in-memory Galaxy/Player,
government GI-tree и renderer session, затем исполнение Robot::FRun и
TfGov::OnOpen с возвратом UI, rewards/history. Получение статистики не заменяет
этот переход. Требуются также естественная победа и пользовательская проверка
native/deko на Switch. Сохранения не загружались и не изменялись.
