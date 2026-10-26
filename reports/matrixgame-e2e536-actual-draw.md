# MatrixGame E2E-5.36 — реальный draw stream и блокер миникарты

**PARTIAL / NATIVE LINE BLOCKER REPRODUCED.** Native renderer не объявлен
готовым: production guard отвергает LINESTRIP оригинальной рамки камеры.
Никакие проверки не ослаблены; failed profiles сохраняют status FAIL/exit 10.

## Что действительно исполнилось

Добавлен необязательный синхронный observer успешных Windows SDK draw.
Он не заменяет draw, не меняет возвращаемый HRESULT и не пишет device state.
Наблюдаются обычные/indexed/UP draw, shadow indexed draw до восстановления
bindings и debug-font background UP. Счётчики теперь также учитывают ранее
неучтённые UP/shadow вызовы. Observer borrowed и отключается до уничтожения
контекста. Без регистрации остаётся только проверка пустого указателя.

Полный исходный Windows сценарий выполнил два Init → RobotPreload → stor.Load
→ PrepareMap → form loop (8 Draw, 125 ms simulation) → focus release/recreate
→ дополнительные Draw/readback → Deinit. Проверки gamma/window/device leases
и исходных stored light/material slots сохранились. HAL result = 0; общий
процесс возвращает 10 из-за независимого native-state rejection после обоих
сеансов. Искусственные состояния не подставлялись в игру.

Сохранены value-only snapshots без COM pointers и GPU owners: topology/FVF,
103 render registers, 13×8 sampler registers, texture stages, presence mask
bindings, world/view/projection/texture matrices, viewport/material/light и
depth attachment. Из них production NativeRenderState/FixedVertexState/
Combiner выполняют реальную validation и подготовку uniforms. Это проверка
состояний, не GPU draw и не сравнение итогового изображения deko3d.

| Сеанс | Draw | Уникальных snapshots | LINESTRIP draw |
| --- | ---: | ---: | ---: |
| 0, cold minimap | 18838 | 9527 | 10 |
| 1, warm minimap | 14852 | 7047 | 10 |
| Всего | 33690 | 16574 | 20 |

10 фактических FVF: 2, 4, 68, 258, 260, 274, 322, 338, 530, 578.
Четыре snapshots (по два на сеанс), суммарно 20 line draw, отвергнуты одной
причиной. Остальные 16570 записей приняты native state validators. Это не
доказательство resident images, корректности vertex bytes, upload budget,
shader output, queue submission или всей последующей игры.

Raw snapshots сохраняют SDK MULTISAMPLEANTIALIAS=TRUE. При проверке native
семантики это значение переводится в 0 только для фактически single-sample
target: [Microsoft описывает его как не влияющее на такой target](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3drenderstatetype).
Другие render guards не отключаются, multisampled target этим не принимается.

## Точный source-confirmed блокер первого кадра

Return addresses разрешены через linker map с поправкой ASLR:
CIFaceList::Render → CIFaceElement::Render → CMinimap::Draw
→ map_device::DrawPrimitiveUP → native ValidateDraw(lines=true).
В оригинальном MatrixMinimap.cpp:871 выполняется
DrawPrimitiveUP(D3DPT_LINESTRIP, 4, campts, sizeof(D3DXVECTOR4)).
campts[4]=campts[0]; это замкнутая рамка текущего обзора камеры.

Параметры capture: topology=3, FVF=4 (XYZRHW), LASTPIXEL=1,
viewport=(13,442,145,145,0,1). Production NativeRenderState::ValidateDraw
отвергает `line rasterization/LASTPIXEL`; DekoDevice::Record вызывает эту же
проверку перед командами GPU. Следовательно, при достижении этой операции
native первый кадр остановится. Это не наблюдение запуска на Switch: IPC,
allocation, decode/upload или иной ресурс могут отказать раньше.

Следующий кодовый этап — настоящая line rasterization с правилами original
LASTPIXEL, viewport/clip и interpolation. Наличие DkPrimitive_LineStrip и
setLineWidth в SDK не доказывает совпадения D3D9 coverage/endpoints. Просто
удалить guard или пропустить рамку было бы подменой поведения; этого не сделано.
Нужны адресные HAL/native shader comparisons, включая реальную рамку миникарты.

## Раздельные результаты

| Среда | Результат |
| --- | --- |
| WINDOWS HAL | ORIGINAL GAME RUNTIME PASS, 2 полных сеанса. NATIVE STATE FAIL: два rejected snapshots на сеанс, итог exit 10. Frame PNG сохранены. |
| HOST ASan/UBSan | BASELINE REPRODUCED: все 33690 записанных draw / 16574 snapshots проиграны production validators, четыре line snapshots отвергнуты; exit 10. Диагностик ASan/UBSan нет. Это не функциональный PASS line backend. |
| ARM64/QEMU | Тот же baseline и тот же отказ, exit 10. Не Horizon/deko3d execution. |
| COMPILE/LINK | Новая полная SDK native-entry ELF, 0 strong undefined; 183 reachable own inputs, 255 generated original source files совпадают с E2E-5.35. Это не runtime. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Реальные native Init, карта и кадр не исполнены. |

Первоначальный capture, где failure callback ещё приводил к повторному cleanup
после Deinit, сохранён в initial-failure. Финальный harness разделяет HAL
result и native rejection, завершает оба сеанса штатно и оставляет exit 10.
HOST/QEMU воспроизводят отказ без этого harness cleanup побочного эффекта.

Артефакты: build/matrixgame/e2e536-actual-draw/{windows,host,arm64,sdk}.
Windows: draw-states-0/1.bin, state-summary.json, caller-resolution.json,
map-frame-0/1.png, actual-draw.map, runtime.log. Профили содержат команды,
exit codes и hashes. [Сводный audit](matrixgame-e2e536-actual-draw-evidence.json).

Открыты прежние два E2E-5.34 raw HAL state-block retry FAIL; это другой сценарий.
P0, GI repeating-text causality, manual-publish ENOENT и старые Switch hangs
не закрыты. Upstream, NRO/frozen, ресурсы и сейвы не менялись; commit/push нет.
