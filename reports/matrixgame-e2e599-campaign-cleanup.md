# E2E-5.99 — владение и cleanup настоящей кампании

Продолжение E598. Полная original generation по-прежнему выполняется настоящим
worker, включая 300 warmup turns, Player/Star/HomePlanet и stage8. Исправления
находятся в port overlay; pinned upstream, frozen, игровые данные и сохранения
не менялись. RNG draw order не менялся, seed override не используется.

Подтверждённые исходными стеками LeakSanitizer пропуски исправлены:

* RebuildOutlineSegments освобождает старые segment records перед заменой list.
* TPlanet_Destroy освобождает уже опустошённый RangerRelations list; integer
  entries не передаются деструктору TObjectList как объекты.
* TPlayer_Destroy освобождает AchievementStats; TRanger_Destroy — опустошённый
  Quests list. TEther_Destroy очищает entries и освобождает настоящий lock.
* TGalaxyEvent::ClearTextData использует typed dispose для WideString cells,
  вызывая настоящий деструктор строки.
* Script runtime владеет TVarArrayEC, созданными в течение его жизни. Исходные
  SetArray/AssignFrom сохраняют borrowed alias semantics. Явный free снимает
  регистрацию. Оставшиеся массивы уничтожаются после остановки всех VM workers
  и original FinalizeScriptHostRuntime; созданные до scope массивы ему не принадлежат.

Дополнительная instrumentation выявила два исходных UBSan дефекта. Инициализация
surface-loot grid выходила за обе границы массива 14×7 (inclusive15×8); циклы
теперь обходят ровно существующие клетки, как исходный BFS. Seed arithmetic в
TryTriggerEconomicEvent сохраняет low32 wrap через unsigned32, вместо signed
C++ overflow. 960 compile-time проверок фактически сгенерированных выражений
против независимого Python low32 oracle PASS на host GCC и devkitA64. Это
арифметическая проверка, не дополнительное выполнение gameplay или RNG draws.

| Gate | Доказательство |
| --- | --- |
| Host ASan/UBSan/LeakSanitizer | asan/5: exit0, stage8, 73 звезды, 339 планет, 47 рейнджеров, turn301, storage3, cleanup; без suppressions |
| VM alias/lifetime | array-lifetime/1: ASan/UBSan/LSan exit0, два цикла, настоящий ctor/dtor, alias survives, explicit free, worker thread, foreign array и duplicate owner |
| Windows reference | windows/1: partial MSVC ASan exit0, stage8, 73 звезды, 348 планет, 41 рейнджер, turn301, storage3 |
| ARM64/QEMU | arm64/2: exit0, stage8, 73 звезды, 328 планет, 42 рейнджера, turn301, storage3, original cleanup |
| SDK dependency | отдельный архив: заменены только семь affected original TUs, остальные 371 member совпадают с E598 |
| SDK standard target | matrixgame-rangers-campaign и transitive owners; retained API AArch64 ELF, strong undefined=0; runtime NOT RUN |

Общий каталог доказательств: `build/matrixgame/e2e599-campaign-cleanup`.
Итоговый аудит: [machine-readable evidence](matrixgame-e2e599-campaign-cleanup-evidence.json).
SDK campaign library SHA256: `0f50621a1166c3188cc263ae116e6a5d80ec00fab01c0200f5c923bac954dc8f`;
retained API ELF SHA256: `649cf013e63fb43a1dc9591ff8ef2fc59a5d781afd46fb2bd04bbd5b38f09d81`.
Аудит выполнился до последующего изменения campaign owner для memory-snapshot
интеграции E600. Эти исторические artifacts не заменяются следующими проверками.
Часть reused original corpus ранее собрана без instrumentation: PASS не означает,
что каждый исходный TU инструментирован. Runtime assertions включены. Windows
MSVC ASan не заменяет host LeakSanitizer gate.

Отрицательные попытки сохранены: asan/1 — surface grid OOB; asan/2 — signed seed
overflow; asan/3 — functional stage8, но 3552 bytes/130 leaked allocations;
asan/4 — compile failure private Pending access, без runtime. asan/5 закрывает
выявленные утечки. arm64/1 завершилась до compile на нормализации путей runner;
arm64/2 использует исправленный runner. Старые successful binaries не менялись.

Воспроизведение новых проверок (attempt directories создаются отдельно):

```text
python tools/matrixgame_campaign_cleanup_checks.py
python tools/matrixgame_script_array_checks.py
python tools/matrixgame_campaign_cleanup_cross.py windows
python tools/matrixgame_campaign_cleanup_cross.py arm64
```

`matrixgame_campaign_cleanup_sdk.py` создаёт новый dependency archive и стандартную
SDK цель в отдельном каталоге; повторная запись существующего архива запрещена.
Исходный CFile/output provider и основная NRO сохранены. Новая NRO на этом этапе
не создавалась. Полный бой, возврат Gov, награды/история, естественная победа и
native/deko FRun этим этапом не доказаны. Switch NOT RUN; commit/push не выполнялись.
