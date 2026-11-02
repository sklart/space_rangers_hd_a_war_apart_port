# MatrixGame E610 r5: аппаратные crash reports 02.11.2026

**HARDWARE FAIL: fatal SDK memblock creation при отрисовке миникарты.**
Исследован C:/pack111.zip, 200701 байт, SHA256
`4d09ca7f3000fd86c7652bfd6eabed6d98be98605d3113b98105da109b8da754`.
Две NRO подтверждены self-SHA и Module Id crash reports, совпадающими с
matching ELF/NRO r5. Build ID исходников: `937eccd13fc2-7a9c102056ba1fc5c8a2`;
source commit: `937eccd13fc2b8911863459ae6e0ee734c31adca`.
Старые отчёты другого приложения в этом ZIP к MatrixGame не отнесены.

| Run | Режим | Журнал | Crash report |
| --- | --- | --- | --- |
| run-1793627904004033-0 | Quick | 4644 records, обрыв без cleanup | 01793627967_0100ba4026e94000.log |
| run-1793627999004117-0 | Interactive | 4644 records, обрыв без cleanup | 01793628063_0100ba4026e94000.log |

## Подтверждённый stack

Оба отчёта содержат Result 0x367 (2359-0001), Type User Break.
Module Id Quick: `1f472634190be2fb310d77b3c1558e64a7630828000000000000000000000000`;
Interactive: `c202a2ae22cfc08dbda5b245727870ed066de427000000000000000000000000`.
ELF SHA сверены с manifest до символизации. Return addresses символизированы
по module-relative offsets минус 4; оба стека показывают одинаковую цепочку:

```text
CGame::Init → CMatrixMap::CreatePoolDefaultResources
→ CMinimap::RenderBackground → DrawLandscape → CMatrixMapGroup::Draw
→ NativeDrawIndexed → DekoDevice::Impl::Record
→ DekoCombinerProgram::Prepare → dkMemBlockCreate
→ dk::detail::RaiseError → diagAbortWithResult → svcBreak
```

Следовательно, прежняя проверка binding больше не останавливает выполнение:
от достигнутого indexed draw управление дошло до создания material packet.
Однако это не PASS всей миникарты и не завершённый Init. В журнале нет
B.complete_original_Init, original map frames, process_exit или journal_close.
Прямой SDK abort объясняет отсутствие C++ exception и обычного cleanup в журнале.

## Значение ошибки и граница вывода

Фактическая RaiseError в matching ELF кодирует MAKERESULT(359, result).
0x367 означает DkResult=1, то есть **Fail** по установленному deko3d.h;
OutOfMemory имеет другое значение. Доказан отказ инициализации MemBlock,
а не конкретная нехватка CPU RAM, GPU RAM или NV object handles.

Fail может возвращаться из NV map/address-space операций. Низкоуровневый Result
этой операции здесь не записан, поэтому точная причина не объявляется найденной.
В нашем material_program.cpp Prepare на каждый packet создаёт отдельные GPU
uniform blocks. Управление их количеством/временем жизни и пределами batch —
предмет следующего адресного исправления/диагностики, а не основание отключать
SDK checks или подменять draw успешным результатом.

Для пояснения error path сверена официальная реализация
[MemBlock](https://github.com/devkitPro/deko3d/blob/master/source/dk_memblock.cpp)
и [RaiseError](https://github.com/devkitPro/deko3d/blob/master/source/dk_device.cpp).
Эти master-ссылки не подменяют версию SDK: вывод о данном crash подтверждается
локальным deko3d 0.5.0 header, совпадающим ELF, objdump и addr2line.

## Выявленный пробел build evidence

Metadata декларировала deko_library=deko3dd, но hosted-diagnostic.map показывает
23 уникальных SDK object members из **libdeko3d.a** и ни одного из libdeko3dd.a.
Дизассемблирование RaiseError подтверждает release-ветку с прямым
diagAbortWithResult, а не diagnostic callback. Поэтому отсутствие sdk_error
записи не означает отсутствие ошибки SDK.

Прежние compile/link/package PASS подтверждали формальную структуру артефактов,
но не правильность выбранного SDK-архива. Этот отдельный gate был пропущен.
Следующая сборка должна проверять фактическое происхождение SDK symbols по map,
устраняя противоречие между metadata и link, и проверять реальный diagnostic path.
Исторические пакеты/отчёты не перезаписываются; это уточнение их границ.

## Сохранённые результаты

[Машинное evidence](matrixgame-e610-r5-hardware-20261102-evidence.json) содержит
SHA, соответствие modules/NRO, символизацию обоих стеков и фактические SDK objects.
Неизменённые журналы, три smoke frames на запуск, два соответствующих crash logs,
фото и символизированные стеки сохранены в
build/matrixgame/e610-r5-hardware-20261102/. Другие игровые crash reports
не включены в этот анализ. Исходный ZIP сохранён без изменений.

GPU smoke, ApplyVideoParams и identity orientation check прошли; независимое
сравнение smoke/orientation-smoke даёт побайтовое равенство. Физическая ориентация
изображения самой игры не проверена. Полный Init, карта, игровой HID/GPU, бой,
Campaign/Gov и победа остаются открытыми. Нормальное освобождение в этих двух
запусках не подтверждено из-за fatal abort. Исходники и NRO при анализе не менялись.
