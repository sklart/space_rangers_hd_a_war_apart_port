# MatrixGame: явные alpha/blend/depth состояния deko3d — 18.10.2026

Статус: native draw submission теперь несёт проверяемый MeshRasterState. Renderer
передаёт supported state каждого draw в настоящий deko3d. Проверены CPU register
семантика и Windows D3D9 Set/Get; Switch GPU image и skin pipeline НЕ PASS.
Hardware freeze после «Готово» остаётся ROOT CAUSE UNCONFIRMED.

## Изменения

RasterRegister использует реальные numeric D3D9 register IDs для ZENABLE,
ZWRITEENABLE, ALPHATESTENABLE, SRCBLEND, DESTBLEND, ZFUNC, ALPHAREF, ALPHAFUNC,
ALPHABLENDENABLE, COLORWRITEENABLE и BLENDOP. Это значения register protocol,
не pointers, fake D3D header/device или методы, возвращающие фиктивный успех.

MeshRasterState поддерживает boolean depth/alpha/blend, 8 compare functions,
11 blend factors, 5 blend operations, byte alpha reference и RGBA write mask.
Apply проверяет копию и публикует её после Validate; ошибка не меняет прежний
state. W-buffer, separate-alpha blend enable, неизвестные register/enum values
явно отвергаются. Это ограниченный typed backend boundary, не полный D3D9 API.

Per-draw ColorState, ColorWriteState, DepthStencilState, BlendState и AlphaRef
устанавливаются для каждого draw. Alpha-test отключается через Compare Always;
при включении используются actual compare и reference/255. RGB factors/operation
также задаются для alpha: это D3D9 semantics без separate-alpha blending.
Default deko alpha One/Zero здесь не подходит и больше не используется неявно.
Основание hardware API: [deko3d GPU state implementation](https://github.com/devkitPro/deko3d/blob/master/source/maxwell/gpu_3d_state.cpp).

VectorMaterial и PlanVectorFrame сохраняют raster state вместе с texture/sampler
и original surface-first order. DrawFrame проверяет весь batch до публикации
uniforms/commands. Полное состояние переустанавливается между draw calls, чтобы
mask/additive pass не оставлял blend/depth/color-write flags следующему pass.
Command buffer увеличен с 64 до 128 KiB для дополнительных per-draw commands;
max batch остаётся 256. Аппаратное выполнение максимального batch не проверено.

Shaders/Uniforms ABI не меняются. Alpha-test применяется к output alpha текущего
shader. Полная fixed-function texture-stage/skin combiner семантика, selection
GSP_SIDE/GSP_ORDINAL, gloss/mask/back и burn passes ещё требуют переноса.
TF_ALPHABLEND не переводится автоматически в единственный универсальный pass:
в original skin code разные passes используют SrcAlpha/InvSrcAlpha или One/One,
а depth write меняется отдельно. Ни один отсутствующий pass не объявляется готовым.

Native defaults явно заданы: depth test/write, Lequal, alpha/blend disabled,
SrcAlpha/InvSrcAlpha, Add, RGBA write, alpha compare Gequal/reference 8.
Это native initial state для текущего pass API, не утверждение о всех default
состояниях D3D9 или точке входа MatrixMap. Значения 8/192 берутся из настоящих
VectorObject/Map alpha-test states; original control flow пока не исполняется.

## Проверка настоящим D3D9 runtime

probe_matrixgame_raster_d3d9.cpp создаёт невидимое окно и настоящий D3D9 NULLREF
device, делает SetRenderState/GetRenderState и освобождает COM/window resources.
Он не вызывает Draw/Present и не создаёт программный framebuffer. Device unavailable
означает exit 77, а не PASS. В данном локальном прогоне NULLREF был доступен.

Independent Python oracle строит 2194 valid changes и 96580 bytes state snapshots.
Включены 119 constant inputs из pinned original source (61 SkinManager, 43 Map,
15 VectorObject), все compare/blend/write-mask ranges, alpha boundaries и 2000
детерминированных изменений. Извлечение constants создаёт inputs для проверки:
это НЕ executed trace original game branches или доказательство полного Draw.

Проверяются исходники LF-normalized SHA-256:

- MatrixSkinManager.cpp: d9d565af29522397011903bf6e3d730575092d0eb8ad540506740ff8f03f4778
- MatrixMap.cpp: dcdc7f878c186741897f158894de353e4bec190b64dd6236857179b665ce1696
- VectorObject.cpp: 11ffb4eb842b5cdf6041d0ddc3bf25a124b06bdd8ac36f090b4c2ac6d9586303

Native MSVC и actual D3D9 NULLREF Set/Get побайтово совпали с oracle:
SHA-256 1827a52d3049151146c473639dfc592cdf637536d212f4269c61d9e78f23e40d.
ARM64/QEMU дал тот же SHA. Native invalid controls проверяют register/value rejection
и сохранение прежнего state; это не сравнение undefined/invalid D3D inputs.
Vector contract дополнительно проверяет forwarding разных material raster states.

```sh
bash tools/matrixgame_raster_arm64.sh
```

Host oracle: tests/probe_matrixgame_raster_state.py --binary <native test>
--output <scratch> [--reference-binary <actual Win32 D3D9 reference>].
CI не требует Windows device и использует CPU/ARM64 contracts.

## Future physical probe

`--raster-state` выполняет восемь actual GPU controls на offscreen 64x64 target:
alpha 7/8 против Gequal reference 8, отсутствие depth write после alpha reject,
явное depth-write off, SrcAlpha/InvSrcAlpha (включая destination alpha), One/One,
color mask, восстановление opaque состояния внутри batch и между frames.
Для fractional blending задан допуск 1 LSB; остальные controls требуют exact bytes.

PASS печатается только после GPU DrawFrame/readback comparisons. Этот режим сейчас
только скомпилирован/связан и НЕ запускался. Он не является игровым боем или host
software substitute. Новая NRO не создаётся, кандидат 3772031 не меняется.

## Открытые ворота

Реальные original skin callbacks ещё не подключены к этому state boundary.
Остаются full CVectorObject/CCache, material/texture combiners, JPEG/auto mip,
Init/Run, карта/бой/возврат результата, physical GPU behavior и весь аппаратный
P0/P1. Успех Set/Get, host tests или ELF не закрывает MG0–MG5, плавность или батарею.
Локальные логи/oracles: build/matrixgame/native-core/raster-*.

## Финальная фиксация

- MSVC x64: полный native suite 16/16; D3D9 NULLREF comparison прошёл.
- ASan/UBSan: полный native suite 17/17, detect_leaks=1, halt_on_error=1;
  raster SHA совпал с D3D9/MSVC oracle.
- ARM64/QEMU: 2194 operations, тот же SHA.
- devkitA64/deko ELF compile/link PASS, nm -u пуст.
  ELF 5272168 bytes, SHA-256
  a11987262e136c48866d24ab407f2b5b17a8ee382475dae2f97bbb80bbe67792.
  Hardware NOT TESTED, nro_packaged false.
- Native VO contract проверяет forwarding raster state вместе с texture/sampler;
  исходная geometry serialization и порядок ranges не меняются.

Работа изолирована в worktree; original checkout, master и кандидат 3772031
не заменяются. Общий объём ТЗ остаётся открытым.
