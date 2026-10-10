#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
out="${TMPDIR:-/tmp}/neon_rift_rally_test"
cc -std=c11 -Wall -Wextra -Werror -I"$project_dir/main" \
  "$project_dir/tests/test_rally_game.c" "$project_dir/main/rally_game.c" \
  -lm -o "$out"
"$out"
python3 "$project_dir/tests/check_feedback.py"
python3 "$project_dir/tests/check_steering_response.py"

state="${TMPDIR:-/tmp}/neon_rift_rally_two_finger.json"
python3 "$project_dir/tests/check_two_finger.py" "$state"
python3 "$project_dir/tests/check_imu_steering.py"
control_state="${TMPDIR:-/tmp}/neon_rift_rally_control.json"
python3 "$project_dir/tests/check_control_flow.py" "$control_state"
content_state="${TMPDIR:-/tmp}/neon_rift_rally_content.json"
python3 "$project_dir/tests/check_content_flow.py" "$content_state"
