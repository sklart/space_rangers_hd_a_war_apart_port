# MatrixGame E610 r3: аппаратный результат 02.11.2026

**HARDWARE PARTIAL: VI QUERY / ORIENTATION READBACK PASS; LATE INIT FAIL.**
Проверен C:/logs.zip, SHA256
`45ed38dca547d0dce8b9f3b7750fa98a9bdeb056d4f21dc3d8b1ec3faa519cc8`,
110771 байт. Архив содержит три прежних r2 журнала и два новых r3.
Новые image SHA совпали с подготовленными Quick и Interactive; build ID:
`d09b7859e693-5d20b97d35db625e284d`.

| Run | Режим | VI / ApplyVideoParams | Identity presentation | Итог |
| --- | --- | --- | --- | --- |
| run-1793620797003608-0 | Quick | PASS, 1280×720 / 60 Hz | PASS, побайтовое совпадение | Late Init FAIL, exit 10 |
| run-1793620969003202-0 | Interactive | PASS, 1280×720 / 60 Hz | PASS, побайтовое совпадение | Late Init FAIL, exit 10 |

## Подтверждённое продвижение

В обоих запусках VI.owner.query_mode переиспользовал живой display, реальные
запросы resolution/VSync прошли. Прежний viOpenDefaultDisplay failed: 4722
не повторился. B.ApplyVideoParams завершился успешно, Init продвинулся дальше.

Для каждого запуска сверены SHA и размеры smoke.bgra, orientation-smoke.bgra,
gamma-smoke.bgra (1280×720 BGRA8). Независимое полнокадровое сравнение smoke
и orientation-smoke даёт max RGB difference 0, alpha полностью совпадает.
Следовательно, исправленный identity gamma/presentation не отражает исходный
framebuffer. Это аппаратный PASS проверенного readback, а не доказательство
фактической ориентации VI scanout: новых фото/наблюдений экрана в архиве нет.

## Новый отказ

Оба запуска завершаются одинаковым исключением:
`original managed texture decode failed: -1`.
Граница: B.interface_and_final_resources после успешного ApplyVideoParams.
Полный Init, четыре настоящих видимых кадра карты, original loop и игровое
управление не достигнуты. В архиве нет original-first.bgra.

По исходникам tools/matrixgame_legacy_texture_overrides.py это локальная
проверка `if(!m_Tex)` в CTextureManaged::Init, выбрасывающая DeviceError с -1.
Это не расшифрованный SDK error и не доказанная ошибка PNG/JPEG decoder.
Журнал не содержит имени проблемного ресурса, его формата или первичного
результата загрузки/создания текстуры. Повреждение или отсутствие данных,
отказ формата и ошибка native texture path здесь не различаются.

Ошибки enclosing scopes A.native_device_owner / A.SwitchDisplay отражают
распространение позднего исключения; сами по себе они не означают, что VI owner
не создался. В обоих журналах выполнены error-screen, host cleanup,
VI.owner.close_window, close_layer, close_display и exit_service;
close_layer/close_display возвращают 0. Оба журнала корректно закрыты.

## Сохранённое evidence и следующая граница

[Машиночитаемые результаты](matrixgame-e610-r3-hardware-20261102-evidence.json)
содержат идентичность обеих NRO, хеши кадров/журналов, результаты сравнения,
точки отказа и VI cleanup. Неизменённые новые журналы и кадры сохранены локально
в build/matrixgame/e610-r3-hardware-20261102/. Исходный C:/logs.zip не изменён.

Следующая адресная задача — установить имя и тип управляемой текстуры,
воспроизвести её настоящий read/decode/create путь и исправить первичный отказ.
Пропуск null texture, подстановка пустого изображения или фиктивный Init PASS
не допускаются. Полную аппаратную приёмку, Campaign/Gov и победу не объявляем.
При этом анализе исходники, NRO, ресурсы, сохранения и upstream не менялись;
новые runtime-тесты и сборки не запускались.
