# Аппаратный r9 — 03.11.2026

Архив `C:/logs55.zip`, SHA256 `eaf3bc7efbd9de29940ec1e3ef869385ee67a1ac03c16ced9b6ea55369960d85`.
Три запуска: два Quick Smoke, один Interactive. Self SHA/build ID совпали с
выпущенным r9, seq непрерывны. Во всех Init и GPU handoff завершены, queue
healthy, SDK warnings=0, SDK error/fatal отсутствует, journal_close присутствует.
Три выхода code 10: diagnostic 120-second cooperative deadline exceeded.
Ошибки попытки показать status screen после deadline вторичны.

| Параметр | Quick 1 | Quick 2 | Interactive |
| --- | ---: | ---: | ---: |
| Init, с | 28.238 | 28.478 | 28.473 |
| Minimap, с | 4.699 | 4.641 | 4.767 |
| Original Draw/Present | 155 | 154 | 211 |
| Средний Draw, с | 0.567 | 0.565 | 0.562 |
| Зачтённые map frames | 0 | 0 | 0 |

R8 Interactive: 1.608 с/Draw и 62.435 с Init. R9: 0.562 с/Draw (наблюдаемое
улучшение примерно 2.86x) и 28.473 с Init (около 2.19x). Это не изолированный
controlled A/B, но улучшение повторяется в двух Quick. Всё ещё медленно.
За весь Interactive run block_create=36.268 с, vertex_decode=11.682 с,
batch_wait=17.117 с, journal IO=13.098 с. Эти totals включают Init и cleanup;
части вложены в draw_cpu и не складываются. Pool hits=377194, misses=117086,
max live packets=64; 4.357 GB upload bytes — cumulative, не live-memory/OOM.

Все original frames классифицированы как transition. Original-first.bgra и
SaveResult отсутствуют, полноценный игровой smoke не пройден. Новых сведений
о физическом HID/звуке от пользователя нет; прежние наблюдения не переносятся.
Campaign/Gov и возврат Galaxy standalone entry не вызываются.

## Статически воспроизведённый отдельный blocker

В закреплённом `CFG/robots/data.txt` (UTF-16) только authored transition layouts
1024.768 и 800.600. Для runtime extent 1280x720 точного layout нет. Оригинальный
BuildTexture возвращается без geometry; RenderToPrimaryScreen затем безусловно
устанавливает MMFLAG_TRANSITION. Takt с m_GeomCnt=0 не входит в цикл и не снимает
флаг. На указанном в manifest CFG этот случай воспроизводится независимо от FPS.
R9 журнал не измеряет m_GeomCnt и не хеширует runtime CFG, поэтому конкретный
hardware field не объявляется непосредственно прочитанным. Это подтверждённый
source/resource defect, согласованный с симптомом трёх запусков. R10 добавляет
layout metadata и geometry_count для подтверждения на устройстве.

Ранее в commentary ошибочно упоминался готовый 640x480 layout: его в этом CFG
нет. Это target case для масштабирования, не третья authored source layout.
[Точные данные](matrixgame-e610-r9-hardware-20261103-evidence.json).
