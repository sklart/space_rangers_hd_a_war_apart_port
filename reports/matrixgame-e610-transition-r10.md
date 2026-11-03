# MatrixGame E610 r10 — authored transition для native разрешения

Основание: [три r9 запуска](matrixgame-e610-r9-hardware-20261103.md). Все по
фактическим SHA относятся к r9, Init 28.2–28.5 с, средний Draw 0.562–0.567 с,
но 154/155/211 original frames остаются transition и истекает deadline.

## Воспроизведённая причина

Закреплённый CFG UTF-16 имеет layouts только 1024.768 (7 quad) и 800.600
(6 quad). Native extent 1280x720 не имеет authored блока. Исходный BuildTexture
возвращается пустым; RenderToPrimaryScreen всё равно ставит flag; original Takt
не может снять его при нулевом geom count. CPU negative control воспроизводит
это даже за 500 шагов по 100 мс. Это defect указанного в manifest source/CFG,
а не непосредственно прочитанный m_GeomCnt устройства: r9 не писал этот field
и runtime CFG SHA. Сценарий согласуется с журналами, новые metadata дают
проверяемое подтверждение в r10. Пользовательские данные не модифицируются.

## Изменение

ResolveGeometry оставляет любой точный authored layout без изменения; если
его нет, выбирает реально существующий 1024.768, затем 800.600. Координаты X/Y
и скорости масштабируются к target extent. Native screenshot/UV/POT bitmap,
CaptureLoadingImage, BC1 LoadFromBitmap, draw calls и original Takt сохранены.
RenderToPrimaryScreen и алгоритм удаления ушедших за экран quad не меняются.
Нет forced flag reset, manufactured frame или автоматического PASS. Empty/
missing geometry даёт exception до публикации анимации. Normalized layout и
длительность сохраняются; при exact resolution geom bytes совпадают.

Clear освобождает m_Geom и при m_Tex==null: реальная capture/parse ошибка до
публикации texture больше не оставляет частично созданный geometry owner.
Для нормального завершения original Takt вызывает тот же Clear. Cleanup
идемпотентен. Добавлены metadata source/target extent и geom count. Это правка
native generated overlay; upstream и CFG, игровые resources, saves/frozen не
меняются. Относительно r9 изменён только generated OriginalTransition.cpp;
SDK gate требует ровно прежние две texture правки плюс этот файл vs E601.

## Адресные проверки

Windows /O2, host /O2 ASan/UBSan/LSan, ARM64/QEMU /O2 UBSan — PASS.
Тест использует обе настоящие CFG layouts, извлечённые original Takt и Clear
(candidate + frozen baseline), шесть extents: 1024x768, 800x600, 640x480,
1280x720, 1920x1080, 1023x777. Exact layouts побайтово сохранены; остальные
масштабируются. Никакой layout 640x480 не выдумывается: это target case.
Independent Python float32 oracle совпал с каждым шагом/geom count/flag всех
шести traces. На 1280x720 original Takt завершает authored animation за 10
100-мс шагов CPU model. Это не hardware timing или GPU/capture proof.
Отрицательные controls: empty geometry stuck 500 ticks; original Clear при
отсутствии texture оставляет geometry; новый Clear освобождает один раз;
missing/empty resource tables и invalid extent отвергаются. Setup parser
сначала уточнялся под UTF-16, два layout и отдельный matcher координат/скорости;
прошедшие финальные прогоны находятся в */5/, прежние попытки сохранены.
Большие Windows боевые сценарии и неизменённые full CPU suites не повторялись.

## Граница

R10 HARDWARE PENDING. Реальный capture/texture upload/render fallback на Switch
ещё не выполнен; first map frame/four frames, физические HID/звук и FPS не
приняты заранее. O2/assertions/debug SDK, pool, sequence, Init handoff и критерии
smoke сохранены. Десятки тысяч pool misses и SDK allocation cost остаются
измеренной целью дальнейшей оптимизации, но не смешиваются с исправлением
перехода. Campaign/Gov/победа/возврат Galaxy не охвачены standalone entry.
[Инструкция](../docs/matrixgame-e610-transition-r10-readme.ru.md).

## Проверенная доставка r10

Source commit: `dd7f6714872d1826bc35f896ee4975bc104f9ea8`; build ID `dd7f6714872d-507a9538a658856b38ba`.
ZIP: `dist\matrixgame-e610-switch-test-r10-transition.zip` (10159587 байт).
ZIP SHA256: `204c5492005f753a82f18eecfc19ce6ac00427d9fbd62e8a2cc383bfdf660301`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| matrixgame-e610-quick-smoke.nro | `6705be9825bf162127b754937a26fc8b839c9a80ce56c00580fc23d6f6cb0270` | `ea942dbc322145425d1030f7fa78c58ffe2eefee` |
| matrixgame-e610-interactive.nro | `e52a885a9eca09706aa0e3d9ba26faafda8e2d2dafa296f25d2a5e3189b4e119` | `ec8c336c097eb05b013dbf9870562c0d522accb5` |

Итог: **SOFTWARE PASS / R10 HARDWARE PENDING**. Две NRO успешно compile/link,
strong undefined=0, structure/font/shader/build-ID/title checks PASS. Compiler
provenance: 321 configured C/C++ commands, -O2/-UNDEBUG, без fast-math или
отключения exceptions/assertions. Map/nm: 23 настоящих debug deko3dd objects,
0 deko3d. 699 own-source SHA совпали. Generated OriginalTransition.cpp в
SDK побайтово совпал с проверенным CPU preview; original Takt не изменён.
Относительно r9 изменён ровно этот generated file; vs E601 — прежние две
texture правки и он. Проверка 206 защищённых прежних файлов/пакетов PASS;
основная NRO, ресурсы/saves/frozen/upstream сохранены.
[Полное evidence](matrixgame-e610-transition-r10-evidence.json).

Копирование: только две NRO из ZIP в `/switch/matrixgame-e610-r10-test/`.
В hbmenu MatrixGame r10 Quick Smoke, затем MatrixGame r10 Interactive.
DATA/CFG не перезаписывать. Ожидаемые новые records: B.transition.geometry,
source/target dimensions и geometry_count. При завершении original transition
проверять настоящий first map frame/original-first.bgra и минимум 4 presents.
Interactive: до 120 с после Init, штатный выход Minus+Plus. Передать два новых
run-каталога и наблюдения об изображении, скорости, HID и звуке.
Новые аппаратные PASS не объявляются до этих результатов; дальнейшая
разработка ожидает следующего аппаратного профиля.
