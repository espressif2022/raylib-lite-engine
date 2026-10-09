#!/usr/bin/env python3
"""Generate deterministic compact synthwave racing cues."""
from pathlib import Path
import math, random, struct, wave

RATE = 24000
ROOT = Path(__file__).resolve().parent

def save(name, duration, sample):
    frames = []
    count = int(RATE * duration)
    for i in range(count):
        value = max(-1.0, min(1.0, sample(i / RATE, i, count)))
        frames.append(struct.pack("<h", int(value * 32767)))
    with wave.open(str(ROOT / name), "wb") as out:
        out.setnchannels(1); out.setsampwidth(2); out.setframerate(RATE)
        out.writeframes(b"".join(frames))

def engine(t, _i, count):
    phase = t / (count / RATE)
    fade = min(1.0, phase * 30.0, (1.0 - phase) * 30.0)
    pulse = 1.0 if math.sin(2 * math.pi * 55 * t) > 0 else -1.0
    return fade * (.045 * pulse + .055 * math.sin(2 * math.pi * 110 * t) +
                   .025 * math.sin(2 * math.pi * 220 * t))

def sweep(start, end, duration, decay=5.0):
    def sample(t, _i, _count):
        k = t / duration
        phase = 2 * math.pi * (start * t + (end - start) * t * k * .5)
        return .28 * math.sin(phase) * math.exp(-t * decay)
    return sample

def chord(freqs, _duration):
    def sample(t, _i, _count):
        env = min(1.0, t * 30.0) * math.exp(-t * 3.8)
        return env * sum(math.sin(2 * math.pi * f * t) for f in freqs) * .09
    return sample

rng = random.Random(0x52494654)
def grit(t, _i, _count):
    return rng.uniform(-1, 1) * .22 * math.exp(-t * 18.0) + \
           math.sin(2 * math.pi * 120 * t) * .14 * math.exp(-t * 9.0)

save("engine.wav", 2.0, engine)
save("boost.wav", .42, sweep(180, 1200, .42, 3.0))
save("drift.wav", .24, lambda t, i, n: grit(t, i, n) * .72)
save("jump.wav", .28, sweep(260, 760, .28, 5.0))
save("land.wav", .24, grit)
save("checkpoint.wav", .25, chord((660, 990), .25))
save("lap.wav", .55, chord((440, 660, 880), .55))
save("finish.wav", 1.15, chord((523.25, 659.25, 783.99, 1046.5), 1.15))
save("offtrack.wav", .34, sweep(190, 72, .34, 5.5))
save("collision.wav", .18,
     lambda t, i, n: .48 * rng.uniform(-1, 1) * math.exp(-t * 19.0) +
     .22 * math.sin(2 * math.pi * (95 - 260 * t) * t) * math.exp(-t * 12.0))
