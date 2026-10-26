# MatrixGame E2E-5.28 — original minimap lifetime/cache cleanup

Программно подключено exception-safe владение original minimap ресурсами:
временные texture/surface handles, scene, target/depth/viewport, locks и временные
флаги. Сохранены исходные rendering loops и PNG/cache алгоритмы. Full native
Init, первая карта и кадр на Switch остаются PARTIAL / HARDWARE_PENDING.

## Итоговые изменения

- minimap_lifetime.hpp: TextureOwner, rollback незавершённого backup slot,
  восстановление нужного map flag без перезаписи остальных битов и rollback
  shadow options. Владельцы не копируются и освобождают настоящие handles.
- offscreen::SurfaceLockScope выполняет настоящий Lock/Unlock. При исключении
  PNG writer bitmap views уничтожаются перед unlock; surfaces и texture живы
  до его завершения. Ошибка явного unlock допускает retry; ошибка cleanup
  сообщается, а не превращается в успешный GPU результат.
- RenderBackground и RenderObjectToBackground используют существующие настоящие
  TargetScope/SceneScope. При ошибке scene завершается до восстановления
  target/depth/viewport. Сохраняется nullptr depth. Нормальные original
  operations/порядок draw не удалены; explicit restore проверяет результат.
- Временные surfaces обоих render путей и cache/store/recreate защищены RAII.
  Успешные warm-cache и cold-render branches передают texture original CTexture
  и снимают временное владение. Cache save берёт уже принятую texture через
  m_Texture->DX(), поэтому не обращается к очищенному temporary owner.
- StoreTexture удаляет незавершённый system backup после исключения import/copy.
  Нормальный backup остаётся у CMinimap. Исходная HRESULT failure ветка
  really_lost сохраняется и уничтожает texture так же, как оригинал.
- RestoreTexture удерживает system backup до успешного UpdateTexture, создания
  настоящего CTexture и передачи ему GPU texture. При update/factory exception
  нет утечки временной GPU texture, backup можно повторно использовать.
  Исходная Create HRESULT failure ветка, которая отбрасывает backup, сохранена.
- Required depth/transfer/readback calls проверяются и в release. ASSERT_DX
  больше не игнорирует их отрицательный результат при ASSERT_OFF.
- Исходные PNG размеры, RGB/BGRA преобразование, texture formats, passes,
  terrain/water/object algorithms и original geometry не заменены.
  Из 252 generated files изменён только MatrixMinimap.cpp; внутри него изменены
  ровно StoreTexture, RestoreTexture, LoadMinimapFromCache, StoreMinimapToCache,
  RenderBackground и RenderObjectToBackground. Остальные методы побайтно прежние.

## Что действительно исполнилось

| Gate | Результат и граница |
| --- | --- |
| HOST ASan/UBSan | Настоящие prepared original StoreTexture/RestoreTexture/private PNG save/load + настоящий CTexture/cache primitives/PNG/CFile/native token coordinators. Проверены adoption, transfer OOM, import exception, retry, исходная allocation-failure discard ветка, отсутствие texture/surface/registry leaks. PASS. GPU upload/readback driver явно CPU model; Init/отрисовка карты не имитировались как успех. |
| HOST ASan/UBSan scopes | Production TargetScope/SceneScope/SurfaceLockScope/flag/shadow-option rollback; исключение внутри scene, restore failure/retry, unlock failure/retry, удерживаемые вторым owner primary surfaces. PASS. |
| ARM64/QEMU | Те же original cache/PNG и scope contracts. PASS. Обычный ARM64 CPU, без Switch GPU runtime. |
| Независимый PNG oracle | PNG CRC, decompression, filters и все RGB pixels 512×512 проверяются Python, независимо от original CBitmap. HOST/ARM64 artifacts одинаковы, соответствуют BGRA driver input. |
| WINDOWS HAL | Полные original Init/RobotPreload/stor.Load/PrepareMap/Draw/Deinit дважды. Подтверждены cold-render, store, warm-load. Actual SDK draws=1008, indexed=1006, primitives=111812 каждый кадр. Device/window/gamma cleanup и настоящий Reset/restore проходят. |
| WINDOWS pixels/cache | Оба 800×600 кадра сохранили hash f6f0d25c1e7b2a194f6523f8c96b9a74f7b7385525e5bb5cf7696de7f09a52bb. Original cache PNG 512×512 сохранил hash bd6fb237804767bce7ae0483a52a31ea2a679a294c570e0066a2a109d06b0af5. |
| COMPILE/LINK | Полный devkitA64 native-entry ELF, 178 link inputs, 0 strong undefined. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. Native Init/карта/первый кадр/боевое управление не исполнены. |

ELF: build/matrixgame/e2e528-minimap/sdk/matrixgame-native-entry.elf

SHA256: bf63a69b05feb0e17489c5532b3a08aaa06c9684f4ab62d781aea1ecd7e49129

CPU gradient PNG SHA256:
22ba66d73ea84b6cf560863f89883a86b4b0836d2876480500aa1f09fd91c44c

## Test fidelity и воспроизводимость

CPU cache test извлекает пять prepared original CMinimap методов побайтно.
В копию настоящего заголовка добавлен только test friend; поля/размеры/алгоритмы
не подменяются. Cache primitives также извлечены из настоящего CacheNative.cpp;
используется исходный logger_type lgr{std::clog}. Это предотвращает случайную
линковку старого полного игрового архива со вторым Minimap TU в ASan тесте.
Минимальная CPU цель и полные Windows/SDK цели остаются отдельными.

Компиляция адресных тестов сохраняет individual objects и GCC dependency hashes:
повторное использование требует совпадения команды, headers/sources и object
hash. Runtime выполняется заново. Не используются allow-multiple-definition,
пустые callbacks или фиктивное создание GPU ресурсов.

Команды: tools/matrixgame_minimap_checks.py prepare / host / arm64 / sdk;
tools/matrixgame_minimap_scope_checks.py host / arm64;
tools/matrixgame_minimap_windows.py; tools/matrixgame_minimap_audit.py.
Commands, source/object hashes и logs — build/matrixgame/e2e528-minimap;
итог — [evidence](matrixgame-e2e528-minimap-evidence.json).

## Следующий настоящий контракт: StoredStateBackend

Ветка CMatrixMap::ReleasePoolDefaultResources вызывает
CDeviceState(map_device::StateBackend()). DekoDevice ещё не реализует этот
интерфейс, поэтому остаётся точный guard:
**map stored device state backend unavailable**.
Это release/reset blocker; первый фактический отказ полного native Init без
аппаратного исполнения пока НЕ УСТАНОВЛЕН.

Выполнена [инвентаризация](matrixgame-e2e528-stored-state-inventory.json) по
original DeviceState.cpp, native setters и локальному официальному Windows SDK:

- 103 render registers; native setter route существует только для 24, часть
  значений ограничена. Raw register map изначально заполнен лишь для 11.
- 13 sampler registers на 8 slots. Отсутствуют BORDERCOLOR, MAXMIPLEVEL,
  SRGBTEXTURE, ELEMENTINDEX и DMAPOFFSET.
- Требуются raw light/material values, enable state и Type=0 для отсутствующего
  light в original restore алгоритме; native getter adapter ещё не подключён.
- Original ordinal НЕ равен numeric SDK register ID. Например, ordinal 1 —
  FILLMODE с numeric ID 8; сейчас такого native setter/raw initial entry нет.
- Existing NativeStateStore::Capture удерживает textures/VB/IB и authoritative
  renderer state; это другой контракт, чем scalar-only original CDeviceState.
  Его нельзя выдать за original StoreState/RestoreState или заменить нулями
  отсутствующие getters. Это также могло бы удерживать default resources при Reset.

Original CDeviceState принадлежит CMatrixMap, выделяется HNew(g_MatrixHeap),
восстанавливается/удаляется в CreatePoolDefaultResources или удаляется с картой.
Следующий шаг — настоящие native register/sampler/light/material capture и
restore, затем проверка full native Init/карты/кадра и дальнейшей игры.
Инвентаризация — SOURCE ANALYSIS ONLY, не закрытый stored-state backend.

## Защищённая область

Upstream, основная NRO, frozen комплекты, игровые resources/robots.pkg/CFG,
пользовательские сейвы и аппаратные входы сохраняются и проверяются hash audit.
NRO не создавалась; commit/push не выполнялись. Новых Switch hardware tests нет.
P0, причина повторяющегося текста/GI, manual-publish ENOENT и исторические
зависания Switch остаются OPEN; эти cleanup/tests их не закрывают.
