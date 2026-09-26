#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S tests/frog_sim -B build/frog-sim -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/frog-sim --parallel 8
mkdir -p build/preview
build/frog-sim/frog_ui_host build/preview
python3 tools/frog_preview.py
