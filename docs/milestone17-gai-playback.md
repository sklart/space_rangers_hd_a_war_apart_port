# M17: portable GAI sequence playback

M17 воспроизводит только embedded sequence `0` из `DATA/Asteroid/00.gai` с `Flags == 0`.  Парсер `ReadGaiSequence` остаётся CPU-only, а `gai_playback_cpu` не читает системные часы и принимает только переданное монотонное число миллисекунд.

На релизном ресурсе sequence содержит 100 кадров с delay 50 ms: nominal cycle равен 5000 ms. Первичный цикл строит каноническую последовательность decoded Format-2 RGBA-пикселей; ожидаемые fingerprint: sequence CRC32 `4b1c6ebf`, FNV-1a-64 `47cdc8c73fc1ce61`; cycle CRC32 `5b7bc7e9`, FNV-1a-64 `f70813ac799a25b3`.

M12 вызывает opt-in callback до presentation, передавая тот же `now_ms`, что использует runtime loop. Ошибка decode, fingerprint, немонотонного времени или timing tolerance завершает загрузку как diagnostic failure. Без callback обычный M12 путь не меняется.

Для Switch-проверки нужен один полный цикл без нажатия Plus. В `port.log` должны быть `[M17] PASS sequence=0 cycle=1`, фактическая длительность, max tick gap и `[M12] exit_reason=plus`; затем обязателен `[BOOT] COMPLETE`.
