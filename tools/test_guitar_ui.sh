#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S tests/guitar_sim -B build/guitar-sim -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/guitar-sim --parallel 8
mkdir -p build/guitar-preview
build/guitar-sim/guitar_ui_host build/guitar-preview
