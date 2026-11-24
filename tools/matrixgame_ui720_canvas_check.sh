#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/matrixgame/ui720-canvas-check
g++ -std=c++20 -O1 -g -fsanitize=address,undefined tests/test_matrixgame_rangers_canvas.cpp -o build/matrixgame/ui720-canvas-check/canvas
build/matrixgame/ui720-canvas-check/canvas
