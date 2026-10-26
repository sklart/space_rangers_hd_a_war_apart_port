# MatrixGame E2E-5.35 — standalone Npad bootstrap и актуальный Init

**COMPILE/LINK PASS; SWITCH HARDWARE PENDING.** Устранён пропуск standalone
input bootstrap. Полный Init/первая карта/кадр на Switch не исполнялись.

## Реальное изменение

SwitchInput::Attach использует padInitializeDefault и читает No1/Handheld,
но намеренно не меняет Npad policy чужого процесса. Раньше standalone entry
также не устанавливал эту policy. По исходникам libnx padInitializeDefault
только задаёт маску чтения PadState; padUpdate читает настроенные HID sources.
Он не заменяет padConfigureInput.

Standalone entry теперь проверяет живой HID service/shared memory, вызывает
hidInitializeNpad → hidSetSupportedNpadIdType(No1, Handheld) →
hidSetSupportedNpadStyleSet(NpadFullCtrl). FullCtrl соответствует Pro,
handheld и паре Joy-Con с обоими стиками и полным набором кнопок текущего
адаптера. Поддержка одиночного Joy-Con этим этапом не заявляется.

Это тот же порядок, что в
[исходном padConfigureInput libnx](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/pad.c),
но ненулевой IPC Result попадает в существующий exception handler вместо
diagAbort. CheckDeviceResult проверяет именно result != 0, поэтому положительный
код ошибки libnx не принимается за успешный HRESULT. Конфигурация стоит до
Window/Device/Runtime: при её отказе графические ресурсы ещё не созданы.

Политика принадлежит самостоятельному процессу и заканчивается вместе с его
applet/HID lifetime. Код не пытается выдавать отсутствие getter supported IDs
за возможность сохранить/восстановить чужую policy. Borrowed SwitchInput и
его Attach/Detach/Poll побайтно совпадают с E2E-5.33; интеграция основной игры
продолжит использовать настройки хозяина. Результаты реальных HID IPC не
проверялись без консоли.

## Проверки

| Категория | Итог |
| --- | --- |
| COMPILE/LINK | Новая отдельная devkitA64 ELF полного native entry, 0 strong undefined. В самом entry object есть внешние ссылки на все три реальные HID операции. 183 reachable own object/archive inputs; 255 generated original source files совпадают с E2E-5.34. |
| HOST ASan/UBSan | NOT RUN в этом этапе: изменён только Switch standalone bootstrap с HID IPC. Предыдущие E2E-5.34 ownership/rollback и surface/render-texture проверки не повторялись и не выдаются за исполнение нового bootstrap. |
| ARM64/QEMU | NOT RUN: QEMU Linux не исполняет Horizon HID IPC. Предыдущий ownership PASS остаётся отдельным доказательством. |
| WINDOWS HAL | NOT RUN: Windows/shared Init/Draw/resources код не менялся. Сохранённый E2E-5.33 полный Init/карта/form loop использован как прежнее доказательство в плане. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING: подключения, handheld/docked, реальные события и управление боем не подтверждены. |

Команды, exits, source/binary/link input hashes:
build/matrixgame/e2e535-standalone-input/sdk/result.json.
Старые SDK ELF/логи сохранены; новой NRO нет.

## Последовательность первой карты и открытые вопросы

Добавлена [актуальная карта Init и cleanup](../docs/matrixgame-init-sequence.md):
standalone/platform resources → original static/heap/package/config/cache →
AttachDeviceImpl → pipeline/config → map members/debug font/cursor →
RobotPreload → stor.Load → PrepareMap → UI/video/presentation/text →
default resources/transition → form/loop/первый Draw/Present.
Для каждой группы указаны provider, владелец, cleanup и граница доказательства.
Проверено по исходникам, текущему generated manifest и сохранённым runtime logs;
это не новый аппаратный журнал.

Отдельно уточнён открытый HAL oracle E2E-5.34: оригинальный map recovery
использует CDeviceState со scalar render/sampler/light/material capture.
В original MatrixGame source нет CreateStateBlock/BeginStateBlock/EndStateBlock.
Поэтому два raw state-block retry FAIL сохраняются открытыми, но не объявляются
воспроизведением отказа CMatrixMap::CheckLostDevice. Никакие ожидания HAL oracle
не ослаблены и E2E-5.34 целиком не переведён в PASS.

Первый точный runtime blocker native Init остаётся **NOT OBSERVED**. Для первой
карты нужны исполнение настоящих GPU/VI allocation/upload, resource lookup,
PrepareMap и первого Draw/Present на Switch. Queue-loss recovery и integrated
host transition остаются открытыми. F9 screenshot — явный optional Unsupported.
P0 основной игры, GI repeating-text causality, manual-publish ENOENT и старые
Switch hangs не закрыты этим bootstrap.

Защищённые входы и git инварианты сверяются в
[evidence](matrixgame-e2e535-standalone-input-evidence.json).
Upstream, основная NRO/frozen, ресурсы и сейвы не изменялись. Commit/push нет.
