# MatrixGame E2E-5.34 — владение default resources при Reset

Статус **PARTIAL**. Native ownership candidate прошёл HOST ASan/UBSan,
ARM64/QEMU и полную SDK compile/link. WINDOWS HAL oracle имеет открытый FAIL:
две повторные попытки Reset после state block возвращают INVALIDCALL.
Этап целиком не закрыт. Switch Init/карта/кадр не исполнялись.

## Исправление

Baseline production guard запрещал Reset, если default texture оставалась
только в sampler bindings устройства после удаления всех игровых tokens.
Отдельный baseline исполнил настоящий native Texture/StateStore и воспроизвёл
эту границу; GPU image в нём — явно обозначенный CPU fixture.

Новый guard считает фактические shared_ptr-поля renderer state и текущих
surface bindings. Он не создаёт сильных ссылок, не выделяет память, не меняет
состояние и не освобождает ресурсы при проверке. Совпадение требует одного
объекта и control block; поля учитываются один раз. Переполнение ledger
консервативно запрещает Reset.

Игровые tokens, внешние shared owners, SurfaceLevel, backing owners,
surface leases и COW snapshots продолжают запрещать Reset. Snapshot,
разделяющий сам state object, учитывается до копирования его полей.
Renderer-only bindings освобождаются существующим Commit после успешного
создания новой presentation. OOM и частичный Attach оставляют прежние state,
targets и texture image пригодными для retry. Старые строгие queries
DefaultResourcesLive сохранены; новая проверка применяется только к Reset.
Никаких fake COM/vtable, HRESULT success или пустых GPU обработчиков не добавлено.

Связь с оригиналом: CMatrixMap::ReleasePoolDefaultResources и
CBaseTexture::OnLostDevice освобождают игровые обёртки; renderer может ещё
удерживать ту же текстуру. Это применимость по исходникам, а не наблюдение
конкретной оставшейся текстуры полной карты на Switch. Дефект не объявляется
первым runtime blocker Init и не связывается с GI-кэшем основной игры.

## Раздельные результаты

| Среда | Что исполнилось |
| --- | --- |
| HOST | Production Texture/Surface/StateStore/Reset transaction под ASan/UBSan: renderer-only release, внешние owners/handles, shared и distinct COW snapshots, aliases, surface leases, managed payload, OOM/attach rollback, no-allocation guard. GPU/VI — CPU fixtures. Отдельно адресная surface/render-texture lifetime регрессия. |
| ARM64/QEMU | Тот же production ownership/transaction candidate, assertions включены. Не GPU. |
| COMPILE/LINK | Полная native-entry ELF devkitA64; 0 strong undefined. Оригинальные 255 generated source files совпадают с E2E-5.33. Число и SHA фактических reachable own link inputs — в evidence. NRO не создавалась. |
| WINDOWS HAL | 15 actual Reset calls, 13 ожидаемых исходов, 2 FAIL. Реальные renderer-only texture/surface/depth bindings допускают Reset; игровые default owners и SurfaceLevel мешают; managed texture допускается. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Реальные native allocation/upload, Init, карта, recovery, submit и Present не подтверждены. |

## Открытый WINDOWS HAL FAIL

Адаптер NVIDIA GeForce RTX 5060, nvldumd.dll, версия из GetAdapterIdentifier
00200000:001006ce, D3DCREATE_SOFTWARE_VERTEXPROCESSING. Каждый независимый
сценарий начинает с нового настоящего HAL device.

Удерживаемый state block с default texture ожидаемо мешает Reset. После
Release state block (возвращён refcount 0) retry остаётся INVALIDCALL вместо
ожидаемого S_OK. Повторяется и при предварительном отключении sampler binding.
Если state block освободить **до первой попытки Reset**, всё проходит; пустой
освобождённый state block также не мешает. Причина различия пока не установлена;
не объявляется дефектом конкретного драйвера.

[Контракт Microsoft Reset](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-reset)
требует освобождать state blocks/default resources до вызова; после неудачи
разрешает только Reset, TestCooperativeLevel и Release. Oracle соблюдает
ограничения после неудачи, но намеренно исследует Reset с живыми owners.
Этот результат нельзя считать доказательством успешного HAL recovery.
Native guard, напротив, отклоняет внешних владельцев до low-level Reset;
его CPU rollback не доказывает поведение реальной queue при потере устройства.

Исходный FAIL сохранён в windows-initial-failure; расширенный oracle оставляет
exit 10 и status FAIL. Ожидания не ослаблены ради PASS.

## Где находится настоящая цепочка

Text::Render → DeviceIdentity / Font.2Ranger уже закрыт программно в
[E2E-5.24](matrixgame-e2e524-text.md): исходный Rangers.ttf, glyph coverage,
метрики/kerning, raster, native upload и cleanup; 17940 точных D3DX9 сравнений.
Оригинальный Init, RobotPreload, stor.Load, PrepareMap и form/loop связаны в
native entry; полная Windows карта исполнялась в E2E-5.33.
Этот этап не повторяет прежнюю полную регрессию и не выдаёт линковку за Init.

Следующий открытый программный вопрос — объяснить raw HAL state-block retry
и проверить необходимость изменения recovery контракта. Для первой карты
на Switch нужны реальный проход native Init/PrepareMap, выполнение GPU
allocation/upload и первого form Draw/Present. Первый точный отказ этой
цепочки без её запуска **не наблюдался**. Recovery после ошибки самой queue
и аппаратная проверка карты/управления остаются открытыми.

## Сохранность и воспроизводимость

Команды, exits, hashes и логи:
build/matrixgame/e2e534-reset-owners/{baseline,host,arm64,sdk,windows,regression-host}.
Baseline sources сохранены отдельно, чтобы проверять прежний код после правки.
Аудит: tools/matrixgame_reset_owners_audit.py; итог:
[machine-readable evidence](matrixgame-e2e534-reset-owners-evidence.json).
Проверяются NRO, frozen, аппаратные входы, robots.pkg, CFG и frozen GI source,
root HEAD и чистый upstream pin. Commit/push не выполнялись; ресурсы и сейвы
не изменялись. P0, GI repeating-text causality, manual-publish ENOENT и старые
Switch hangs остаются OPEN.
