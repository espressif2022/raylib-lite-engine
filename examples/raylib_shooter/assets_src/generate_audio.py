#!/usr/bin/env python3
"""Generate Mosaico Strike's deterministic arcade cues and space-synth loop."""
from __future__ import annotations

from pathlib import Path
import wave

import numpy as np

RATE = 24000
ROOT = Path(__file__).resolve().parent
RNG = np.random.default_rng(0x53545249)


def write(name: str, samples: np.ndarray, peak: float = .8) -> None:
    samples = np.asarray(samples, dtype=np.float64)
    samples = samples * (peak / (float(np.max(np.abs(samples))) or 1.0))
    with wave.open(str(ROOT / name), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes((np.clip(samples, -1, 1) * 32767).astype("<i2").tobytes())


def frames(seconds: float) -> int:
    return int(round(RATE * seconds))


def phase(freq, count: int) -> np.ndarray:
    return np.cumsum(np.broadcast_to(np.asarray(freq, float), (count,))) / RATE


def sine(freq, count: int) -> np.ndarray:
    return np.sin(2 * np.pi * phase(freq, count))


def square(freq, count: int, duty: float = .5) -> np.ndarray:
    return np.where(phase(freq, count) % 1.0 < duty, 1.0, -1.0)


def saw(freq, count: int) -> np.ndarray:
    return 2.0 * (phase(freq, count) % 1.0) - 1.0


def smooth(samples: np.ndarray, width: int) -> np.ndarray:
    if width <= 1:
        return samples
    acc = np.cumsum(np.concatenate([np.zeros(width), samples]))
    return (acc[width:] - acc[:-width]) / width


def env(count: int, attack: float = .002, decay: float = 8.0) -> np.ndarray:
    t = np.arange(count) / RATE
    return np.minimum(1.0, t / attack) * np.exp(-t * decay)


def noise(count: int) -> np.ndarray:
    return RNG.uniform(-1, 1, count)


NOTE = {n: 440.0 * 2 ** ((i - 9) / 12) for i, n in enumerate(
    ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"])}


def hz(note: str) -> float:
    return NOTE[note[:-1]] * 2 ** (int(note[-1]) - 4)


# --- cues --------------------------------------------------------------------

n = frames(.09)
write("shot.wav", smooth(square(np.geomspace(1700, 520, n), n, .3), 2) * env(n, decay=30.0), .5)

n = frames(.42)
boom = smooth(noise(n), 5) * env(n, .001, 9.0) + .6 * sine(np.geomspace(180, 40, n), n) * env(n, .001, 11.0)
write("explode.wav", boom)

n = frames(.36)
write("hurt.wav", smooth(saw(np.geomspace(420, 90, n), n) + .6 * noise(n), 4) * env(n, .001, 6.0))

def seq(notes, seconds, decay=9.0, duty=.25):
    parts = []
    for note in notes:
        count = frames(seconds)
        parts.append(smooth(square(hz(note), count, duty), 2) * env(count, decay=decay))
    return np.concatenate(parts)

write("wave.wav", np.concatenate([seq(["E5", "B5"], .08), seq(["E6"], .3, 5.0)]), .6)
write("power.wav", seq(["C5", "E5", "G5", "C6", "E6"], .05, 14.0, .125), .6)
write("start.wav", np.concatenate([seq(["G4", "C5", "E5"], .07), seq(["G5"], .32, 5.0)]), .62)
write("game_over.wav", np.concatenate([seq(["E5", "D#5", "D5"], .2, 3.5, .5),
                                       seq(["C#5"], .8, 2.0, .5)]), .62)


# --- music -------------------------------------------------------------------

def space_loop() -> np.ndarray:
    bpm, bars = 132, 8
    step = 60.0 / bpm / 4.0 * RATE
    total = int(round(step * 16 * bars))
    out = np.zeros(total)

    def place(at: float, samples: np.ndarray, gain: float) -> None:
        start = int(round(at * step)) % total
        end = start + samples.size
        if end <= total:
            out[start:end] += samples * gain
        else:
            head = total - start
            out[start:] += samples[:head] * gain
            out[:end - total] += samples[head:] * gain

    roots = ["E2", "C2", "D2", "B1"] * 2
    for bar, root in enumerate(roots):
        base = bar * 16
        f = hz(root)
        for s in range(16):
            count = int(step * .85)
            octave = 2.0 if s % 2 else 1.0
            place(base + s, smooth(saw(f * octave, count), 6) * env(count, decay=14.0), .26)
        arp = [f * 4, f * 4 * 2 ** (7 / 12), f * 8, f * 4 * 2 ** (3 / 12)]
        for s in range(16):
            count = int(step * .7)
            place(base + s, square(arp[s % 4], count, .125) * env(count, decay=18.0), .06)
        for beat in range(4):
            count = frames(.14)
            place(base + beat * 4, sine(np.geomspace(130, 45, count), count) *
                  env(count, .001, 20.0), .6)
        for at in (4, 12):
            count = frames(.16)
            hit = noise(count)
            place(base + at, (hit - smooth(hit, 5)) * env(count, .001, 18.0), .3)
        for s in range(1, 16, 2):
            count = frames(.025)
            hit = noise(count)
            place(base + s, (hit - smooth(hit, 2)) * env(count, .0005, 90.0), .08)
    lead = ["B4", "-", "E5", "-", "G5", "-", "F#5", "E5", "-", "-", "D5", "-", "E5", "-", "-", "-"]
    for bar in (2, 3, 6, 7):
        for s, note in enumerate(lead):
            if note == "-":
                continue
            count = int(step * 1.8)
            tone = smooth(square(hz(note), count, .5) + .5 * saw(hz(note) * 1.004, count), 4)
            place(bar * 16 + s, tone * env(count, .006, 4.0), .08)
    return out


write("music.wav", space_loop(), .7)
