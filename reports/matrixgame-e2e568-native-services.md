# E2E-5.68 — scoped Rangers callbacks в native entry

Статус: **host ASan/UBSan, ARM64/QEMU и отдельная SDK/NRO PASS**.
Это интеграция передачи/владения callback table, не реализация отсутствующих
standalone audio/resource/text/music services и не завершение порта.

## Изменения

`NativeGameOptions::services` принимает настоящую `SMGDRangersInterface`.
Все 16 function pointers обязательны при передаче таблицы: неполная таблица
отклоняется с именем отсутствующего callback до output binding и исходного Init.
Указатели копируются в `NativeRangersServicesScope`; исходная структура может
быть изменена/освобождена после входа. **Контекст и реальные host services
остаются borrowed и должны жить до конца Deinit.**

Явная попытка заменить уже активную `g_RangersInterface` отвергается, чужая
привязка не сбрасывается. Null сохраняет прежнюю семантику caller-managed
binding; standalone diagnostic по умолчанию по-прежнему не устанавливает
фиктивную таблицу.

На успешном входе scope переходит владельцу `NativeInitRuntime`. При Deinit
он снимается после исходного CGame/map/sound/text/cache teardown и resource
scope. На исключении Init локальный scope остаётся привязанным во время
оригинального rollback, затем снимается при unwind. Это не ранний reset таблицы
перед возможными `m_SoundDestroy`/`m_RangersTextClear`. Полный успешный native
teardown с живыми sound/text ресурсами этим этапом не исполнен: порядок проверен
по production code, early rollback — runtime-тестом.

Нельзя менять `g_RangersInterface` или вызывать legacy DLL bind/unbind, пока
владеет native scope. Повреждение его ownership invariant вызывает fail-fast.
Существующий Windows launcher и `OriginalGameOptions` не изменены. Оригинальные
DLL Init/Deinit действительно пишут `g_RangersInterface`, но они не являются
владельцем новой native session и не вызываются новым путём.

## Подключение существующего bridge

Схема для настоящего Rangers host, где уже готовы **все** реальные callbacks:

```cpp
EngineCallbacks host(real_callbacks); // реальные host services живут ещё дольше
NativeInitRuntime game;
NativeGameOptions options{window, device};
options.services = EngineCallbacks::Table();
// Также задаются map/resource_root/output_root и остальные параметры сессии.
game.InitializeNativeGame(options);
// Исходный игровой цикл.
game.Deinit();                        // до уничтожения host и window/device
```

`EngineCallbacks::Table()` имеет правильный тип, но одной непустой таблицы
недостаточно: объект EngineCallbacks должен быть активен, и его реальные
ресурсные зависимости должны работать. Проверка function pointers не способна
доказать готовность внешнего host. В production не добавлены пустые реализации
sound/text/music и не придуман mapping символических Sound.* names в WAV.

## Проверки и точные границы

| Проверка | Что доказано |
| --- | --- |
| Host ASan/UBSan | Копия таблицы, независимость от последующей мутации caller table, фактический оригинальный CLoadProgress dispatch, callback exception/unbind, 16 отдельных missing-callback preflight отказов, запрет takeover, два early Init cleanup цикла |
| ARM64/QEMU | Те же адресные случаи с той же настоящей original engine зависимостью |
| Actual Init prefix | Default и provided services проходят исходные statics/heap/data/cache до намеренного `ReadCaps → NATIVE_SERVICES_CAPS_PENDING`; затем настоящий rollback и unbind |
| SDK compile/link/NRO | Новый native API и владение собраны в отдельной standard target, strong undefined = 0; generated original inputs без изменений |

Прогресс передан оригинальным `CLoadProgress::SetCurLPPos`, а не прямым
синтетическим вызовом callback. Этот компонентный progress вызов не выдаётся
за достижение позднего loading stage внутри Init. Полный успешный Init, карта,
host audio/text/music playback/rendering и устройство не исполнялись.

Callback fixtures содержат все слоты, но неиспользуемые sound/text/music методы
немедленно бросают Unsupported: они не заявляют выполненную обязательную
операцию. Они устанавливаются только в тесте и не включаются в NRO.
Первоначальная ошибка Window fixture сохранена в
`host/compile-test-first-fixture-failure.log`. Окончательные `host-final` и
`arm64-final` пересобирают последний test TU и используют прежние проверенные
production objects с записанными SHA; большие map/HAL suites не повторяются.

[Evidence с командами, объектами и хешами](matrixgame-e2e568-native-services-evidence.json).

## Воспроизведение и артефакт

Из корня checkout, при наличии тех же original sources и SDK dependencies:

```powershell
python tools/matrixgame_native_services_checks.py host
python tools/matrixgame_native_services_checks.py arm64
python tools/matrixgame_native_services_recheck.py host
python tools/matrixgame_native_services_recheck.py arm64
python tools/matrixgame_native_services_sdk.py
python tools/matrixgame_native_services_audit.py --output build/matrixgame/e2e568-native-services/audit-repeat.json
```

Для нового аудита выбирается новое имя выходного файла. Небольшой recheck
существует для переиспользования проверенных production objects; полный runner
также компилирует окончательный fixture, если выполняется заново.

Отдельная NRO:
`build/matrixgame/e2e568-native-services/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`.
Размер: 6981829 байт. SHA256:
`d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750`.
Точные размер/SHA256 ELF также записаны в evidence. Основная NRO не затронута.

## Текущая цепочка Init и открытые компоненты

Последний аппаратный run остаётся ReadParams → ApplyGammaRamp до map constructor.
E568 не меняет эту runtime границу. Прежние интеграции gamma, settings/sampler,
StoredStateBackend, minimap output и F9 сохраняются.

Теперь native session может получить таблицу готового настоящего Rangers host
с контролируемым сроком жизни. Standalone diagnostic всё ещё требует самих
реальных host resource/audio/text/music services; именно их наличие новый API
не доказывает. В прочитанном оригинальном `upstream/cpp/src/Robot.cpp`
RobotCreateSound создаёт TSoundBufferControl и вызывает Configure, когда
булев флаг RobotSound разрешает звук;
RobotPlaySound использует GR_Main::SoundManager. Новый binding не создаёт
эти host dependencies и не заменяет их PCM recorder-ом.
Также открыты successful native Init/PrepareMap и аппаратные
image/VRAM/HID/focus/presentation/recovery/F9 gates. Отсутствие Switch не мешает
дальнейшей программной интеграции, но hardware evidence не заменяется fixture.

Protected input hashes, upstream clean pin
`218115a72fa9c5d43269f4ca84a8596a0bcb82f1`, frozen/resources/saves,
пользовательский logs2.7z и предыдущая E567 NRO сохранены. Commit/push и
взаимодействия с устройством нет.
