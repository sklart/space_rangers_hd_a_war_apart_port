# MatrixGame: независимые sampler-состояния

Статус: CPU/register/reference PASS; Horizon compile/link PASS;
**Switch GPU NOT TESTED; full MatrixGame/battle parity OPEN**.

## Подтверждённое расхождение и исправление

Оригинальный `MatrixLib/3G/3g.cpp:661–671` устанавливает linear
min/mag/mip и LOD bias −1. `VectorObject.cpp:1270–1271` меняет только
min/mag на point: linear mip сохраняется. Прежние восемь пресетов
`TextureSampling` связывали min/mag/mip и не передавали LOD bias,
поэтому не могли представить этот путь.

`MeshSamplerState` хранит min/mag/mip, адресацию U/V/W и bias независимо.
Поддержаны point/linear, mip none/point/linear, wrap/clamp. Старые восемь
пресетов сохранены как явные преобразования. Проверяются NaN/Inf,
недопустимые значения и регистры; `Apply` сохраняет предыдущее состояние
при ошибке. Планировщик VO проверяет и передаёт полное состояние материала.

Рендерер формирует отдельный sampler descriptor для каждого draw (до 256)
до записи команд. Дескрипторы не перезаписываются до завершения синхронной
очереди; handle ссылается на индекс draw. Полный batch проверяется до
публикации uniforms/descriptors. Независимые фильтры/оси и bias передаются
в настоящие `dk::Sampler::setFilter/setWrapMode/setLodBias`.

## Проверки и границы доказательства

- MSVC: полный набор 18/18 PASS; после расширения VO-проверки повторены
  sampler/vector 2/2 PASS.
- Linux ASan/UBSan: полный набор 19/19 PASS. VO проверяет mixed
  point-min/linear-mag/linear-mip, разные U/V и bias −1, а также отказ
  от материала с недопустимым sampler.
- ARM64/QEMU: 2040 изменений регистров, 57 148 байт transcript,
  SHA-256 `917e030385fc500efb0849f66209d51ad89d38b8f789f027939d01bb6faf8139`.
- Настоящий Windows D3D9 NULLREF `SetSamplerState/GetSamplerState`
  совпал побайтно с независимым Python transcript и native-кодом.
  Это проверка состояния, **не отрисовки**. Из pinned исходников извлечены
  12 константных вызовов; извлечение не считается выполнением веток игры.
- Настоящая D3D9 HAL и REF отрисовка/readback: по 13 контролей,
  2304 проверяемых внутренних пикселя на контроль, unexpected_pixels=0.
  Проверены point texels с linear mip, point/no mip, bias −1/+1,
  четыре сочетания U/V wrap/clamp, независимые min/mag при увеличении
  и уменьшении. Для линейной интерполяции допуск 1 в красном канале;
  остальные каналы и point-контроли точные. Адаптер vendor=4318/device=11525.
- `tools/matrixgame_deko_build.py`: настоящий Horizon SDK compile/link,
  ELF64 AArch64 без strong UND. ELF 5 544 736 байт,
  SHA-256 `f084c2b5893800cdd37e99490c43418a8e1e23d8da188e6929d60bbb54f712df`.
  Это рабочая сборка данного изменения, **не аппаратный прогон и не NRO**.
- Будущий probe `--sampler-state`: 13 таких же GPU-контролей и один
  двухпроходный кадр с разными wrap/clamp на одном texture slot.
  Последний проверяет изоляцию sampler descriptors между draw.
  Он скомпилирован и слинкован, но **не выполнялся на Switch**.

Локальные результаты: `build/sampler-d3d9-oracle/oracle.json`,
`build/mg-sampler-arm64/fixtures/oracle.json`,
`build/sampler-pixels-{hal,ref}.{json,log}`, `build/sampler-asan-ctest.log`,
`build/matrixgame/deko/build-report.json`.

## Открытая работа

`MatrixSampleStateManager.cpp` при включённом AF заменяет linear на
anisotropic и передаёт `MAXANISOTROPY`. Этот путь ещё не портирован;
anisotropic явно отклоняется. Mirror/border, sRGB, max mip и displacement
также не реализованы. Перечень регистров в `DeviceState.cpp` не является
доказательством выполнения каждого режима. Нужны дальнейший аудит
материалов/настроек, настоящий GPU-прогон и интеграция движка/боя.

Дробный bias в deko3d квантуется SDK: его реализация ограничивает bias
диапазоном −15…+15 и кодирует с 8 дробными битами. Native-состояние
сохраняет исходный конечный float, не выдавая это за побитное совпадение
изображения D3D9 для произвольного bias.

Источники семантики:
[D3DSAMPLERSTATETYPE](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dsamplerstatetype),
[deko3d sampler encoding](https://github.com/devkitPro/deko3d/blob/master/source/maxwell/tsc_generate.cpp).

P0-зависание после «Готово» остаётся ROOT CAUSE UNCONFIRMED. Эти изменения
его не доказывают и не меняют согласованный аппаратный кандидат `3772031`.
