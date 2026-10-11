# E2E-4 Switch hardware smoke (pending)

Use the E2E-4 Audio OFF NRO only after checking its embedded `build_git`, file
name and SHA-256 against the build report. An Audio ON NRO must have a separate
name, report and hash; none is currently qualified. Preserve the E2E-2 and
E2E-3 NRO files and their recorded hashes.

For each candidate, record the Switch firmware, SD filesystem, game-data
revision, build ID, available memory, start time and controller used. Capture
both `logs/e2e.log` and the game's `########.log` after each run. Analyze the
first with `python tools/e2e4_log_analyzer.py path/to/e2e.log`.

1. Launch and confirm build ID, menu and visible pointer.
2. Move the pointer onto `NEW GAME`; press A and separately try X/Enter.
3. Change GameSettings values; start generation and record every progress and
   error transition, including Introduction and the first gameplay screen.
4. Exercise a script-triggered event; save, restart, load, and compare the
   same game state. Keep a copy of the old save when investigating failures.
5. For a future Audio ON candidate, check menu hover/confirm effects, music,
   simultaneous gameplay effects, volume/pan, fade and rapid track changes.
   Confirm decode, PCM submission and cursor advancement in logs; listen for
   dropouts and channel errors. Audio OFF provides the control run.
6. Record frame-time and memory readings for the same scene in both builds,
   if both builds exist. Run extended playback and exit through PLUS.
7. Preserve logs and crash reports. Mark any step that did not execute as
   `NOT REACHED`, a step without enough evidence as `NOT VERIFIED`, and all
   physical checks as `HARDWARE PENDING` until a real console run.

No hardware run is reported by this document.
