#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
out="${TMPDIR:-/tmp}/tomb_explorer_test"
cc -std=c11 -Wall -Wextra -Werror -I"$project_dir/main" \
  "$project_dir/tests/test_tomb_game.c" \
  "$project_dir/main/tomb_game.c" \
  "$project_dir/main/tomb_level_data.c" -lm -o "$out"
"$out"
