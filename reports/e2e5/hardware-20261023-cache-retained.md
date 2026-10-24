# Проверка совместного владения cache entry

25.10.2026. Адресное наблюдение, **не исправление аппаратного P0**.

На настоящем Bm.FormRuins.2RCbg воспроизведено поведение оригинального
TCacheControlEC::EvictData: control без удержания уничтожает общую запись,
хотя другой привязанный control имеет RetainCount=1. После вызова второй
control сохраняет счётчик 1, но BoundData уже nullptr; LRU и ResidentBytes пусты.
Деструкторы настоящих TCGiEC/TgiGR освобождают принадлежащий им растр.
Освобождённый указатель в тесте не разыменовывался.

Данные загружены и масштабированы оригинальными GI-методами. Два control и
их binding/RetainCount заданы явно через исходный AppendControl и поля классов:
это адресный тест контракта eviction, не запись реального GUI acquisition.
EvictData, удаление из LRU, destructor и unlink — оригинальный код, а не имитация.
Политика EvictData отличается от TrimToBudget, который проверяет всех controls.

**Связь с аппаратным фоном не установлена.** EvictRuinsAndGovernmentCaches
вызывает EvictBlockChildrenFromCache для TCGaiEC, тогда как проблемный фон —
TCGiEC. FindDataByKeyAndClass требует точного совпадения класса. Прямое GI-eviction
меню касается Bm.FormMain3, а не Bm.FormRuins.2RCbg. В имеющемся журнале нет
события освобождения именно ошибочного GI при живом retainer. Кроме того,
nullptr binding не доказывает повторный вывод старого текста.

Изменение общей политики EvictData без подтверждения её использования в
проблемном переходе не внесено: это мог бы быть намеренно принудительный API.
Тест фиксирует опасное условие для дальнейшей проверки, не выдавая его за
найденную первопричину. Основная NRO, frozen ZIP, ресурсы и сейвы не менялись.

Воспроизведение: `python3 tools/e2e523_cache_retained.py` в WSL.
Результат и команда: [evidence](hardware-20261023-cache-retained.json).
Host native slice, без нового sanitizer/ARM64/hardware результата.
