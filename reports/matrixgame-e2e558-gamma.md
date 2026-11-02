# E2E-5.58 — native presentation gamma, software candidate

26.10.2026. **ORIGINAL GAMMA / PRESENTATION LUT SOFTWARE PASS; SWITCH HARDWARE PENDING.**

В пользовательском logs2 исходный CGame::Init остановился на
`backend calibrated gamma submission unsupported`. Исправлен непосредственно
этот отсутствовавший provider, без удаления исходного ApplyGammaRamp и без
успешного пустого override. [Аппаратный baseline E557](matrixgame-e2e557-logs2.md).

## Реализация

Original GenRamp и ApplyGammaRamp сохранены. DekoDevice::ApplyCalibratedGamma
принимает настоящие три таблицы WORD[256] и публикует их в NativeGammaStore.
Неподдерживаемый swapchain index и известная потеря queue остаются ошибками.
ReadGamma возвращает текущую таблицу **данного application presentation**;
это не чтение системной gamma консоли. Физический calibrator не предоставляется,
caps.calibrate_gamma остаётся false. Согласно контракту CALIBRATE при отсутствии
calibrator исходная таблица передаётся напрямую:
[SetGammaRamp](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setgammaramp).

Отдельный GPU fullscreen triangle применяет LUT после original draw/cursor,
перед показом swapchain image. Таблица хранится с исходной 16-bit точностью,
std140 uniform занимает 4096 bytes; RGB изменяются независимо, alpha сохраняется.
Для промежуточных значений unorm применяется линейная интерполяция соседних
элементов, в том числе для 5/6-bit source channels. Физическая response curve
экрана и поведение немедленного изменения уже сканируемого изображения не
заявляются: применение выполняется при очередном Present.

Game primary surface теперь указывает на отдельный scene image того же
формата/размера; swapchain image служит выходом LUT. Поэтому gamma не меняет
original render targets, minimap cache, texture contents или game readback.
Это соответствует разделению surface/display в
[Gamma Controls](https://learn.microsoft.com/en-us/windows/win32/direct3d9/gamma-controls).
Публичные original draw/offscreen API не заменены. Шейдеры material не изменены.

## Владение и cleanup

- Scene backing входит в тот же TargetSet/DisplayImages owner, что и swapchain
  backing, и проходит существующий reset transaction/rollback. Лимит allocation
  учитывает обе color copies: `2 * color_bytes * BackBufferCount + depth_bytes`.
- Издержки: ещё один color image на каждый backbuffer и fullscreen pass на
  Present. LUT/descriptors packet — 12288 bytes, shader code — отдельные SDK
  blocks. Это не заявление о сохранении VRAM/FPS; измерений на Switch нет.
- Gamma packet immutable для конкретной submission; удерживается до GPU wait,
  а при failed queue — до закрытия channel. Recorder уничтожается до backing.
- Owning CaptureState lease включает snapshot gamma. Scalar Store/Restore
  CDeviceState остаётся отдельным исходным контрактом. Device close инвалидирует
  gamma leases; попытка позднего Restore возвращает DeviceLost.
- GammaStore не меняется при allocation failure захвата snapshot, не удерживает
  texture/surface owners и не изменяет global/system display state.

## Diagnostic проверка следующего ручного запуска

Smoke сохраняет `smoke.bgra` из original scene: центральный пиксель должен
оставаться красным BGRA `[0,0,255,255]`. Затем передаётся отдельная diagnostic
таблица R=16384, G=32768, B=49152 во всех 256 entries. После GPU submission,
до present/release acquired image, считывается **выходной swapchain image**:
`gamma-smoke.bgra`, ожидаемый центр BGRA `[191,128,64,255]`, RGB tolerance ±1.
Простое сохранение красного source image такую проверку пройти не может.

Journal stages: `GPU.presentation_gamma`, `GPU.presentation.after_gamma`,
`A.gamma.presentation_check`. После smoke исходная таблица восстанавливается;
дальше выполняется неизменённый original Init и bounded original loop.
Readback после LUT — дополнительная diagnostic операция только для 32-bit color;
обычный renderer поддерживает прежние presentation formats. Проверка не
изменяет платформу, ресурсы, сохранения или логи прошлых запусков.

## Исполненные software проверки

| Gate | Результат |
| --- | --- |
| HOST ASan/UBSan | PASS. 18 реальных original GenRamp/ApplyGammaRamp вызовов; 21 таблица; 4 128 768 component samples; snapshot/restore, repeated restore, invalid index, closed lifetime; diagnostic final-pixel rejection/checker. |
| ARM64/QEMU | PASS, тот же production code и original methods, те же samples. |
| Windows reference | PASS, тот же original GenRamp и typed original ApplyGammaRamp с минимальным declaration/routing fixture. Это не новый полный Windows Init. |
| Table bytes | HOST/ARM64/Windows совпали: `3f065dff3ff0750d03042f61d6a89e8e4f13fde295e51dfbd12f96412ef8facb`. |
| GLSL | PASS: точные production vert/frag sources, Mesa llvmpipe 4.6/EGL, 21 таблица ×3328 = 69 888 pixels, RGB ±1 в RGBA8; alpha exact; orientation и source image сохранены. Это software OpenGL, не native Switch GPU. |
| SDK | Полный devkitA64 diagnostic compile/link PASS; uam скомпилировал оба новых shader. Их DKSH встроены в ELF; strong undefined = 0. |
| NRO | Штатная отдельная CMake target PASS; NRO segments/ASET/icon/NACP проверены, внешний RomFS не требуется. |
| Сохранность | 255 generated original files совпали с E556. Upstream clean/pin сохранён; primary/cumulative/E556 NRO, оба старых dist пакета, frozen, 15 protected inputs, CFG/robots.pkg, 107 hardware inputs и пользовательский logs2 archive проверены по hashes. Commit/push нет. |

Адресные тесты используют exact original GenRamp; при сверке generated body
допускается только существующая замена WORD на std::uint16_t. Header shell в
CPU тесте содержит необходимые три SGammaVals, не весь CMatrixConfig; routing
fixture направляет actual ApplyGammaRamp в production NativeGammaStore. Эти
тесты не объявляются успешным полным CGame::Init.

Первоначальные адресные сборки выявили const handle API mismatch и пропущенный
test include path; они исправлены. Итоговые все gates выше PASS. Broad Windows
HAL/gameplay tests не повторялись: original Windows D3D9 path не менялся.
Material и gamma shader manifests имеют отдельные имена.

## Артефакт и воспроизведение

NRO: `build/matrixgame/e2e558-gamma/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro`

Размер: **6 961 349 bytes**.
SHA-256: `2fb8689975480771e690cd764b89af803c6e2d2cd234fe24904c18cb7c86fdd1`.
Build ID: `82034333146d-0a5a726dccbc8ffa2534`.

[Точный audit/metadata/commands/hashes](matrixgame-e2e558-gamma-evidence.json).
SDK configure command и полный link graph: `build/matrixgame/e2e558-gamma/sdk`.
Требуются установленные SDK и существующие checked original font assets,
как в E556. Для текущего дерева:

```powershell
$taskPython = 'C:/Users/Sklyarov/AppData/Local/Programs/Python/Python313/python.exe'
& $taskPython tools/matrixgame_gamma_checks.py host
& $taskPython tools/matrixgame_gamma_checks.py arm64
& $taskPython tools/matrixgame_gamma_checks.py windows
& $taskPython tools/matrixgame_gamma_checks.py gl
& $taskPython tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e558-gamma --diagnostic-source build/matrixgame/e2e543-journal/source
$env:DEVKITPRO = 'C:/devkitPro'
$env:DEVKITA64 = 'C:/devkitPro/devkitA64'
$taskCmake = 'C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
& $taskCmake --build build/matrixgame/e2e558-gamma/sdk --target matrixgame-native-diagnostic-nro -j 4
& $taskPython tools/matrixgame_gamma_audit.py --output build/matrixgame/e2e558-gamma/recheck.json
```

Final auditor создаёт новый JSON эксклюзивно. E556 standard auditor намеренно
сохраняет старую границу source changes; он не является аудитором нового E558.

## Что остаётся открытым

Новая NRO на Switch **не запускалась**. Последняя реальная аппаратная граница
остаётся E557: gamma failure до map construction/PrepareMap. Новый post-gamma
pass/readback, дополнительный scene memory demand, actual default-pool reset,
native полный Init, карта, первый original frame и управление не подтверждены.
Остальные ранее отмеченные limits и независимые GI/P0 вопросы не закрыты.
Следующий фактический runtime blocker после gamma пока неизвестен. Аппаратное
исполнение и перенос файлов по-прежнему выполняет пользователь вручную.
