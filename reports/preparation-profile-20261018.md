# A6: разбор подготовки мира на host, 18.10.2026

Статус: PARTIAL. Нет принятого ускорения и нет Switch performance proof.
Артефакты измерений до raw-cache accounting fix: candidate 8890e70b…07e5e1,
whole-payload control a8ebf322…94278e. Текущий 5ce8a9af…07eb9f этим профилем не покрыт.

В чередующейся тройке candidate stage 1 занимал 658 / 661 / 2561 мс, control
657 / 686 / 656 мс. Stage 1 включает создание звёзд и GenerateGalaxyLayout.
Stage 3 включает AssignTextQuestsToPlanets и инициализацию distance tiers.
Native quest instrumentation уже имелась в effective source и была извлечена
из существующих логов: 71 quest, acquire 767967…1306946 мкс у candidate,
885111…894578 мкс у control; parse только 6910…9928 мкс, search 337…541 мкс.
Значительная вариация приходится на acquire raw resource, а не разбор quest metadata.

## Внешние OS-наблюдения

Отдельная пара запусков без изменения ELF: candidate 10.920374 с, control 11.643167 с.
Полный payload, все 14 world/RNG checkpoints и native save transitions совпали
с предшествующей серией. По семь OS snapshots на вариант. Приблизительные границы
берутся при доставке native stage records во внешний наблюдатель (не синхронно
с worker). Считались /proc/PID/stat и /proc/PID/task/TID/stat, clock=100 Hz.

| Stage 3 | Native wall, с | Наблюдённый wall, с | Process CPU, с |
|---|---:|---:|---:|
| Candidate | 1.092 | 1.096 | 0.32 |
| Control | 2.092 | 2.091 | 0.44 |

В этих логах quest acquire = 1081921 / 2081666 мкс, parse = 7116 / 7662 мкс.
Разницу wall time не объясняет учтённый Linux CPU процесса. Возможны ожидания,
внешний host/WSL I/O и особенности учёта виртуализации; конкретная причина не доказана.
Это не основание обвинять генератор или объявить ускорение по одной обратной паре.
Stage 1 outlier 2.561 с в этой паре не воспроизвёлся (651 / 642 мс).

/proc/PID/io отсутствует (ENOENT), поэтому io_delta=null, а не нулевой I/O.
Первый внешний sampler завершился на временном DrvFS ENODATA; результат его игры
не использован. После проверки отсутствия оставшихся процессов выполнена версия v2:
бинарное чтение с сохранением byte offset, retry без перезапуска игры, немедленное
сохранение samples. Обработаны ENODATA 3/5 раз. Они могут смещать границы наблюдений;
например, одна соседняя граница сдвинулась примерно на 100 мс. Полный profile/v2
прошёл строгое world/RNG сравнение. Это вспомогательная host-телеметрия, не цикл CI.

Данные: build/process-profile-v2-{candidate,baseline}-os.json,
build/process-profile-v2-summary.json; logs build/process-profile-v2.log.
Sampler остаётся в ignored build/profile-generation-process-v2.py, не меняет игру.

## Отдельный подтверждённый дефект учёта

В реальных логах все 71 cache load PlanetQuest имели bytes=0. Native TCBufEC
не выставлял ResidentBytes при копировании SourceBuffer в owned Buffer. Отдельный
native regression воспроизвёл capacity=131335 при resident=0. Этот дефект исправляется
адресно в disposable overlay; он не назван причиной всей задержки или зависания.
