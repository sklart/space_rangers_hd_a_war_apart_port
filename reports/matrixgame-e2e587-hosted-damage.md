# E2E-5.87: настоящий Damage и изменение состояния объектов

Статус: **PASS в двух Windows D3D9 HAL hosted-сессиях; Switch NOT RUN**.
Доказательства: [evidence](matrixgame-e2e587-hosted-damage-evidence.json).
Production-код не изменён. Основная и отдельные E586 NRO сохранены.

## Новое доказательство

Ранее E585 доказал Fire/WeaponHit callbacks. Теперь вызовы наблюдаются
непосредственно вокруг настоящего виртуального `hiti->Damage(...)` в обоих
исходных branches WeaponHit. Исходные условия и аргументы вызовов сохранены.
Сценарий использует настоящий selection/arcade и configured EngineSink KA_FIRE;
координаты, HP, параметры оружия, флаг и TTL поджога не задаются тестом.

В каждой сессии:

| Измерение | Результат |
| --- | --- |
| Симуляция / удержание fire / ожидание после release | 5000 / 4000 / 1000 мс |
| Fire / WeaponHit callbacks / callbacks с объектом | 4 / 11 / 7 |
| Настоящие виртуальные Damage | 7 |
| Вызовы с измеримым изменением состояния | 2 |
| Поджог BEHF_BURN map objects | false → true, TTL 0 → 10000 мс |
| Снижение HP / удаление объекта | 0 / 0 |
| Original loop Draw / игровое время loop | 208 / 5125 мс |
| Readback после loop/recovery | 4 разных кадра 800×600 |

Поджог произошёл в двух событиях на времени 550 мс, weapon type 1000.
Остальные пять Damage не дали наблюдаемого изменения состояния.
`health_known=0` означает отсутствие применимого HP-показателя у этих объектов,
а не нулевое HP. Урон вражескому роботу, уникальность всех callback targets,
полный бой и результат миссии этим сценарием не доказаны.

## Наблюдение и владение

Добавлены только отдельные test TUs и runner/audit:
`tests/matrixgame_damage_probe.hpp/.cpp`,
`tools/matrixgame_hosted_damage_checks.py`,
`tools/matrixgame_hosted_damage_audit.py`.

Временная настоящая ссылка `SObjectCore` удерживает core на время вызова,
не запрещая удаление самого объекта. После Damage объект читается через
`core->m_Object`; при удалении это nullptr. RAII освобождает ссылку и восстанавливает
предыдущий observation guard. Assertions проверяют отсутствие активного guard
перед Verify и уничтожением probe. Наблюдатель счётчиков Fire не сбрасывает их.

Protected IsAblaze/GetAblazeTTL читаются из отдельной test-копии самого
`CMatrixMapObject::Damage`, где такой доступ законен. Первоначальный неуспешный
compile с внешним доступом сохранён в result.json; заголовки/access modifiers
не менялись. Успешный предварительный result-1.json предшествует дополнительным
cleanup assertions. Итоговый **result-2.json** соответствует текущему коду.

Аудит удаляет ровно внесённые observation строки и требует точного совпадения
обоих тел TUs с E583 Windows-generated source и E586 SDK-generated source.
Он проверяет source/link/binary/output hashes, пересчитывает transitions из
каждого DAMAGE_EVENT, сверяет summary, независимо декодирует восемь PNG
со CRC/zlib/filter validation и проверяет два cleanup цикла. Наблюдения двух
сессий совпали. Повторный runtime понадобился только из-за добавленных assertions.

## Достигнутая цепочка и границы

Совместное Windows HAL доказательство охватывает настоящий owning Rangers host,
original full CGame::Init, загрузку карты, transition Takt/Draw, original form и
L3GRun, selection/arcade/input/fire, WeaponHit → virtual Damage → поджог,
focus release/recreate, readback, CGame::Deinit и освобождение owning host.
Ранее отдельно проверены native output-folder/CFile cache и StoredStateBackend,
изображения/шрифты/audio/data providers, host ASan/UBSan и ARM64/QEMU в областях,
зафиксированных соответствующими отчётами. Здесь большие suites не повторялись:
изменено только тестовое наблюдение. **Этот сценарий Damage выполнен только
Windows HAL, без ASan/UBSan и QEMU.**

Native/deko цепочка compile/link/package подготовлена E586. Сохранены:

| NRO | SHA256 |
| --- | --- |
| E586 interactive | `3ebdf7e7217d1c9e110a9994663196614ee3228a0957a18e76bd4a91cc7aa713` |
| E586 quick | `3ee1b00f60940c83b2b638a6a217201027e45f8b42fb90b7501871de27e80928` |

Их пути и стандартные SDK build targets приведены в
[E586](matrixgame-e2e586-interactive-diagnostic.md) и
[инструкции ручного теста](../docs/matrixgame-e2e586-interactive-diagnostic.md).
Новых обязательных программных зависимостей этот тест не выявил.
Открыты full native/deko Init с видимыми map frames; аппаратные image/audio/HID,
F9/focus/recovery; вражеский HP Damage, полноценный бой и возврат результата
owning игре. Последняя реальная Switch boundary из logs2 остаётся
ReadParams/ApplyGammaRamp до конструктора карты. PC-проверка её не продвигает.

Аудит сверил чистые upstream pins, игровые/frozen fingerprints, обе E586 NRO,
основную и cumulative NRO, D:/logs2.7z. Commit/push и работа с устройством
не выполнялись. Цель порта остаётся активной.

## Воспроизведение

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_hosted_damage_checks.py
python tools/matrixgame_hosted_damage_audit.py --output reports/matrixgame-e2e587-hosted-damage-recheck.json
```

Runner использует уже проверенные Windows E583 native archives и E577 Rangers
objects. Полные команды и fingerprints записаны в evidence. Аудит создаёт новый
отчёт exclusively; существующий evidence перезаписывать не требуется.
