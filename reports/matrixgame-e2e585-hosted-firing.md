# MatrixGame E585: штатный arcade-ввод, стрельба и callbacks

Статус: два Windows HAL hosted loop цикла с оригинальной стрельбой PASS.
Production-код не изменён; E583 NRO сохранена. Switch **NOT RUN**.

## Проверенный игровой путь

Новый сценарий выполняет настоящие Init/map/transition, создаёт original form,
закрывает стартовый диалог, проходит Enter/Pause/menu/scroll и выбирает живого
вооружённого робота игрока. Далее выполняются штатные операции:

`CreateGroupFromCurrent → Select(ROBOT) → SetArcadedObject → EngineSink key down
по g_Config KA_FIRE → original Takt/Fire/effects/WeaponHit → key up → StopFire
через original Takt → focus recovery → frames → выход из arcade → Deinit`.

Ввод и часы заданы детерминированным fixture; это не OS mouse hit-test и не
physical Switch HID. Робот, selection group, оружейный UI, карта, устройство,
Rangers host/callbacks и исходная симуляция настоящие. Тест не задаёт напрямую
trace point, координаты робота, HP, результат damage или flags выстрела.

Для наблюдения используется отдельная **test-копия** generated
MatrixEffectWeapon.cpp. В ней два вызова read-only observer: после настоящего
`++m_FireCount` и из настоящего WeaponHit. Они читают owner identity и
неразрушающий GetFireCount; IsFireWas/IsHitWas не вызываются, потому что очищают
игровые flags. Original body/callbacks, trace, damage handler, sound и teardown
не заменяются. Audit удаляет только два observer calls и include и требует
полное текстовое совпадение с исходной production-копией.

## Результаты двух сессий

Обе дали одинаковые наблюдения:

| Метрика | Значение на сессию |
| --- | --- |
| Начало удержания configured KA_FIRE | 125 мс игрового времени, key=1 |
| Удержание / отпускание / ожидание завершения | 4000 мс / на 4125 мс / ещё 1000 мс |
| Стрелявших экземпляров оружия | 2 |
| Настоящих вызовов Fire | 4; у каждого оружия GetFireCount 1→2 |
| Вызовов WeaponHit | 11 |
| Callback events с настоящим object pointer | 7 |
| Callback events TRACE_STOP_LANDSCAPE | 0 |
| Последний выстрел | 3625 мс; после отпускания новых выстрелов нет |
| Робот на конце наблюдения | Жив, очередь приказов пуста |

11 callbacks не означают 11 попаданий или 11 разных целей: среди них есть
завершающие/прочие уведомления, а object events могут повторяться. Урон
противнику, изменение HP врага, победа/поражение и полный бой **не проверены**.
Счётчики наблюдателя сверены с настоящими GetFireCount, без ResetFireCount.

Каждый original loop выполнил 208 Draw и 5125 мс симуляции; стрельба заняла
последние 5000 мс этого интервала. Затем прошли actual focus loss/recreate,
четыре настоящих map readback frames и полное освобождение form/runtime/host.
Восемь PNG 800×600 проверены независимо: CRC/zlib/RGB, 139640–139827 distinct
colors, четыре разных RGB hashes внутри каждой сессии. Один кадр просмотрен:
настоящие arcade camera/HUD, оружейные элементы, карта и эффекты видны.

Gamma/window/viewport, device/timer leases, retained owners и cwd восстановлены
в двух циклах. Observer core reference и arcade mode освобождаются до original
Deinit. Оригинальные audio services используются с прежним SDL dummy driver;
слышимость и аппаратный звук этим тестом не доказаны.

## Исправления сценария проверки

Первые попытки сохранены и не объявляются дефектами production-кода:

- Прямой низкоуровневый robot->Fire вне arcade дал shots=0: AI стороны вправе
  отменить его без соответствующего высокоуровневого приказа. Сценарий заменён
  штатным configured input в режиме ручного управления; ожидание shots/hits
  не ослаблено.
- Вход в arcade без selection group дал access violation. CDB + link map
  локализовали null GetCurGroup в CreateWeaponDynamicStatics, read address 0x30.
  Fixture теперь проходит обычные CreateGroupFromCurrent/Select перед arcade.
  Создание UI не пропускается; заглушка и production null-guard не добавлены.

Исходники, executable, objects, metadata и логи первой попытки и неправильного
arcade precondition сохранены в отдельных snapshot folders. CDB executable/map/
log тоже сохранены. Audit сверяет их fingerprints. Правильный сценарий затем
завершился двумя полными успешными циклами.

## Артефакты и воспроизведение

[Точные команды, исходники, observations, snapshots и hashes](matrixgame-e2e585-hosted-firing-evidence.json).

```powershell
$env:PYTHONIOENCODING='utf-8'
python tools/matrixgame_hosted_firing_checks.py
python tools/matrixgame_hosted_firing_audit.py --output reports/matrixgame-e2e585-hosted-firing-recheck.json
```

Audit создаёт новый файл exclusively. Нужны прежние VS/Python dependencies,
E583 native archives и E577 Rangers host objects. Большие host-ASan/ARM64/SDK
suites не повторялись: shipping production source не менялся. Новые тестовые
observer files в NRO не добавлены. Это Windows execution proof, не новый
sanitizer/ARM/deko runtime PASS.

Сохранённая отдельная NRO:
`build/matrixgame/e2e583-robot-orders/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.
Размер **14895301 bytes**, SHA256
`53db0a16a8f187fc8dd50a55be20e2e3bd3335d6f86e80c02c13e164a9664233`,
build ID `82034333146d-7f1c2b2bb3e62afaeca9`. Все её записанные own/production
inputs, compiled dependencies, ELF/NRO/NACP/map fingerprints совпадают с E583.
Основная/cumulative/прежние NRO, 78 game inputs, доступные frozen/save artifacts,
logs2.7z и clean upstream pins сверены read-only и сохранены.

## Следующие gates

Цепочка на PC теперь включает movement и настоящую ручную стрельбу с callbacks
через original form/loop и owning host. Native/deko Init/map/form/loop всё ещё
NOT RUN. Последняя измеренная Switch boundary остаётся ReadParams/ApplyGammaRamp
до map constructor. Нужны physical HID, image/audio/F9/focus/recovery и полный
бой/результат/возврат хозяину. Аппаратный запуск и перенос данных остаются за
пользователем; commit/push агент не выполнял.
