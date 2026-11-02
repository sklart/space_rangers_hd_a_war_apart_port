# E2E-5.88: original AI attack и настоящий урон HP противнику

**PASS: две Windows HAL hosted-сессии с ускоренной original form симуляцией.
Switch NOT RUN.** [Машинные доказательства](matrixgame-e2e588-hosted-battle-evidence.json).

Закрыта программная граница E587: теперь подтверждён настоящий урон HP
вражескому боевому объекту после штатной команды атаки. Production-код не менялся.

## Сценарий и измерения

После настоящего Rangers owning host → original full Init → map/transition →
original form/L3GRun тест выбирает ближайшую пару из существующих player robots
и hostile robots/cannons. Обнаружены 4 подходящих player robots и 27 hostile
combatants. Исходная XY дистанция ближайшей пары — 1924.511963 world units.

На времени 125 мс штатные `RobotToLogicGroup` и `PGOrderAttack` получают игрока
из клетки 192,473 и существующую цель стороны 4 в клетке 342,353. Код теста
не вызывает Damage/Fire напрямую, не задаёт координаты, HP, параметры оружия,
aim или внутренние AI target fields. Поиск пути, выбор огневой цели, оружие,
WeaponHit и Damage выполняются исходной игрой.

Для ограниченного теста Pump вызывает до 20 дополнительных настоящих
`g_FormCur->Takt(25)` между кадрами, максимум 4000 вызовов. После первого
наблюдаемого снижения hostile HP дополнительные Takt прекращаются. Это
**ускоренная программная симуляция**, без утверждения о realtime performance,
FPS на Switch или кадре на каждый tick. Существующий deterministic loop clock
не изменён; дополнительное игровое время учитывается отдельно.

Обе сессии дали одинаковые игровые измерения:

| Показатель | Результат на сессию |
| --- | --- |
| Дополнительные original form Takt(25) | 1324, то есть 33100 мс |
| Original loop Draw / базовое loop time | 208 / 5125 мс |
| Игровое время loop с дополнительной симуляцией | 38225 мс |
| Наблюдаемые hostile virtual Damage от выбранного attacker | 2 |
| Время обоих Damage / weapon type | 34875 мс / 1000 |
| HP до → после первого Damage | 210 → 185 |
| HP до → после второго Damage | 185 → 160 |
| Статус после обоих Damage | object exists, returned_dead=false |
| Verify после команды / attacker и исходная цель существуют | 38100 мс / да |
| Focus recovery, readback, Init/Deinit/host cleanup | PASS |

В каждом событии тип объекта 3 — robot, сторона 4. HP снято непосредственно
до и после исходного виртуального Damage через настоящий getter. Для каждого
вызова доказано уменьшение HP на 25 getter units. Наблюдатель допускает любого
hostile combatant, поражённого выбранным attacker; identity между событиями и
совпадение с первоначальной PGOrderAttack target отдельно не записывались.
Поэтому таблица не служит доказательством общей HP-истории единственной цели.
Конечные координаты и сам маршрут также отдельно не измерялись.

## Изменения и проверка

Добавлены только тестовые файлы:

- `tests/matrixgame_battle_probe.hpp/.cpp`: штатная команда атаки, read-only
  snapshots и RAII-ссылки на настоящие SObjectCore attacker/target/hit objects.
- `tools/matrixgame_hosted_battle_checks.py`: отдельная композиция настоящего
  owning host, Windows HAL, full original Init/form/loop и bounded extra Takt.
- `tools/matrixgame_hosted_battle_audit.py`: проверка событий, времени, источников,
  оригинального тела WeaponHit, журналов, восьми PNG и защищённых fingerprints.

В отдельной test-копии MatrixEffectWeapon.cpp ровно два исходных virtual Damage
calls обёрнуты snapshots. Ссылка удерживает core, сохраняя возможность удаления
объекта; после вызова объект читается через core->m_Object. Сторона и тип цели
сняты до вызова. При отсутствии HP после вызова значение помечается unknown,
а не используется как доказательство снижения HP.

Аудит удаляет внесённые строки и требует точного совпадения original weapon
body с E583 Windows-generated и E586 SDK-generated источниками. Проверены
все 12 compiled TU fingerprints, link inputs, executable и outputs; numeric
Damage summaries пересчитаны из событий. У обоих журналов последовательные
seq, ожидаемый composition source hash, успешный B.CGame::Init и journal_close.
Все восемь PNG независимо декодированы с CRC/zlib/filter проверками; в каждой
сессии четыре RGB images различны. Focus/video resources recreate, gamma,
window, timer/device leases, повторный Deinit и host/global cleanup PASS.

Выполнен только новый адресный Windows HAL сценарий. ASan/UBSan, ARM64/QEMU
и SDK build не повторялись: production inputs не изменены. Новый сценарий
урона сам по себе **не имеет sanitizer/ARM64 runtime proof**. Большие ранее
пройденные suites сохраняют только свою документированную область.

## Артефакты и остаток

E586 interactive NRO SHA256
`3ebdf7e7217d1c9e110a9994663196614ee3228a0957a18e76bd4a91cc7aa713`,
quick NRO SHA256
`3ee1b00f60940c83b2b638a6a217201027e45f8b42fb90b7501871de27e80928`
сверены и сохранены. Стандартные SDK targets и инструкция находятся в
[E586](matrixgame-e2e586-interactive-diagnostic.md).
Аудит также сверил current SDK own/generated sources и Rangers archives,
игровые/frozen fingerprints, clean upstream pins, основную/cumulative NRO
и неизменный D:/logs2.7z. Новых обязательных зависимостей этот сценарий не выявил.

Открыты full native/deko Init с видимой картой, аппаратные image/audio/HID/F9/
focus/recovery, полноценный бой с исходным результатом и возвратом owning игре.
Последняя измеренная Switch boundary остаётся ReadParams/ApplyGammaRamp до
конструктора карты. PC-симуляция её не продвигает. Commit/push, работа с Switch
и изменение upstream/resources/saves не выполнялись. Цель остаётся активной.

## Воспроизведение

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_hosted_battle_checks.py
python tools/matrixgame_hosted_battle_audit.py --output reports/matrixgame-e2e588-hosted-battle-recheck.json
```

Используются уже проверенные E583 native archives и E577 Rangers objects.
Exact compiler/link/runtime commands и SHA256 записаны в evidence. Аудит
создаёт новый отчёт exclusively.
