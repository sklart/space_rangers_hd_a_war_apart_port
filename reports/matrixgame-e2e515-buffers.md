# MatrixGame: завершение BigVB/BigIB

20.10.2026. Локальный коммит исходников
`7d3b4f5566e41d4d1b29fa8f0770e22aa2d06ee5`. Push не выполнялся.

Оригинальные алгоритмы BigVB/BigIB сохранены через проверяемый overlay.
Ресурсы теперь имеют отдельные vertex/index типы, CPU staging, map/unmap,
контроль инициализированных диапазонов, версии записи и владение привязанным
хранилищем. Это не полный D3DControl и не доказательство готовности GPU.

Исправлены освобождение массивов/буферов, публикация count до realloc и
noneed-count после инвалидирования ресурса. До исправления ASan находил
160 байт утечек в шести allocations; промежуточная очистка выявила OOB из-за
count. Конечные шесть allocation-fault случаев PASS без ASan/UBSan/leak ошибок.
После первоначального allocation failure владелец уничтожается; повторная
инициализация того же объекта не обещается. Восстановление существующего
источника после growth realloc failure проверено в двух случаях.

Повторный CTest: navigation, lifecycle, big-buffer-contract,
buffer-allocation-fault — **4/4 PASS** под санитайзерами. Четыре transcript
повторно сверены на native host, оригинальном Windows x86 D3D9 и ARM64/QEMU:

| Transcript | Байты | SHA-256 на всех трёх платформах |
|---|---:|---|
| Контрольный | 15642 | `2e9774da1b3e95588892c3a0436994360733e946ac99c8df0e2f3f67123aa891` |
| dagger00 | 4076 | `282044ebf82ebc4737f7e49f96d6647d45f6782709599e17012d2d6b9fc0996b` |
| wood05 | 7648 | `b7d1f550f4aa0b0b823778398fdd566c8762469382d175c0eac9dd3de07f1c08` |
| Terron | 7906256 | `2c2a0040515d66da37ed8d801b1e8e4029882886a0ddebc3e498c00f5debb800` |

Windows reference изменяет только include провайдера, используя настоящий D3D9.
Реальные VO прочитаны через CStorage: 104 кадра / 338 диапазонов. Неопределённый
D3D9 padding не сравнивается. Terron имеет 108008 вершин; предел deko mesh
увеличен с 65536 до 262144 — исходные 8 МиБ BigVB при stride 32. u16 индексы и
base vertex сохранены. Исходные модели и captures остаются только в ignored build/.

SDK CPU ELF COMPILE/LINK PASS: SHA
`2a4817e22648d32f2e1b0274ce5510d226ae83bc192d15485dc41b40c96e5a7f`,
Build ID `4b93cd3280094caef912fa6a6e44db757d56cc7b`.
Deko adapter ELF COMPILE/LINK PASS: SHA
`4a9c1ea14ed1524a3e605e60c7a3991bc3a43d8458bcaf0e70f1372c445cc7bd`.
В нём настоящие upload/indexed draw/readback вызовы. Switch GPU execution pending.
Отдельная аппаратная smoke-обёртка имеет собственные SHA, если подготовлена.

Полный compile inventory остаётся 13 PASS / 94 FAIL из 107 cpp; manifest SHA
`757d50e653307b136ff615d41cca8d8c44a71e182785f0057be69ec97ada87a6`.
Полные CVectorObject/D3DControl/device, CGame Init/Run, материалы/тени/terrain
и настоящий бой остаются OPEN. Эти экспериментальные компоненты не входят в
основной NRO. Следующий большой этап отложен до первого аппаратного прогона игры.
