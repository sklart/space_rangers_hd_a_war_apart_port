# Общий GI-кэш: применимость к frozen E2E-5.16

25.10.2026. Аудит исходников, без нового аппаратного запуска.

Аппаратная сборка: 03651ced28ecf0125f0b62c4f8129bd71e27f5c8 dirty,
build_id b6c18a13cef88fe757d683e18cf48fc667067bda. Effective source map указывает
на `port/switch/build/e2e-game-perf515/generated/EC_Cache.cpp`.
SHA-256 **48a3c5114244762a89e121db3f2547511beba76720d4e8d44ebb9497885a018c**
совпал с сохранённым source-sha256.json комплекта. Все файлы 14 найденных
определений/вызовов EvictData и EvictBlockChildrenFromCache также совпали с manifest.

Таким образом, дефект EvictData применим к точным frozen-исходникам:
проверяется RetainCount только вызывающего control, общая запись удаляется
без проверки retain остальных controls. Ранее отдельный host-тест исполнил
это на настоящем `GI,Bm.FormRuins.2RCbg`, DATA/FormRuinsBG/2RCbg.gi из
background.pkg (Format2). Деструктор освобождает растр, у удерживающего control
остаётся RetainCount=1 при BoundData=nullptr. Этот тест не был GUI acquisition
и повторно здесь не запускался.

Однако известный EvictRuinsAndGovernmentCaches передаёт **TCGaiEC**.
Его временный control не привязан к данным, а FindDataByKeyAndClass требует
точный класс: запись проблемного **TCGiEC** этим вызовом не выбирается.
Прямые GI-вызовы относятся к Bm.FormMain3; найденный вызов aScript — к TCBufEC.
В EvictData уже привязанный BoundData не фильтруется по переданному классу;
это отдельное условие, но такой control для ошибочного фона в аппаратном
переходе не доказан.

Итого: дефект общего владения и применимость к типу/ресурсу подтверждены;
реальное освобождение этого ресурса в ошибочном переходе и происхождение
повторяющегося текста **OPEN**. Менять глобальную политику eviction без
доказательства исходного forced-eviction контракта этот аудит не обосновывает.

Воспроизведение аудита: `python tools/e2e523_cache_frozen_audit.py`.
Подробности: [evidence](hardware-20261023-cache-frozen-audit.json).
Основная NRO, frozen комплекты, ресурсы, logs и сейвы не изменены.
