# E2E-5.20: оригинальный Init и первый кадр настоящей карты

25.10.2026. **WINDOWS HAL RUNTIME PASS** для двух полных циклов
`CGame::Init → original form Draw → framebuffer readback → CGame::Deinit`.
Полный порт Switch и P0 основной игры этим не закрываются. Commit/push нет.

## Реальное исполнение

- SDK `Direct3DCreate9/CreateDevice(HAL)` успешен; использован software vertex
  processing на настоящем HAL, без NULLREF и без синтетической сцены.
- Тот же оригинальный robots.pkg/CFG и `desert1_3e.cmap`: map 276×285,
  groups 28×29. Оригинальные RobotPreload, storage Load, PrepareMap и оставшийся
  Init завершились, `OriginalGameReady()==true`, map опубликован.
- Обе инициализации реально изменили клиентскую область на 800×600; оригинальный
  ApplyVideoParams выполнил SDK Reset, согласовал backbuffer/depth и viewport.
- В каждом цикле полный исходный `CFormMatrixGame::Draw` вызвал BeforeDraw,
  DrawObjects, DrawLandscape, DrawLandscapeSurfaces, DrawWater, DrawShadows,
  DrawEffects. Логирование в начале исходных методов подтверждает исполнение.
- Через map backend зарегистрировано **1008 успешных SDK draws**, из них
  **1006 indexed**, **111812 primitives** на кадр. Это счётчик данного backend;
  дополнительные внутренние вызовы D3DX text/SDK могут в него не входить.
- После EndScene, до discard Present, настоящий GetRenderTargetData дал
  **800×600, 154038 различных RGB-цветов** в каждом из двух кадров.
  Снимок визуально проверен: карта, рельеф, вода, здания, роботы, тени и
  начальный dialog UI. Это не clear-only/triangle probe.
- Оригинальный Deinit освободил map/heap/skin/draw owners. Проверены успешный
  StateRestore, gamma, window rectangle, backbuffer и viewport 640×480,
  отсутствие исходно отсутствовавшего depth target, пустые state/timer leases.
  Второй вход также завершился успешно.

Снимки: `build/matrixgame/e2e520/windows/map-frame-0.png` и `map-frame-1.png`.
Точные хэши, команды, наблюдения и защищённые входы:
[evidence](matrixgame-e2e520-first-frame-evidence.json).

## Resize/Reset и владение

Первоначальный отказ был явным `Window::ResizeOuter` по умолчанию, без вызова
Win32. Оригинальный ApplyVideoParams ожидает SetWindowPos с компенсацией рамки,
затем Reset и SetViewport. Win32Window теперь действительно вызывает эти
операции, а также оригинальные fullscreen style/move операции. WindowAttachment
сохраняет и восстанавливает placement/style/exstyle/visibility.

D3D9Device разделяет Borrowed и явный Exclusive presentation access. Borrowed
устройство по-прежнему не разрешает Reset. Для принадлежащего driver устройства
reset-safe snapshot хранит доступный typed state скалярно, без удержания
stateblock; исходный baseline должен быть без программируемых shaders и bound
streams/indices, с owner backbuffer. DEFAULT texture references отвергаются.
Контракт не обещает безопасно сбрасывать произвольное устройство чужой игры.

Reset снимает bindings и реально вызывает SDK с исходными параметрами.
Восстановление presentation происходит после освобождения игровых ресурсов,
до возврата состояния хозяину. После неудачного Reset сохраняется возможность
повтора через разрешённые cooperative/Reset операции; неподдерживаемые Set/Get
на lost device не подменяют повторный Reset. Это соответствует
[контракту Microsoft Reset](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-reset).

Адресный SDK-тест подтвердил отказ Borrowed reset и нулевого размера окна.
Намеренно удержанный DEFAULT render target вызвал настоящий
`D3DERR_INVALIDCALL (8876086c)`; после Release повторный Reset успешен.
Это ожидаемый отрицательный тест, а не оставшийся дефект.
Дополнительно SDK GetStreamSource/GetIndices и CPU context подтвердили
согласованное bind/unbind настоящих vertex/index buffers и освобождение владельцев.

## Исправления, необходимые для оригинальной цепочки

1. **Staging → SDK.** Прежние BigVB/BigIB выбирали CPU staging, но projected
   shadow RenderMin доходил до SDK DrawIndexedPrimitive без соответствующих GPU
   bindings и давал AV. D3D9 provider теперь загружает настоящие buffers с
   сохранением layout/index width; cache использует Id/Version. Software vertex
   processing usage согласован с реальными creation flags. Draw проверяет
   initialized ranges, base/minimum, topology и переполнение до SDK-вызова.
   Явные provider bindings синхронизируют CPU context, поэтому water draw не
   использует оставшийся индексный буфер предыдущего прохода.
2. **Исходные VO/material callbacks.** На время полного Windows game owner
   привязывает NativeDrawSink к настоящему SDK draw. Исходные GetSkin/pass tables
   и material setup выполняются; ordinal/demo materials не используются.
3. **Цветовые текстуры стороны.** Два CACHE_CREATE_TEXTURE в оригинальном
   CreatePoolDefaultResources создавали portable CacheTexture и приводились к
   несовместимому original CTexture. Последующая MakeSolidTexture портила объект,
   что проявлялось AV в destructor стороны. Теперь оба вызова создают настоящий
   original CTexture через typed factory; два полного cleanup подтверждены.
4. **Первый кадр.** Полный исходный form Draw вынесен в отдельную функцию без
   изменения алгоритма; SDK/platform операции направлены в существующие backend
   boundaries. Единственное добавление в последовательность — readback observer
   между EndScene и Present. Исходный шрифт Rangers.ttf включён в resource EXE,
   исходный файл не менялся. Выборочные методы карты дополнены только логами.

## Матрица проверок

| Уровень | Что подтверждено |
| --- | --- |
| COMPILE Windows | Изменённые provider/owner/map/prepare/frame TU и оба driver TU, RC resource |
| LINK Windows | Полная original Init/frame closure с хэшированными повторно использованными libs; отдельный game target |
| HOST RUNTIME | Адресный staging contract под ASan/UBSan/leaks: published ranges, strips, base/minimum, overflow, u32, cleanup |
| ARM64/QEMU | Тот же staging contract runtime; полной GPU/Init цепочки на QEMU нет |
| devkitA64 COMPILE | Изменённые portable owner/map_device/map/prepare/frame TU |
| devkitA64 LINK | Адресный staging test ELF, strong undefined=0; не полный game ELF |
| WINDOWS HAL RUNTIME | Два original Init/frame/readback/Deinit, настоящий resize/reset и отрицательный Reset-тест |
| SWITCH HARDWARE | NOT RUN; прежний GPU alignment fix остаётся аппаратно неподтверждённым |

CMake configure/generate проверен; минимальный bootstrap и полный game target
разделены. Адресная сборка использовала прежние библиотеки, весь большой движок
повторно не пересобирался. Исторические Path/baseline и большие CPU-наборы не
повторялись: новый переносимый draw-контракт проверен отдельно.

Воспроизведение на этом рабочем дереве:

```text
python tools/matrixgame_e2e520_windows.py
python tools/matrixgame_e2e520_checks.py host
python tools/matrixgame_e2e520_checks.py arm64
python tools/matrixgame_e2e520_checks.py sdk
python tools/matrixgame_e2e520_audit.py
```

Host/ARM64 команды выполняются в WSL с установленными compiler/QEMU. Windows
runner использует сохранённые E2E-5.19 compile/link recipes и их библиотеки;
это воспроизведение локального кандидата, не чистая автономная release-сборка.

## Оставшаяся граница

Для критерия E2E-5.20 **блокера Init/первого кадра на Windows больше нет**.
Полный игровой цикл, взаимодействие, завершение боя, все режимы fullscreen/MSAA,
случайные потери устройства и общая визуальная совместимость не принимаются
по одному кадру. Их проверка впереди, как и полная интеграция Switch.

[Нативные Window/Device contracts](../docs/matrixgame-native-providers.md)
задают переход к libnx/deko3d без фиктивных D3D identities и механического
копирования Win32 API. Первый Switch барьер — полноценный typed native entry
и Device/display lifecycle; затем native uploads/materials/present и аппаратная
приёмка. Прежний standalone GPU smoke не доказал draw на Switch.

Основная игра и её P0 остаются OPEN: GI defect не объявляется причиной
повторяющегося текста, manual-publish ENOENT и Switch hangs не закрыты.
Frozen источники основной игры, основная NRO, старые комплекты, ресурсы и сейвы
сохраняются; новых аппаратных прогонов и push нет.
