# MatrixGame: аудит и частичный перенос анизотропии

Статус: **native MIN-анизотропия реализована; full settings/battle integration OPEN;
Switch GPU NOT TESTED**. Продолжение PR №20, не аппаратный PASS.

## Реальное поведение исходного менеджера

Новый Windows x86 reference линкует **оригинальный**
`CMatrixSampleStateManager` из `MatrixGameInternal` и вызывает его с настоящим
`IDirect3DDevice9`. Глобальные `g_D3DD/g_D3DDCaps` в probe имеют настоящие
SDK-типы и указывают на созданный HAL/REF device; это не имитация D3D.
Pinned upstream не изменён. SHA-256 manager.cpp после нормализации CRLF:
`6d2562c1ba84c166023efbdff1b16b92a7e0904bd5a22474d17037f8702b9b1e`.
Verifier проверяет также manager.cpp в snapshot, использованном сборкой.

На каждом из HAL и REF выполнены 54 перехода настоящего менеджера:
9 настроек AF × MIN/MAG/MIP × POINT/LINEAR. Проверены ограничение степени
capabilities и назначение ANISOTROPIC/MAXANISOTROPY. На данном адаптере
cap=16; настройки 32 и −1 дают 16 из-за исходного unsigned-сравнения.
Это наблюдение исходного кода, не рекомендация принимать отрицательную
настройку в будущем интерфейсе порта.

Исходный manager подменяет LINEAR на ANISOTROPIC для **всех трёх** фильтров.
При AF>0 Set/Get успешно возвращают это состояние, но исходный набор
не проходит `ValidateDevice` (HRESULT `88760822`). Затем режимы проверены
**по отдельности**, при остальных LINEAR:

| ANISOTROPIC в регистре | HAL ValidateDevice | REF ValidateDevice |
|---|---|---|
| MINFILTER | PASS | PASS |
| MAGFILTER | `88760822` | PASS |
| MIPFILTER | `88760822` | `88760822` |

HAL filter_caps=`03030700`, REF=`1f071f00`. MAG-анизотропия зависит от
capabilities устройства. Использование ANISOTROPIC для MIPFILTER прямо
обозначено как undefined в
[документации D3D9](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtexturefiltertype).

Выполнены draw/readback для исходных настроек AF=0/1/2/4/8/16.
В контрольной сцене замена только undefined MIP на LINEAR не изменила
изображение ни на HAL, ни на REF. При AF=8 центральный пиксель HAL=`ff400000`,
REF=`ff480000`, причём это различие сохраняется и с корректным LINEAR MIP.
**Нельзя приписывать различие цветов одному undefined MIP** или выдавать
картинку одного устройства за универсальную побитную норму анизотропии.
Результаты undefined-конфигурации сохранены как наблюдения.

## Реализованный путь

`MeshSamplerState` принимает ANISOTROPIC только для min-фильтра и хранит
MAXANISOTROPY (1–16) независимо. MAG/MIP-анизотропия явно отклоняется.
Состояние VO-материала передаётся без потери этих полей. В deko3d min
становится linear с `setMaxAnisotropy`; для point/linear min передаётся 1,
даже если в исходном состоянии сохранилось MAXANISOTROPY=16. Это исключает
случайное сохранение AF после возврата материала к обычному фильтру.

SDK квантует степень аппаратного фильтра; код хранит исходный integer,
не обещая побитного равенства AF-изображений D3D9 и Switch. Реализация SDK:
[tsc_generate.cpp](https://github.com/devkitPro/deko3d/blob/master/source/maxwell/tsc_generate.cpp).

Оригинальный manager **не заменён** этим состоянием и ещё не интегрирован
в native battle engine. В частности, его MAG/MIP-подмены нельзя молча
пропускать или объявлять полностью портированными. Нужен дальнейший перенос
настроек с capabilities и определённой политикой MIP.

## Проверки

- MSVC: полный набор 18/18 PASS.
- Linux ASan/UBSan: полный набор 19/19 PASS.
- Native Windows, настоящий D3D9 NULLREF Set/Get и ARM64/QEMU:
  2057 операций, transcript 65 856 байт,
  SHA-256 `48f15591c7d0687e77978d45975ac01f6331b3099d664b1d02c1d050737e4e69`.
  Включены все степени 1–16, включение min-анизотропии, возврат к point/linear,
  отказ от MAG/MIP-анизотропии и некорректных степеней без изменения состояния.
- HAL и REF: прежние 13 pixel-контролей и дополнительный контроль LINEAR
  при сохранённом MAXANISOTROPY=16 — по 14 PASS, unexpected_pixels=0.
  Это проверка отключённой AF/сохранённого состояния, **не качества native AF**.
- Будущий Switch `--sampler-state` расширен до 15 контролей: 14 pixel-контролей
  плюс изоляция двух sampler-дескрипторов. Он только скомпилирован.
- Настоящий Horizon SDK: compile/link PASS, ELF64 AArch64 без strong UND,
  5 548 832 байт, SHA-256
  `b5c342627371ac3021194af50f1ed99283149f92e1cc413138ede867e71d4862`.
  Никакой NRO/аппаратный прогон этим не заявляется.

Результаты: `build/anisotropy-manager-oracle/oracle.json` и raw stdout/stderr,
`build/anisotropy-state-oracle/oracle.json`,
`build/mg-sampler-arm64/fixtures/oracle.json`,
`build/anisotropy-pixels-{hal,ref}.{json,log}`,
`build/anisotropy-{msvc,asan}-ctest.log`, `build/matrixgame/deko/build-report.json`.

Повторить Windows reference:

```text
cmake --build build/mg-native --config Release --target matrixgame-anisotropy-d3d9-reference
python tests/probe_matrixgame_anisotropy.py --binary build/mg-native/Release/matrixgame-anisotropy-d3d9-reference.exe --source-dir build/mg-src --output build/anisotropy-manager-oracle
```

Для GUI subsystem процесс запускается и ожидается через subprocess;
непосредственный запуск из PowerShell без ожидания не является доказательством
его завершения. Общая цель ТЗ, интеграция MatrixGame, аппаратная устойчивость
и зависание после «Готово» остаются открытыми.
