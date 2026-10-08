# IMU steering tuning — 2026-10-08

The previous mapping treated raw X acceleration as steering and used a high
lateral-speed gain. Steering release passed through both the IMU filter and
the vehicle response, while camera yaw followed the raw command. A quick tilt
could therefore move the bike too far and keep turning after returning level.

The game now derives roll from the gravity vector, calibrates that angle during
countdown, and rejects acceleration magnitudes outside 0.65–1.45 g. This rejects
obvious impacts/translations; it is not a full accelerometer/gyroscope fusion
system. Small tilts use a nonlinear response, steering engagement is rate
limited, and release is faster. Vehicle motion and camera yaw share the shaped
command. Curve drift remains, with enough steering authority to counter it.

## Host verification

The [control replay](../tests/check_steering_response.py) compiles the actual
Host module once and runs all three courses. Each scenario holds X=0.25 g,
Y=0 and Z=1 g for the 90-tick countdown, then measures 90 driving ticks.
Pulse scenarios last 15 ticks (0.5 seconds) before returning to that pose.
The figures below are maximum lateral differences from a matching neutral
replay, in track-local game units; they are not physical distances.

| Input | Before | After |
| --- | ---: | ---: |
| Moderate pulse: X increases by 0.30 g | 1.04 | 0.23 |
| Larger pulse: X increases by 0.60 g | 2.78 | 0.75 |
| Larger pulse exits the road during the measured window | Yes | No |

These pulse measurements matched across all three courses. Additional checks
cover alternating ±0.30 g swings every six ticks, neutral touch takeover,
leftward touch drag, repeated 3 g spikes, steering release and heading recovery.
After the repeated swings, the shaped steering returns to zero; maximum extra
lateral excursion is approximately 0.25 units. Deliberately holding a large
right tilt still exits the road. High-speed model tests check both directions,
all-course completion with active steering, and failure without steering.

```sh
python3 examples/neon_rift_rally/tests/check_steering_response.py \
  --output examples/neon_rift_rally/build-host/steering-candidate.json
bash examples/neon_rift_rally/tests/run_host_tests.sh
```

The earlier renderer profile predates this gameplay tuning. Recapture a
renderer baseline after gameplay changes; its comparison deliberately rejects
different game-state hashes. These tests do not validate actual sensor noise,
board-axis orientation or hand-held feel. Those need a device session.
