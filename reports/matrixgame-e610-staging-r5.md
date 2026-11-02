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

## Итоговая сборка и пакет

**SOFTWARE PASS / R5 HARDWARE PENDING.**
Source commit: `937eccd13fc2b8911863459ae6e0ee734c31adca`.
Каталог SDK: build/matrixgame/e610-staging-r5/1.
Build ID: `937eccd13fc2-7a9c102056ba1fc5c8a2`.
Configure и compile/link двух стандартных целей завершились с 0. Проверены
отсутствие strong undefined, AArch64 ELF, NRO0/ASET/icon/NACP, встроенные шрифты
и соответствие ELF/NRO build ID. Все 687 собственных source hashes совпали.
Generated original engine побайтово совпадает с r4; 171 ранее существовавший
файл dist/logs/saves и прежние SDK-артефакты прошли сверку SHA.
Основная/root NRO, оригинальные ресурсы и frozen inputs проверены builder guards.

В обеих ELF сохранены проверки постусловий vertex/index setters; прежний
comparison-only error marker отсутствует. Managed eviction, VI query и
orientation checker сохранены; автоматический SDK default-window startup не
включён, SDL accessor по-прежнему связан с реальным окном владельца.

Пакет: `dist/matrixgame-e610-switch-test-r5-staging.zip`, 10054895 байт.
SHA256: `f4daff97e481550fa0bff3bd5b8cd78053963c4ebd9a90d9926fb69407f8da68`.

| NRO | SHA256 | ELF build ID |
| --- | --- | --- |
| Quick Smoke | `9ae039508821491b9c59cb02cee6ff29a3bfb8b1dbdb26cd01bee09f600947cd` | `1f472634190be2fb310d77b3c1558e64a7630828` |
| Interactive | `2cfb5d13c71149bd53736eed7281eb85ef2f26970fddab17051f525bd172113c` | `c202a2ae22cfc08dbda5b245727870ed066de427` |

ZIP содержит ровно шесть файлов: две NRO, README.ru.md, manifest.json,
SHA256SUMS.txt и build-info.json. CRC, SHA и побайтовое соответствие папке
пакета проверены. Игровые данные и сохранения в ZIP не включены.
[Машиночитаемое evidence](matrixgame-e610-staging-r5-evidence.json).

Копировать две NRO в `/switch/matrixgame-e610-r5-test/`; DATA/CFG оставить на
прежних местах. Сначала Quick Smoke, затем Interactive с выходом Minus+Plus.
При отказе передать новые папки run directories и фото экрана. До этих результатов
полный Init и четыре настоящих игровых кадра r5 не объявляются пройденными.
