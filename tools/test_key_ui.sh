#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S tests/key_sim -B build/key-sim -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/key-sim --parallel 8
mkdir -p build/key-preview
build/key-sim/key_ui_host build/key-preview
