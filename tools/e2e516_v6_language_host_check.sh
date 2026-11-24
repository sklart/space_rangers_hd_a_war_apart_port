#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=build/matrixgame/v6-language-replay-20261124-1
test -f "$out/CFG.TXT" && test -f "$out/Lang.txt"
test ! -e "$out/host-result.json"
python3 tools/e2e5_host_game_probe.py \
 --binary port/switch/build/e2e5-host-game/reproducible/original-game-host \
 --game-root 'windows/Space Rangers HD A War Apart' \
 --library-dir port/switch/build/e2e5-host-game/reproducible/okgf \
 --cfg-file "$out/CFG.TXT" --lang-file "$out/Lang.txt" --timeout 50 \
 >"$out/host-result.json" 2>"$out/host-error.log"
