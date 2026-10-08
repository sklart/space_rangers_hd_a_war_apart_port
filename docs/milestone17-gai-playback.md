# M17: portable GAI sequence playback

M17 воспроизводит только embedded sequence `0` из `DATA/Asteroid/00.gai` с `Flags == 0`.  Парсер `ReadGaiSequence` остаётся CPU-only, а `gai_playback_cpu` не читает системные часы и принимает только переданное монотонное число миллисекунд.

На релизном ресурсе sequence содержит 100 кадров с delay 50 ms: nominal cycle равен 5000 ms. Первичный цикл строит каноническую последовательность decoded Format-2 RGBA-пикселей; ожидаемые fingerprint: sequence CRC32 `4b1c6ebf`, FNV-1a-64 `47cdc8c73fc1ce61`; cycle CRC32 `5b7bc7e9`, FNV-1a-64 `f70813ac799a25b3`.

M12 вызывает opt-in callback до presentation, передавая тот же `now_ms`, что использует runtime loop. Ошибка decode, fingerprint, немонотонного времени или timing tolerance завершает загрузку как diagnostic failure. Без callback обычный M12 путь не меняется.

Switch hardware PASS получен на NRO SHA-256 `E753BFEAA6BF71C48086C98BB8A27307A0929ECB05B362D343CFA7B8777DCD0D` с embedded `build_git=13ad513`. Первый цикл завершился за 5033 ms при nominal 5000 ms, max tick gap 129 ms и допустимом excess 134 ms; оба fingerprint совпали с oracle. После `[M17] PASS sequence=0 cycle=1` M12 продолжил работу, отработал 832 frames/presents за 41,746 ms, вышел через `PLUS` и записал `[BOOT] COMPLETE`.
