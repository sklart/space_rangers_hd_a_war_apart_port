# MatrixGame E610 r5: staging setters для миникарты

Основание: аппаратные r4 Quick/Interactive выполнили managed eviction и дошли
до B.CMinimap::RenderBackground, где отклонилась смена vertex binding.
[Журналы и граница r4](matrixgame-e610-r4-hardware-20261102.md).

## Причина и исправление

В DekoDevice::BindStagingVertex и BindStagingIndex находились только проверки
совпадения аргументов с текущим buffers::Context. Эти методы вызываются как
setters из адаптированного оригинального pipeline. Например, водные проходы
MatrixRenderPipeline выбирают собственные VB/IB; ранее выбранный BigVB может
отличаться. Корректное переключение отвергалось до чтения новых vertex bytes.
По аппаратным журналам конкретные значения аргументов не записаны; дефект
контракта setter воспроизведён независимо на настоящих staging ресурсах.

Новый общий native_staging helper реализует настоящий Context::BindVertex/Index.
Контекст удерживает новый ресурс перед освобождением предыдущего. Stream 0,
locked/range/stride и owner checks сохранены; неверный ввод не изменяет binding.
Нулевой VB нормализует offset/stride в ноль, как существующий Windows backend.
После успешного изменения проверяется постусловие фактически выбранной привязки.
Это не подавление ошибки или возврат фиктивного успеха: NativeDraw/Indexed читает
именно этот Context и выполняет настоящий последующий decode/upload/record.

GPU upload в setter не добавлялся: native backend уже декодирует CPU staging
и загружает фактические draw packets при Draw. Snapshots сохраняют настоящие
ссылки на staging buffers, поэтому после переключения/Release предыдущие данные
доступны до завершения восстановительных leases. Upstream, generated engine
и сам CMinimap не редактировались. Generated delta к E601 остаётся ровно двумя
managed texture файлами, введёнными r4; новая правка находится в native backend.

## Адресные доказательства

Тест использует реальные Context/VertexBuffer/IndexBuffer, FixedVertexState::DecodeOne
и NativeStateStore leases. Подмены GPU или успешного Init в нём нет.
Начальная BigVB-подобная привязка A меняется на B с ненулевым offset; проверяется,
что decode возвращает x=113 из B вместо x=10 из A. Проверены index switching,
null unbind, nested snapshot restore, retain/release после Release владельцем,
отказ foreign/locked buffers, stream 1, нулевого stride и offset за границей.
После освобождения всех bindings/leases фактический Context budget полностью свободен.

- Windows/MSVC x64: PASS CPU reference.
- Host ASan/UBSan/LSan: PASS.
- ARM64/QEMU с полным UBSan/vptr: PASS. Сохранена локализация instrumented
  type_info inline, введённая для корректной работы sanitizer runtime в r4.
- Negative control извлекает точные тела r4 BindStagingVertex/Index из Git;
  тот же тест останавливается с native vertex binding differs from retained
  staging context (expected exit 6). Проверки и assertions не ослаблялись.

Результаты: build/matrixgame/e610-staging-r5-checks/{windows,host,arm64,baseline}/1/.
Большие Windows боевые сценарии и полные регрессии повторно не запускались.

## Граница поставки

Сохраняются managed residency r4, VI owner/SDL binding, orientation checker,
настоящие Quick четыре видимых map frames и Interactive до 120 секунд после Init.
Названия в hbmenu — MatrixGame r5 Quick Smoke / MatrixGame r5 Interactive.
Пути существующих ресурсов не меняются; вывод и журналы остаются отдельными.
[Инструкция r5](../docs/matrixgame-e610-staging-r5-readme.ru.md).

Новых аппаратных результатов r5 пока нет. Программный тест setter не доказывает
полный RenderBackground, Init, карту, GPU/HID или бой на Switch. Campaign/Gov
entry и естественная победа остаются вне standalone покрытия. Старые NRO/ZIP,
основная NRO, ресурсы, сохранения, upstream и frozen-комплекты сохраняются.
