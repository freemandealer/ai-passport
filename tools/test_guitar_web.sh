#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/guitar-sim
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/guitar_score_cli.c main/guitar_score.c -o build/guitar-sim/guitar_score_cli
node tests/test_guitar_web.mjs
