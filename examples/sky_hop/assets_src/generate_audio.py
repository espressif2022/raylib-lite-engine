#!/usr/bin/env python3
"""Generate Sky Hop's original, deterministic chiptune cues and two music loops."""
from __future__ import annotations

from pathlib import Path
import wave

import numpy as np

RATE = 24000
ROOT = Path(__file__).resolve().parent
RNG = np.random.default_rng(0x534B59)


def write(name: str, samples: np.ndarray, peak: float = .82) -> None:
    samples = np.asarray(samples, dtype=np.float64)
    top = float(np.max(np.abs(samples))) or 1.0
    pcm = np.clip(samples * (peak / top), -1.0, 1.0)
    with wave.open(str(ROOT / name), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes((pcm * 32767).astype("<i2").tobytes())


def timeline(seconds: float) -> np.ndarray:
    return np.arange(int(RATE * seconds)) / RATE


def phase_of(freq: np.ndarray | float, count: int) -> np.ndarray:
    freq = np.broadcast_to(np.asarray(freq, dtype=np.float64), (count,))
    return np.cumsum(freq) / RATE


def poly_blep(phase: np.ndarray, step: np.ndarray) -> np.ndarray:
    out = np.zeros_like(phase)
    rising = phase < step
    x = phase[rising] / step[rising]
    out[rising] = x + x - x * x - 1.0
    falling = phase > 1.0 - step
    x = (phase[falling] - 1.0) / step[falling]
    out[falling] = x * x + x + x + 1.0
    return out


def pulse(freq, count: int, duty: float = .25) -> np.ndarray:
    """Band-limited (PolyBLEP) pulse: naive edges alias and defeat IMA-ADPCM."""
    step = np.broadcast_to(np.asarray(freq, dtype=np.float64), (count,)) / RATE
    phase = phase_of(freq, count) % 1.0
    wave_ = np.where(phase < duty, 1.0, -1.0)
    return wave_ + poly_blep(phase, step) - poly_blep((phase - duty) % 1.0, step)


def triangle(freq, count: int) -> np.ndarray:
    return 4.0 * np.abs((phase_of(freq, count) % 1.0) - .5) - 1.0


def noise(count: int) -> np.ndarray:
    return RNG.uniform(-1.0, 1.0, count)


def envelope(count: int, attack: float = .004, decay: float = 6.0) -> np.ndarray:
    t = np.arange(count) / RATE
    return np.minimum(1.0, t / attack) * np.exp(-t * decay)


def lowpass(samples: np.ndarray, amount: float = .35) -> np.ndarray:
    out = np.empty_like(samples)
    state = 0.0
    for i, value in enumerate(samples):
        state += (value - state) * amount
        out[i] = state
    return out


def sweep(start: float, end: float, seconds: float, shape: str = "pulse",
          decay: float = 6.0, duty: float = .25) -> np.ndarray:
    count = int(RATE * seconds)
    freq = np.geomspace(start, end, count)
    wave_ = pulse(freq, count, duty) if shape == "pulse" else triangle(freq, count)
    return wave_ * envelope(count, decay=decay)


def notes(sequence: list[tuple[float, float]], shape: str = "pulse",
          duty: float = .25, decay: float = 5.0) -> np.ndarray:
    parts = []
    for freq, seconds in sequence:
        count = int(RATE * seconds)
        tone = pulse(freq, count, duty) if shape == "pulse" else triangle(freq, count)
        parts.append(tone * envelope(count, decay=decay))
    return np.concatenate(parts)


# --- effects ---------------------------------------------------------------

write("jump.wav", lowpass(sweep(330, 880, .16, duty=.5, decay=9.0), .5))
write("coin.wav", notes([(987.77, .06), (1318.5, .22)], duty=.25, decay=7.0))
stomp = timeline(.18)
write("stomp.wav", .7 * sweep(420, 90, .18, shape="triangle", decay=14.0) +
      .5 * noise(stomp.size) * envelope(stomp.size, decay=30.0))
hurt = timeline(.42)
vibrato = 1.0 + .04 * np.sin(2 * np.pi * 28 * hurt)
write("hurt.wav", pulse(np.geomspace(520, 140, hurt.size) * vibrato, hurt.size, .5) *
      envelope(hurt.size, decay=4.5))
write("win.wav", notes([(523.25, .1), (659.25, .1), (783.99, .1), (1046.5, .1),
                        (783.99, .1), (1046.5, .5)], duty=.25, decay=3.0))
spring = timeline(.36)
boing = 220 * (1 + 1.6 * spring / spring[-1]) * (1 + .08 * np.sin(2 * np.pi * 22 * spring))
write("spring.wav", triangle(boing, spring.size) * envelope(spring.size, decay=6.0))
chime = notes([(1567.98, .08), (2093.0, .3)], shape="triangle", decay=5.0)
write("checkpoint.wav", chime + .35 * notes([(783.99, .08), (1046.5, .3)], duty=.125,
                                             decay=6.0))
land = timeline(.09)
write("land.wav", lowpass(noise(land.size), .18) * envelope(land.size, decay=45.0) +
      .6 * sweep(160, 70, .09, shape="triangle", decay=40.0), .6)
write("game_over.wav", notes([(392.0, .2), (369.99, .2), (349.23, .2), (261.63, .7)],
                             duty=.5, decay=2.4))
write("start.wav", notes([(392.0, .07), (523.25, .07), (659.25, .07), (1046.5, .24)],
                         duty=.125, decay=6.0))


# --- music -----------------------------------------------------------------

NOTE = {name: 440.0 * 2 ** ((index - 9) / 12) for index, name in enumerate(
    ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"])}


def freq(note: str) -> float:
    if note == "-":
        return 0.0
    return NOTE[note[:-1]] * 2 ** (int(note[-1]) - 4)


def render_song(bpm: float, lead: list[str], bass: list[str], chords: list[list[str]],
                duty: float, swing_hat: bool) -> np.ndarray:
    step = 60.0 / bpm / 4.0
    steps = len(lead)
    count = int(round(step * steps * RATE))
    out = np.zeros(count)
    step_count = int(round(step * RATE))

    def place(start_step: int, samples: np.ndarray, gain: float) -> None:
        start = int(round(start_step * step * RATE))
        end = min(count, start + samples.size)
        out[start:end] += samples[:end - start] * gain

    for index, note in enumerate(lead):
        if note in ("-", "."):
            continue
        length = 1
        while index + length < steps and lead[index + length] == ".":
            length += 1
        size = step_count * length
        tone = pulse(freq(note) * (1 + .006 * np.sin(np.arange(size) / RATE * 2 * np.pi * 5.5)),
                     size, duty)
        place(index, tone * envelope(size, decay=2.2 / length), .30)
    for index, note in enumerate(bass):
        if note in ("-", "."):
            continue
        size = step_count * 2
        place(index, triangle(freq(note), size) * envelope(size, attack=.002, decay=3.0), .55)
    bar = 16
    for bar_index, chord in enumerate(chords):
        for beat in range(8):
            note = chord[beat % len(chord)]
            size = step_count
            tone = pulse(freq(note), size, .125) * envelope(size, decay=14.0)
            place(bar_index * bar + beat * 2 + 1, tone, .11)
    for index in range(steps):
        beat = index % 16
        if beat in (0, 8) or (beat == 10 and swing_hat):
            size = int(.12 * RATE)
            kick = np.sin(2 * np.pi * phase_of(np.geomspace(140, 42, size), size))
            place(index, kick * envelope(size, attack=.001, decay=22.0), .55)
        if beat in (4, 12):
            size = int(.1 * RATE)
            place(index, lowpass(noise(size), .5) * envelope(size, attack=.001, decay=26.0), .32)
        if index % 2 == 1:
            size = int(.03 * RATE)
            place(index, noise(size) * envelope(size, attack=.0005, decay=90.0), .10)
    return lowpass(out, .55)


def bars(*items: str) -> list[str]:
    return " ".join(items).split()


day_lead = bars(
    "E5 . G5 . C6 . G5 . A5 . . G5 E5 . D5 .",
    "C5 . E5 . G5 . E5 . F5 . . E5 D5 . . .",
    "A4 . C5 . F5 . C5 . G5 . . F5 E5 . D5 .",
    "G4 . B4 . D5 . G5 . F5 . E5 . D5 . . .",
    "E5 . G5 . C6 . G5 . A5 . . C6 B5 . A5 .",
    "C5 . E5 . A5 . E5 . G5 . . E5 D5 . C5 .",
    "F5 . A5 . C6 . A5 . G5 . E5 . D5 . G4 .",
    "C5 . E5 . G5 . E5 . C5 . . . - . . .",
)
day_bass = bars(
    "C3 - C3 - G2 - C3 - C3 - C3 - G2 - C3 -",
    "A2 - A2 - E3 - A2 - A2 - A2 - E3 - A2 -",
    "F2 - F2 - C3 - F2 - F2 - F2 - C3 - F2 -",
    "G2 - G2 - D3 - G2 - G2 - G2 - D3 - B2 -",
    "C3 - C3 - G2 - C3 - C3 - C3 - G2 - C3 -",
    "A2 - A2 - E3 - A2 - A2 - A2 - E3 - A2 -",
    "F2 - F2 - C3 - F2 - G2 - G2 - D3 - G2 -",
    "C3 - C3 - G2 - C3 - C3 - G2 - E2 - G2 -",
)
day_chords = [["C5", "E5", "G5"], ["A4", "C5", "E5"], ["F4", "A4", "C5"], ["G4", "B4", "D5"]] * 2
write("music.wav", render_song(150, day_lead, day_bass, day_chords, .25, False), .7)

night_lead = bars(
    "A4 . . C5 E5 . . A5 G5 . E5 . D5 . E5 .",
    "F4 . . A4 C5 . . F5 E5 . C5 . A4 . . .",
    "C5 . . E5 G5 . . C6 B5 . G5 . E5 . G5 .",
    "G4 . . B4 D5 . . G5 F5 . D5 . B4 . . .",
    "A4 . . C5 E5 . . A5 B5 . C6 . B5 . A5 .",
    "F4 . . A4 C5 . . F5 G5 . A5 . G5 . F5 .",
    "E5 . . G5 C6 . . G5 D5 . F5 . B5 . G5 .",
    "A5 . . E5 C5 . . A4 - . . . E4 . G#4 .",
)
night_bass = bars(
    "A2 - A2 A3 A2 - E3 - A2 - A2 A3 G2 - E2 -",
    "F2 - F2 F3 F2 - C3 - F2 - F2 F3 E2 - C3 -",
    "C3 - C3 C4 C3 - G2 - C3 - C3 C4 B2 - G2 -",
    "G2 - G2 G3 G2 - D3 - G2 - G2 G3 B2 - D3 -",
    "A2 - A2 A3 A2 - E3 - A2 - A2 A3 G2 - E2 -",
    "F2 - F2 F3 F2 - C3 - F2 - F2 F3 E2 - C3 -",
    "C3 - C3 C4 G2 - G2 G3 G2 - D3 - B2 - G2 -",
    "A2 - A2 A3 E2 - E2 E3 E2 - E2 - G#2 - B2 -",
)
night_chords = [["A4", "C5", "E5"], ["F4", "A4", "C5"], ["C5", "E5", "G5"],
                ["G4", "B4", "D5"]] * 2
write("music_night.wav",
      render_song(132, night_lead, night_bass, night_chords, .125, True), .7)
