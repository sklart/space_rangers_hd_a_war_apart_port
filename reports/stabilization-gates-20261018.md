# Критерии ТЗ: состояние проверок 18.10.2026

Это текущая карта оставшихся работ, не объявление готовности релиза.
Полная цель сохраняется: P0, локальные P1, одна clean NRO/ELF/provenance, CI,
аппаратные проверки и настоящая интеграция MatrixGame MG-0…MG-5.
Термины A1…A9/B/C/D соответствуют разделу 13 приложенного ТЗ.

| Gate | Состояние | Фактическое доказательство и остаток |
|---|---|---|
| A1, Introduction + optional music fault | HOST PASS | build/host-reclaim-lock-normal.json (SHA 11a5e0c9…3fb66e): 300 ходов, два heartbeat готового UI, bad_alloc настоящего TFileStreamEC, raw touch/callback, экраны 20/4 и 60+ кадров. Неподвижный аппаратный экран этим не объяснён. |
| A2, allocator/reserve/fragmentation/invalid sizes | PARTIAL | ASan/UBSan bounded package и настоящий TFileStreamEC OOM/late failure/cleanup PASS; Reserve pressure realloc before FAIL/fixed PASS и 1024 mixed alloc/realloc/free: host ASan/UBSan + ARM64/QEMU PASS; см. reserve-resize-20261018.md. Дополнительно исправлен нулевой ResidentBytes TCBufEC: native before FAIL/fixed PASS, 71 реальные загрузки учтены на 18250536 байт capacity (cache-buffer-accounting-20261018.md). Полная fragmentation/stress при аппаратном ресурсообороте остаётся открытой. |
| A3, cache readers/writers/lifecycle | PARTIAL | Настоящий EC_Cache: concurrent retry после pre/post-buffer bad_alloc, shared retain, event/buffer cleanup/trim PASS (native-cache-20261018.md). Interleaved package readers, stream IdleEvent/join/fd cleanup PASS; host injection TSan race воспроизведена и устранена. Reclaim ResidentBytes data race before TSan FAIL / fixed PASS: 200 loads +2441 parallel reclaim, см. reclaim-concurrency-20261018.md. Это не полный stress всех очередей cache/jobs и writer paths. |
| A4, pointer provenance/history | PARTIAL | Effective audit 380 units: 220 классифицированных findings, fixed_site_regression_failures=[], один REVIEW_NATIVE_SEMANTICS. Его PC field contract теперь сверён с оригинальным EXE: Date getter читает FlyToStar, setter пишет FlyDate; ARM64 treatment OPEN (ship-destination-native-20261018.md). Полный runtime ownership/provenance основных режимов и истории не доказан. |
| A5, arcade 60 frames + geometry/state | PARTIAL | Текущий host SHA 11a5e0c9…3fb66e: build/host-reclaim-lock-arcade.json, 40.76 с, screen 21, 60+ кадров, crash=false/timeout=false; 3 млн geometry pairs, pixel/stored coordinate/rounding controls PASS. Эквивалентность всей боевой state sequence и новая Switch производительность не доказаны. |
| A6, generation world hash/RNG/3 timings | CURRENT HOST EQUIVALENCE PASS / PERFORMANCE OPEN | После Reclaim fix SHA 11a5e0c9…3fb66e: три полных мира, payload +14 world/RNG checkpoints/native save transitions совпали с тройкой до fix. Времена 9.535782 / 9.487811 / 9.379595 с; эпохи хронологические, speedup не заявлен. Предыдущий A/B профиль и ограничения сохранены. Нет Switch performance proof. |
| A7, original text/bounding boxes | PARTIAL | Russian/English original framebuffer и 35 допустимых случаев 0/1/99/100 + #attempt fits=1. Прочие локализации, монотонность/reset во всех маршрутах и аппаратный screenshot ещё не закрыты. |
| A8, ordinary/combined ARM64/effective audit | ORDINARY + COMBINED LOCAL LINK PASS | После Reclaim fix: build/ordinary-reclaim-lock-link.log, build/combined-reclaim-lock-link.log и matching reports. undefined=0, required stubs=[], first_failure=null. Локальная проверка ELF; NRO не упаковывалась. |
| A9, MatrixGame x86/ABI/head fix | PARTIAL, substantial native proof | Настоящая карта, движение/бой/гибель, проигрыш/статистика; x86 ABI/export PASS; реальный native head pricing before FAIL/fixed PASS на 159 шаблонах. Выбор модулей через UI, победа и полный return contract ещё не проверены. Tracked CMake wrapper со свежим source snapshot build/run PASS. |
| B, one clean combined artifact | OPEN | Scoped commit, clean NRO + matching ELF + Build ID/SHA/provenance, GitHub run/download verification не выполнены. Dirty ELF не выдаётся как hardware release. |
| C, physical Switch scenarios | OPEN | Требуется один согласованный аппаратный прогон кандидата: короткие сцены, затем один New Game и заданные переходы. Не заставлять повторять генерацию до разбора первого критического отказа. |
| D, handheld battery/CPU/GPU/frame times | OPEN | Сопоставимые физические сессии не выполнялись; host/QEMU и фрейм-счётчик не заменяют их. |

## Программные и аппаратные границы

P0.1: первопричина визуального зависания после «Готово» UNCONFIRMED. Диагностика теперь
отделяет освобождение worker, окончание подготовки, GI event enter/return, состояние
root/cursor/button и CPU framebuffer. Обхода расчётов/авто-продолжения нет.
P0.2: устранено полное ReadPayload-копирование при первом ReadEntry; это не доказательство
причины всех аппаратных OOM и не подтверждение отсутствия фрагментации.
P0.3: статический аудит не закрывает полный live-object lifecycle.

Для P1 нет принятого нового аппаратного ускорения/экономии батареи. Генератор,
300 ходов, физика, порядок RNG, original formats и pinned upstream не упрощались.
Аппаратная запись сохранений выключена.

## MatrixGame track

| Этап | Состояние |
|---|---|
| MG-0, лицензирование/архитектура | PARTIAL: GPL headers/NOTICE и запрет закрытых ресурсов соблюдены; итоговая совместимость всех компонентов выбранного backend ещё не проверена. |
| MG-1, Windows x86 эталон | PARTIAL: настоящий бой/проигрыш/статистика и head fix проверены; UI-конструктор, победа и полный возврат остаются открыты. |
| MG-2, native ARM64 bridge | OPEN: x86 layout/ABI и различия ownership описаны; реальный адаптер ещё не реализован. |
| MG-3, Switch GPU/platform backend | OPEN: нет работающей GPU 3D-сцены на Switch. |
| MG-4, все игровые функции/оптимизация | OPEN: Windows functional proof не подтверждает Switch путь и оптимизацию. |
| MG-5, campaign integration | OPEN: нет запуска из кампании, win/loss возврата и двух последовательных аппаратных боёв без утечек. |

MG-3…MG-5 не должны задерживать единую стабилизационную NRO после локальных P0/P1 gates,
но остаются частью полной цели. Auto-win/фиктивный battle result не используется.

Подробные факты/хеши/ограничения: stabilization-20261018.md и matrixgame-20261018.md.


Полная optimized host-пересборка после reserve resize и A1 normal opt-out PASS
на SHA 8890e70bf7db7983e98e8dfc4583325e08c165ffcef9a1b48f9b22026d07e5e1.
A6 повторён на том же SHA: три candidate и три control, strict world/RNG PASS;
median candidate/control +8.10%, performance OPEN. Дополнительный настоящий EC_Cache integration
(tests/probe_e2e56_native_cache.py) runtime ASan/UBSan PASS на том же host source set.


После этих SHA добавлено отдельное исправление raw TCBufEC ResidentBytes:
host 5ce8a9af…07eb9f, native ASan/UBSan accounting/retained trim/release eviction PASS.
Обычный host route до ангара и оба ARM64 link/audit нового исправления PASS. Прежние
A1/A6/A8 результаты выше относятся к указанным артефактам, не автоматически к новому ELF.
Профиль подготовки до исправления: preparation-profile-20261018.md.


Следующая A3 проверка после текущего raw accounting блока: Reclaim читает
GlobalCache->ResidentBytes до/после Trim без явно охватывающего CacheLock, тогда как
load/trim меняют его под lock. Текущий single-thread reclaim PASS не доказывает
concurrent snapshots/return value. Требуется адресная native TSan проверка, затем
синхронизация при подтверждении; это не установленная причина аппаратного freeze.


Актуальный host SHA после raw accounting:
5ce8a9af61f89c3dbb1f4f9315907745653a7f22180e5205cf7cbdeb0407eb9f.
На нём A1 normal, native cache/recovery ASan/UBSan и A6 (три мира) PASS.
Оба ARM64 новых исходников link/audit PASS, Python required checks 25+8+8 PASS.
Эта строка заменяет ранние пометки pending для raw accounting; история SHA сохраняется.


Новый дополнительный A3 дефект: Reclaim ResidentBytes race воспроизведён на настоящем
cache под TSan (before exit 66), synchronized snapshot/trim fixed; 200 loads + 2441
Reclaim PASS. Текущий host 11a5e0c9…3fb66e, normal маршрут и оба ARM64 link/audit PASS.
Это заменяет предыдущий пункт о pending проверке именно Reclaim snapshots; остальные
cache/writer/cancel/lifetime gates не закрыты. См. reclaim-concurrency-20261018.md.
Native regressions добавлены в host-regression CI; remote run ещё не выполнялся.


Актуальные проверки Reclaim fix на host SHA
11a5e0c96078c95b3d54ccae01d2eeeded18162ae4e032134afa152e173fb66e:
normal до ангара, native cache TSan/ASan, native FileStream ASan, три complete world/RNG
PASS; ordinary/combined ARM64 PASS, YAML и 41 Python checks PASS. Ранние SHA выше
остаются историей. Нового clean NRO, GitHub run/download и Switch proof пока нет.
