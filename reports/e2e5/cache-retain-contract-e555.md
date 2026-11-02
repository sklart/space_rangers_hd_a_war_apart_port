# E555 — GI retain через настоящий acquire и контракт eviction

Проверен следующий пробел прежнего воспроизведения P0: теперь BoundData и
RetainCount не выставляются вручную. Оригинальный AcquireCachedGi дважды
вызывается для настоящего TCGiControlEC и загруженного GI, создавая binding и
RetainCount=2. После чужого EvictData возвращённый указатель действительно
становится висячим: ASan диагностировал heap-use-after-free при обращении
к его Image, exit=86 как и предусмотрено проверкой baseline.

Это **воспроизведение дефекта, не исправление**. Глобальная политика кэша
не изменена. Полный GUI transition, повторяющийся текст и Switch hangs не
воспроизводились и не объявляются объяснёнными этим результатом.

## Ресурс и точность исходников

Прочитан DATA/background.pkg, извлечён DATA/FormRuinsBG/2RCbg.gi (Format2,
1920×1536). Настоящие LoadFromConfigBuffer, rescale/repack и DrawToGraphBuf
исполняются в ранее проверенном fixture; 1280×720 RGB565 fnv64 остаётся
c7abbe2c80d5e0f4. Менеджеру передаётся уже загруженная запись. Это hot acquire;
холодная package/config publication и ожидание загрузчика здесь не проверяются.

EC_Cache.cpp компилируется заново с ASan/UBSan прямо из frozen effective source:
port/switch/build/e2e-game-perf515/generated/EC_Cache.cpp,
SHA-256 48a3c5114244762a89e121db3f2547511beba76720d4e8d44ebb9497885a018c.
Файл не изменяется. Объекты GI/GraphBuf/codec/runtime из прежнего fixture
переиспользованы и не объявляются заново инструментированными.

## Наблюдения

| Сценарий | Реальный результат |
| --- | --- |
| Чужой непривязанный control, правильный класс GI | Lookup выбирает запись и освобождает её. У владельца RetainCount=2, BoundData=nullptr. |
| Сам вызывающий control удерживает GI | Его RetainCount=1 запрещает eviction; чужой retained GI остаётся действительным. |
| Непривязанный control, неверный класс | Тест использует базовый TCacheDataEC вместо TCGiEC; exact-class lookup не выбирает GI. |
| Уже привязанный control, неверный класс | После настоящего Acquire/Release остаётся BoundData; аргумент класса не фильтрует эту ветку. Чужой retained GI освобождается. |
| TrimToBudget(0) | Сохраняет запись при RetainCount=2. После двух Release удаляет её и корректирует ResidentBytes. |
| Чтение возвращённого retained pointer после eviction | Ожидаемый ASan heap-use-after-free, exit=86. Это диагностический baseline, не кандидат PASS. |

TrimToBudget проверяет все controls; EvictData — только caller. Clear по
исходникам намеренно инвалидирует все записи, включая retained. Поэтому
механическая замена всех forced operations обычным trim не обоснована.
Следующий шаг для исправления владения — определить сохранение forced lookup
invalidation при живых пользователях старых данных и проверить cleanup;
просто запретить все forced eviction этот этап не предлагает.

Ранее установлено, что реальные EvictRuinsAndGovernmentCaches в известных
переходах передают TCGaiEC через непривязанный temporary control. Exact-class
lookup исключает проблемный TCGiEC. Настоящий вызов, освобождающий именно этот
GI в ошибочном аппаратном переходе, по-прежнему не доказан.
[Предыдущий frozen audit](hardware-20261023-cache-frozen-audit.md).

## Границы проверок

- HOST: шесть адресных сценариев. ASan на fixture и полном frozen cache TU;
  UBSan на тех же новых объектах, но vptr для cache TU отключён: его metadata
  удерживает RTTI неполинкованных классов всей игры. LSan отключён; утечки
  этим набором не проверялись. Старые codec objects не инструментированы заново.
- В тесте холодные ReadBufferByPath/event/sleep endpoints аварийно завершаются,
  если будут вызваны. Успешная холодная загрузка ими не имитируется. Texture
  eviction endpoint моделирует пустой GPU texture cache; GPU не выполняется.
- ARM64/QEMU, WINDOWS HAL и отдельный SDK compile/link этого cache fixture:
  NOT RUN. Здесь нет нового production candidate или новой Switch сборки.
- Последний MatrixGame SDK ELF E554 сохраняется; его source/link fingerprints
  проверены отдельно. Это не перенос HOST baseline результата на Switch.
- SWITCH HARDWARE: NOT RUN / HARDWARE_PENDING.

Запуск в WSL из корня репозитория:

```sh
python3 tools/e2e555_cache_contract.py
```

Команды, логи шести сценариев, frozen object и binary находятся в
build/e2e555-cache-contract. Итог:
cache-retain-contract-e555-evidence.json. Контроль ресурсов и protected inputs
в этом аудите отдельный; основная NRO, оба diagnostic пакета, frozen, upstream,
оригинальные игровые ресурсы и сейвы не менялись. Commit/push нет.
