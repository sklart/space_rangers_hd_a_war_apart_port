# E2E-5.19 — полный terrain TU и воспроизведённый double-free

23.10.2026, продолжение `69f57f6`, master, без push.

## Подтверждённая причина и исправление

Оригинальный CTerSurface::ClearSurfaces вызывает деструкторы поверхностей и
освобождает m_Surfaces/m_SurfacesDraw, но оставляет оба указателя и
m_SurfacesCnt. На настоящем g_MatrixHeap выполнены исходные AllocSurfaces(3),
ClearSurfaces(), ClearSurfaces(). После первого Clear IsSurfacesPresent всё
ещё true; второй Clear воспроизводит **AddressSanitizer: double-free** в
Base::CHeap::Free. Baseline exit1 и обе allocation/free stacks сохранены.

После исходного освобождения теперь обнуляются оба указателя, счётчик и
пустой диапазон рисования [0,0). Деструкторы и освобождения не заменены и
не пропущены. Это устраняет воспроизведённое повторное освобождение и позволяет
новый цикл выделения. Исправление внесено generated overlay, pinned upstream
не изменён.

## Интеграция исходного движка

В общий native-cache включён **полный MatrixTerSurface.cpp**, а не отдельная
имитация Clear. Сохранены оригинальные Load/LoadM, BeforeDraw, сортировка,
Draw/DrawAll, ctor/dtor и работа BigVB/BigIB. Graphics границы SetFVF,
SetRenderState, world transform и indexed draw направлены в существующий
typed platform/map backend. Полный header получил только недостающий forward
declaration CTextureManaged; поля и layout класса не менялись.

Map backend дополнен реальным Windows DrawIndexedPrimitive forwarding с
полным исходным tuple: primitive, signed base vertex, minimum vertex,
vertex count, start index, primitive count. Без реального backend — явный
Unsupported; успешный draw не синтезируется. Новые serialized IDs сверены
compile-time с настоящим Windows SDK. HRESULT/ASSERT_OFF ветви следуют
ранее принятому map adapter contract.

Пять исходных terrain statics уже принадлежат GameStaticResources.cpp;
они не дублировались. Остальные statics остаются в полном terrain TU.

## Реальные результаты

- Общий host ASan/UBSan/leak-check: exit0, два bootstrap-сеанса. В каждом
  AllocSurfaces(3), повторный Clear, AllocSurfaces(2), повторный Clear;
  IsSurfacesPresent возвращается в false. Исполнены настоящие ctor/dtor и
  heap allocations/frees. После проверки продолжается прежний bootstrap.
- ARM64/QEMU: тот же общий bootstrap exit0 и те же lifecycle/reentry проверки.
- Новый indexed draw путь без GPU явно отказывает и сохраняет настоящих
  CPU owners в существующей проверке device boundaries.
- devkitA64: COMPILE/LINK PASS отдельного ELF, strong undefined=0; сохранены
  настоящие AllocSurfaces/ClearSurfaces и indexed adapter symbols. SHA-256
  `797ddb128dae8b5e30e39e2dfb65b276ae19cffd38dd12d7653fc163cf709e5c`.
  NRO не собиралась, runtime на Switch HARDWARE PENDING.
- MSVC x86: полный terrain TU и настоящая Windows-ветка map backend COMPILE
  PASS; SDK constants contract COMPILE PASS. Terrain Windows runtime NOT RUN.
- Свежий CMake configure/generate PASS: полный terrain входит в cache и
  engine audit target, реальные recipes имеют function/data sections.
  Полная повторная сборка CMake не проводилась.

На host остаются прежние исключения Console ASan globals и Logic/Map vptr;
для terrain новых sanitizer исключений не добавлялось. Завершённые suites
CList, BigVB/BigIB и RoadNetwork не повторялись.

## Точная достигнутая граница

Проверены настоящие массивы **ещё не загруженных** terrain objects. Нет
загрузки геометрии, текстур, зарегистрированных vertex/index sources, GPU
submission или проверки освобождения живых GPU buffers. Не проверялся отказ
второго выделения AllocSurfaces. Это не доказательство полного terrain load
или render: staging binding и FILLMODE register8 пока не поддержаны данным
Windows provider contract, исходный terrain Draw не исполнялся.

Настоящий CMatrixMapLogic ctor/dtor force-retain link теперь требует
**18** уникальных missing symbols вместо19: CTerSurface::ClearSurfaces снят.
Остались реальные world object/side/water/minimap/effects/group/skin/hint/UI
dependencies; точный список — в evidence. Full map owner LINK FAIL,
CGame::Init, RunGameLoop, CFormMatrixGame и бой NOT EXECUTED. Общий Windows
bootstrap в этом шаге не перелинковывался; прежний FAIL150 — исторический gate.

Все242 engine-manifest файла в двух views совпали; отдельно проверены66 core
outputs в трёх views. Из engine sources изменились только MatrixTerSurface.cpp
и MatrixTerSurface.hpp. [Evidence](matrixgame-e2e519-terrain-evidence.json)
содержит source/object/binary hashes, команды, failed baselines и успешный
кандидат. build_dirty=true — проверки до локального коммита.

Повторно совпали13 доступных protected inputs,107 аппаратных файлов, frozen
main NRO/ELF и оба ZIP. G:/pack.zip недоступен. Сейвы и ресурсы не менялись.
Этот double-free найден в MatrixGame; он не объясняет аппаратные дефекты
основной игры. Intro raster owner, две исторические manual-publish ENOENT,
freeze карты и whole-console exit freeze остаются OPEN.
