# E2E-5.54 — один geometry/descriptor upload block на draw

В native Impl::Record геометрия, необязательные индексы, sampler descriptors
и image descriptors использовали три/четыре отдельных GPU memory blocks.
Добавлен NativeDrawUpload: один настоящий owned block с отдельными страницами
для тех же данных. На indexed draw стало три allocations вместо шести с учётом
двух неизменённых uniform blocks; на non-indexed — три вместо пяти.

Смещения областей остаются кратны DK_MEMBLOCK_ALIGNMENT=4096, что также
удовлетворяет SDK alignment 32 для descriptors и выравниванию индексов.
Размер блока равен сумме прежних rounded allocations. Прежнее выражение
batch budget, пределы draw/bytes, drain/submit/wait, ownership image versions,
uniforms, shaders, raster/lighting и порядок команд не изменены.

Блок принадлежит DrawPacket, проходит те же move/vector growth и освобождается
при прежнем ReleaseCompleted. Если создание packet не завершилось, SDK owner
освобождается при unwind до публикации команд draw. При queue loss действует
прежний порядок закрытия channel перед освобождением packet owners.

## Проверки

| Gate | Фактический результат |
| --- | --- |
| HOST ASan/UBSan | PASS. 88 комбинаций vertex/index bytes, включая пустые индексы и границы страницы. Геометрия и все восемь descriptor slots совпадают побайтно с прежней записью. Неиспользуемые image slots обнулены; padding не затронут. |
| ARM64/QEMU | PASS, тот же набор. |
| Allocation count | В CPU SDK endpoint model прежний извлечённый upload prefix запросил 341 блок, NativeDrawUpload — 88. Это 253 устранённых запроса; два uniform blocks в данном тесте не создаются и в production не менялись. |
| Failure/lifetime | Проверены отказы каждого прежнего allocation и нового allocation, null/undersized backing, нулевой vertex extent, overflow, недопустимые slots, moved-from access, move assignment и 257 packet owners с vector relocation. Все блоки освобождены. |
| COMPILE/LINK | Полный diagnostic devkitA64 ELF PASS; production Record использует NativeDrawUpload; strong undefined отсутствуют. |
| WINDOWS HAL | NOT RERUN: original D3D9 path, общие Init/Draw contracts, material program и shaders не менялись. Ранее сохранённые потоки используются только как счётчики. |
| SWITCH HARDWARE | NOT RUN / HARDWARE_PENDING. GPU image, FPS, первая карта и Present не доказаны. |

CPU test использует настоящий production NativeDrawUpload и реальные SDK
UniqueMemBlock wrappers. C endpoints хранения и адресов — явные byte-array
fixtures; они не выполняют GPU commands, shader sampling или original Init.
Прежний allocation/write prefix извлечён из сохранённого Impl::Record.
Доказательство неизменности остальных участков Record — обратная замена
оговорённых upload/address выражений и сравнение SHA-256 полного прежнего TU.

Для оценки количества запросов без нового большого Windows прогона прочитаны
два сохранённых frame-cost CSV E539: 92 кадра, 208886 всех draw, из них 175902
indexed. Арифметически geometry/descriptor часть для такого же потока требует
802560 отдельных блоков раньше и 208886 теперь (разница 593674); два uniform
blocks на draw сохраняются. Это расчёт по прежним Windows counts, не исполнение
потока на native GPU и не измерение времени или экономии VRAM. Суммарные bytes
на packet сохранены, а peak определяется прежним batch policy.

```powershell
python tools/matrixgame_draw_upload_checks.py host
python tools/matrixgame_draw_upload_checks.py arm64
python tools/matrixgame_actual_draw_checks.py sdk --output build/matrixgame/e2e554-draw-upload --diagnostic-source build/matrixgame/e2e543-journal/source
```

Команды/бинарные файлы/извлечённые inputs: `build/matrixgame/e2e554-draw-upload`.
Итоговый аудит: `matrixgame-e2e554-draw-upload-evidence.json`.

Просмотр texture residency не дал нового воспроизведённого дефекта; этот этап
уменьшает накладные расходы реального draw backend. Последняя аппаратная
граница остаётся run2 после BC1 upload, до draw/Present/original Init. E547–E554
на Switch ещё не выполнялись. Новая NRO не создана; commit/push нет. Защищённые
ресурсы/сейвы/frozen, оба diagnostic пакета и original generated inputs сохранены.
