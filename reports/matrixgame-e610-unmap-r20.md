# MatrixGame E610 r20 — covered buffer publication

Следующий подтверждённый CPU расход после r19: Resource::Unmap повторно
push/sort/merge уже покрытого диапазона при normal/no-overwrite updates.
Добавлена проверка покрытия первым инициализированным интервалом с прежними
unlock/version++. Остальные интервалы и расширения идут по прежнему пути.
Discard очищает initialized_ в Map и не может попасть в этот fast path.
Map, reserve до выдачи памяти, version overflow, guards, Storage, refs и
порядок upload/render не меняются. Нового кеша и состояния нет.
Оригинальные клиенты через big_buffer_bridge.hpp по-прежнему вызывают настоящий
Map/Unmap. Выигрыш относится только к повторным covered updates, не к первому
заполнению/Discard. Число таких операций на Switch пока неизвестно.

CPU probe сравнивает frozen r19 a2f87f3a40d72153a10b76a0fb4193535c935a9f
и текущий полный buffer_resources.cpp с независимой побайтовой моделью.
1033 обновления: disjoint/holes/adjacent/merge, subsets первого/второго
интервала, Discard/NoOverwrite, data/version, guards/errors, OOM-before-exposure,
allocation-free Unmap и retained binding. 4499748 coverage queries.
Host ASan/UBSan/LSan, ARM64/QEMU UBSan, Windows O2: semantic PASS,
checksum 1775817787135797556 одинаков на всех платформах и у frozen/current.
Первая версия fixture не привязывала Scope перед Restore; frozen тоже падал.
Исправлен тест, assertions сохранены; failing evidence в checks */1.

Предыдущий bitmap proxy кандидат дал менее 1% и отклонён. Его исходники,
результат и решение сохранены в build/matrixgame/e610-bitmap-r20-checks.
Production indexed body побайтово эквивалентен r19 после нормализации EOL.

Benchmark: 10 млн Map/write/Unmap на full initialized buffer и 10 млн на
частично initialized buffer, ABBAAB, host O2 UNDEBUG без sanitizers/LTO.
Это synthetic CPU publication, не frame/FPS/GPU/Init. Коэффициенты r10–r20
не перемножаются. Числа итогового замера, SDK/package и SHA добавлены ниже.

Большие Windows боевые сценарии не повторяются. Ресурсы/saves/upstream/frozen,
основная NRO и прежние пакеты сохраняются. HARDWARE PENDING. Standalone entry
не охватывает Campaign/Gov, естественную победу и возврат Galaxy.
[Два ручных запуска](../docs/matrixgame-e610-unmap-r20-readme.ru.md).
