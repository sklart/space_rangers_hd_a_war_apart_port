# E2E-5.62 — оригинальный UI restore на single-sample target

26.10.2026. **SOFTWARE FIX PASS; NATIVE INIT/HARDWARE NOT RUN.**

Исправлен конкретный отказ native renderer: оригинальный
`CIFaceElement::Draw` после `CInstDraw::ActualDraw` безусловно вызывает
`SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE)`. Прежний native setter
бросал Unsupported при любом TRUE, включая обычные single-sample targets.
Проверка до исправления воспроизвела этот отказ под ASan/UBSan.
[Evidence](matrixgame-e2e562-single-sample-evidence.json).

## Семантика и реализация

На single-sample render target D3DRS_MULTISAMPLEANTIALIAS и
D3DRS_MULTISAMPLEMASK не изменяют результат rasterization. Их значения при
этом остаются наблюдаемым state и должны сохраняться/восстанавливаться.
Это прямо определено в
[официальной документации D3D9](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3drenderstatetype).

NativeRenderState теперь принимает и хранит BOOL enable и полный 32-bit mask;
GetRenderState, NativeStateStore lease и оригинальный CDeviceState roundtrip
возвращают исходные значения. Прежний запрет mask != 0xffffffff на draw снят
для single-sample pipeline. Проверка BOOL 0/1 и атомарный отказ неверного
значения сохранены.

Ненулевой target MultiSampleType или MultiSampleQuality по-прежнему запрещён:
у backend нет multisample allocation/resolve pipeline. DekoDevice::Record
передаёт ValidateDraw параметры фактического color attachment, выбранного для
обычного или presentation draw. Проверка выполняется до записи GPU-команд.
Настоящие creation/negotiation guards также сохранены. Native SDK rasterization
остаётся single-sample; принятие логического enable не создаёт MSAA и не
выдаётся за реализацию multisample rendering.

В `tests/matrixgame_actual_draw_state.hpp` удалена прежняя нормализация raw
Windows enable=TRUE в native enable=FALSE. Replay теперь передаёт исходное
значение. Проверка unsupported multisample targets использует записанный в
capture target type, а не значение логического enable.

## Доказательства

| Gate | Результат |
| --- | --- |
| Baseline | PASS: до исправления `NativeRenderState::Render(161,1)` воспроизвёл Unsupported, соответствующий исходному UI restore. Исходные cpp/hpp восстановлены в `baseline/source` и побайтно совпали с hashes, записанными при baseline build. Старый executable сохранён. Полный CIFaceElement::Draw этим тестом не исполнялся. |
| Windows HAL | PASS: настоящий single-sample target, DrawPrimitiveUP и GetRenderTargetData. Восемь сочетаний enable={FALSE,TRUE}, mask={0,1,0x55555555,0xffffffff}; все 8 framebuffer images совпали побайтно, 1441 pixel содержит triangle. GetRenderState вернул каждое исходное значение. Это настоящий D3D9 draw/readback, без Present и без native Init. |
| HOST ASan/UBSan | PASS: logical state/capture/restore, masks, invalid BOOL rollback, явный отказ multisample type/quality. |
| Linux ARM64/QEMU | PASS: та же адресная state проверка. |
| Windows Win32 | PASS: MSVC адресная state проверка. |
| Оригинальный CDeviceState | PASS на host ASan/UBSan, ARM64/QEMU и Win32. 103 render states, 13×8 sampler states, lights/material, три restore/reset, отсутствие texture/buffer retention. Теперь сохранённая конфигурация содержит enable=TRUE и mask=0. |
| Raw original map captures | PASS: текущий host ASan/UBSan replay 33 690 draws, 16 574 unique records, failures=0. Из них 18 338 draws имеют raw enable=TRUE, все targets single-sample. Это state/shader-preparation replay сохранённых Windows captures, не повторный полный Windows Init и не deko GPU draw. |
| devkitA64/libnx/deko3d | PASS: отдельный полный SDK compile/link и стандартная цель NRO, strong undefined=0. Новая сигнатура ValidateDraw связана с фактическим native Record. |

В старом stored-state тесте обновлены ожидания для mask=0 в соответствии с
проверенной single-sample семантикой. Также учтена уже реализованная в E537
aliased line support: проверяется разрешённый LASTPIXEL=TRUE и сохраняется
отказ LASTPIXEL=FALSE. Point rasterization остаётся запрещённой. Это не снятие
проверок неподдерживаемой работы.

Большие Windows Init/PrepareMap/Draw tests повторно не запускались. Использованы
два неизменённых capture файла E543, их hashes сверены с исходным result.json:

* session 0: 18 838 draws, 9527 unique, raw TRUE — 9169 draws;
* session 1: 14 852 draws, 7047 unique, raw TRUE — 9169 draws.

## Сборка и воспроизведение

Новая отдельная NRO:

`build/matrixgame/e2e562-single-sample/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

Размер: **6 965 445 байт**. SHA-256:
`15ce098bd30ab5ec37d9212fb1d53d63e4f08bb43769a2fd319c1a04defc5c9a`.

NRO имеет три корректных сегмента, встроенные icon/NACP и не требует внешнего
RomFS. SDK script использует стандартную `matrixgame-native-diagnostic-nro`
с nacptool/elf2nro. Все команды и hashes записаны под E562 в result.json.
Сборка воспроизводится из текущего подготовленного workspace и сохранённых
локальных prerequisites, перечисленных в SDK configure command.

```powershell
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_single_sample_reference.py
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_single_sample_checks.py host
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_single_sample_checks.py arm64
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_single_sample_checks.py windows
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_single_sample_checks.py host --suite stored
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_single_sample_checks.py arm64 --suite stored
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_single_sample_checks.py windows --suite stored
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_single_sample_checks.py host --suite replay
C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe tools/matrixgame_single_sample_sdk.py
```

`baseline` mode служил запуску до исправления и на текущем production source
должен перестать воспроизводить прежний отказ. Для исторического воспроизведения
сохранены baseline executable, build command, hashes и точные старые cpp/hpp.

## Init и ограничения

Это офлайн-исправление настоящего исходного UI path, а не новый аппаратный
результат. Последний Switch journal по-прежнему остановился в
`CGame::Init → ReadParams → ApplyGammaRamp` до карты. E558 gamma implementation
и E559 full ReadParams CPU evidence сохранены; более поздний actual native
Init/PrepareMap/первый игровой framebuffer пока не проверен.

Открыты настоящие multisample targets/resolve, другие unsupported renderer
режимы, аппаратная проверка gamma/геометрии/изображения/VRAM, queue-loss recovery
и дальнейшая игра. Наличие wireframe/point/scissor guards само по себе не
доказывает следующий обязательный runtime blocker; нужны конкретные исходные
вызовы и их контекст. Устройство не требуется для продолжения этой работы.

Основная NRO и E556/E558/E561 NRO сохранены. Upstream clean на pin
218115a72fa9c5d43269f4ca84a8596a0bcb82f1; frozen, игровые ресурсы, saves,
пользовательский logs2.7z и прежние dist NRO сверены с fingerprints.
Commit/push и взаимодействия с устройством не выполнялись.
