#!/usr/bin/env python3
"""Generate Neon Rift Rally's deterministic synthwave cues and course loops.

Every clip is synthesized from oscillators and seeded noise, so the output is
original and reproducible. One music voice is available on device, so each
course loop carries its own low engine bed instead of a separate engine clip.
"""
from __future__ import annotations

from pathlib import Path
import wave

import numpy as np

RATE = 24000
ROOT = Path(__file__).resolve().parent
RNG = np.random.default_rng(0x52494654)


def write(name: str, samples: np.ndarray, peak: float = .84) -> None:
    samples = np.asarray(samples, dtype=np.float64)
    top = float(np.max(np.abs(samples))) or 1.0
    pcm = np.clip(samples * (peak / top), -1.0, 1.0)
    with wave.open(str(ROOT / name), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes((pcm * 32767).astype("<i2").tobytes())


def frames(seconds: float) -> int:
    return int(round(RATE * seconds))


def phase_of(freq, count: int) -> np.ndarray:
    freq = np.broadcast_to(np.asarray(freq, dtype=np.float64), (count,))
    return np.cumsum(freq) / RATE


def saw(freq, count: int) -> np.ndarray:
    return 2.0 * (phase_of(freq, count) % 1.0) - 1.0


def square(freq, count: int, duty: float = .5) -> np.ndarray:
    return np.where((phase_of(freq, count) % 1.0) < duty, 1.0, -1.0)


def sine(freq, count: int) -> np.ndarray:
    return np.sin(2 * np.pi * phase_of(freq, count))


def supersaw(freq: float, count: int, detune: float = .007) -> np.ndarray:
    return (saw(freq, count) + saw(freq * (1 + detune), count) +
            saw(freq * (1 - detune), count)) / 3.0


def noise(count: int) -> np.ndarray:
    return RNG.uniform(-1.0, 1.0, count)


def smooth(samples: np.ndarray, width: int) -> np.ndarray:
    """Boxcar low-pass; cheap enough to run on every Host launch."""
    if width <= 1:
        return samples
    padded = np.concatenate([np.zeros(width), samples])
    acc = np.cumsum(padded)
    return (acc[width:] - acc[:-width]) / width


def band(samples: np.ndarray, low: int, high: int) -> np.ndarray:
    return smooth(samples, high) - smooth(samples, low)


def env(count: int, attack: float = .004, decay: float = 6.0,
        release: float = .01) -> np.ndarray:
    t = np.arange(count) / RATE
    shape = np.minimum(1.0, t / max(attack, 1e-4)) * np.exp(-t * decay)
    tail = frames(release)
    if tail and count > tail:
        shape[-tail:] *= np.linspace(1.0, 0.0, tail)
    return shape


NOTE = {name: 440.0 * 2 ** ((index - 9) / 12) for index, name in enumerate(
    ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"])}


def hz(note: str) -> float:
    return NOTE[note[:-1]] * 2 ** (int(note[-1]) - 4)


# --- cues ------------------------------------------------------------------

def tone_seq(seq, wave_fn=square, decay=7.0, gain=1.0):
    parts = []
    for note, seconds in seq:
        count = frames(seconds)
        parts.append(wave_fn(hz(note), count) * env(count, decay=decay) * gain)
    return np.concatenate(parts)


n = frames(.13)
write("countdown.wav", smooth(square(880, n, .5), 3) * env(n, decay=10.0), .7)

n = frames(.55)
go = (supersaw(hz("A5"), n) + .7 * supersaw(hz("E5"), n) + .5 * square(hz("A4"), n, .25))
write("go.wav", smooth(go, 2) * env(n, attack=.002, decay=4.2))

n = frames(.55)
rise = np.geomspace(120, 980, n)
whoosh = band(noise(n), 2, 9) * np.linspace(.2, 1.0, n)
write("boost.wav", (smooth(saw(rise, n), 3) * .6 + whoosh * .7) * env(n, attack=.02, decay=3.0))

n = frames(.28)
screech = sine(1180 + 90 * np.sin(np.arange(n) / RATE * 2 * np.pi * 34), n)
write("drift.wav", (band(noise(n), 2, 6) * .8 + screech * .25) * env(n, attack=.01, decay=7.0), .6)

n = frames(.26)
write("jump.wav", smooth(square(np.geomspace(220, 780, n), n, .3), 3) * env(n, decay=6.0), .66)

n = frames(.22)
thump = sine(np.geomspace(140, 45, n), n) * env(n, attack=.001, decay=16.0)
write("land.wav", thump + .5 * smooth(noise(n), 6) * env(n, attack=.001, decay=30.0))

bell = lambda f, c: sine(f, c) + .35 * sine(f * 3.01, c) + .15 * sine(f * 5.02, c)
write("checkpoint.wav", tone_seq([("E6", .07), ("B6", .26)], bell, decay=8.0), .64)

write("lap.wav", tone_seq([("A5", .08), ("C#6", .08), ("E6", .08), ("A6", .42)],
                          lambda f, c: supersaw(f, c) * .7 + bell(f, c) * .5, decay=5.0))

fanfare_notes = [("A4", .14), ("C#5", .14), ("E5", .14), ("A5", .3),
                 ("F#5", .14), ("G#5", .14), ("A5", .7)]
fanfare = tone_seq(fanfare_notes, lambda f, c: supersaw(f, c) + .4 * square(f / 2, c, .25),
                   decay=2.4)
write("finish.wav", smooth(fanfare, 2))

n = frames(.95)
fall = np.geomspace(330, 70, n)
write("fail.wav", smooth(saw(fall, n) + saw(fall * 1.012, n), 4) *
      env(n, attack=.01, decay=2.6))

n = frames(.38)
grind = smooth(noise(n), 14) * (1 + .6 * square(23, n))
write("offtrack.wav", (grind + .4 * square(np.geomspace(150, 70, n), n, .5)) *
      env(n, attack=.01, decay=5.0), .7)

n = frames(.32)
ring = sine(1530, n) * .3 + sine(2410, n) * .2
write("collision.wav", (noise(n) * env(n, attack=.0005, decay=22.0) +
                        sine(np.geomspace(160, 50, n), n) * env(n, attack=.001, decay=10.0) +
                        ring * env(n, attack=.001, decay=14.0)))

n = frames(.36)
doppler = band(noise(n), 2, 7) * np.sin(np.linspace(0, np.pi, n)) ** 2
write("overtake.wav", doppler + .3 * smooth(square(np.geomspace(500, 1200, n), n, .25), 3) *
      env(n, attack=.02, decay=6.0), .7)

n = frames(.36)
doppler = band(noise(n), 3, 12) * np.sin(np.linspace(0, np.pi, n)) ** 2
write("overtaken.wav", doppler + .3 * smooth(square(np.geomspace(420, 180, n), n, .5), 3) *
      env(n, attack=.02, decay=5.0), .62)

n = frames(.22)
write("near_miss.wav", band(noise(n), 1, 4) * np.sin(np.linspace(0, np.pi, n)) ** 3 +
      .25 * sine(np.geomspace(1800, 2600, n), n) * env(n, decay=12.0), .62)

n = frames(.34)
shimmer = sum(sine(hz(note), n) for note in ("E6", "A6", "C#7"))
write("draft.wav", shimmer * env(n, attack=.06, decay=6.0) *
      (1 + .5 * np.sin(np.arange(n) / RATE * 2 * np.pi * 18)), .5)

# Another rider lights nitro nearby: a detuned engine snarl that passes.
n = frames(.48)
snarl = saw(np.geomspace(180, 420, n), n) + saw(np.geomspace(181.5, 424, n), n)
pass_by = np.sin(np.linspace(0, np.pi, n)) ** 1.5
write("rival.wav", (smooth(snarl, 4) * .55 + band(noise(n), 2, 8) * .5) * pass_by *
      env(n, attack=.03, decay=2.2), .6)


# --- course loops ----------------------------------------------------------

def render_loop(bpm: float, chords: list[tuple[str, str]], lead: list[str],
                arp_octave: int, bed_hz: float, bright: bool, bell_arp: bool) -> np.ndarray:
    """Eight bars of synthwave: pumping bass, pad, 16th arp, sparse lead, drums."""
    sixteenth = 60.0 / bpm / 4.0
    steps = 16 * len(chords)
    total = frames(sixteenth * steps)
    out = np.zeros(total)
    step = sixteenth * RATE

    def place(at_step: float, samples: np.ndarray, gain: float) -> None:
        start = int(round(at_step * step)) % total
        end = start + samples.size
        if end <= total:
            out[start:end] += samples * gain
        else:
            head = total - start
            out[start:] += samples[:head] * gain
            out[:end - total] += samples[head:] * gain

    duck = np.ones(total)
    beat = int(round(step * 4))
    curve = 1.0 - .55 * np.exp(-np.arange(beat) / (RATE * .07))
    for b in range(steps // 4):
        duck[b * beat:(b + 1) * beat] = curve[:min(beat, total - b * beat)]

    music = np.zeros(total)
    for bar, (root, quality) in enumerate(chords):
        base = hz(root + "2")
        third = 2 ** ((4 if quality == "maj" else 3) / 12)
        tones = [1.0, third, 2 ** (7 / 12)]
        size = int(step * 16)
        pad = sum(supersaw(base * 4 * t, size, .009) for t in tones)
        pad = smooth(pad, 7 if bright else 10) * env(size, attack=.18, decay=.25, release=.08)
        start = bar * size
        music[start:start + size] += pad[:total - start] * .16
        for eighth in range(8):
            count = int(step * 2)
            octave = 2 if eighth % 2 else 1
            tone = smooth(saw(base * octave, count) + .5 * square(base * octave, count, .5), 5)
            place(bar * 16 + eighth * 2, tone * env(count, attack=.002, decay=9.0), .34)
        arp_tones = tones + [2.0, tones[1] * 2, tones[2], 2.0, tones[1]]
        for s in range(16):
            count = int(step * .9)
            f = base * 2 ** (arp_octave - 2) * arp_tones[s % len(arp_tones)]
            tone = (sine(f, count) + .4 * sine(f * 3.01, count)) if bell_arp else \
                smooth(square(f, count, .25), 2)
            place(bar * 16 + s, tone * env(count, attack=.001, decay=14.0), .10)
    for index, note in enumerate(lead):
        if note in ("-", "."):
            continue
        length = 1
        while index + length < steps and lead[index + length] == ".":
            length += 1
        count = int(step * length)
        vibrato = 1 + .004 * np.sin(np.arange(count) / RATE * 2 * np.pi * 5.2)
        tone = smooth(supersaw(hz(note), count, .005) * vibrato[0] +
                      .4 * square(hz(note) * vibrato, count, .5), 4 if bright else 6)
        place(index, tone * env(count, attack=.012, decay=1.4 / length), .17)
    out += music
    out *= duck
    for s in range(steps):
        pos = s % 16
        if pos % 4 == 0:
            count = frames(.16)
            kick = sine(np.geomspace(150, 44, count), count) * env(count, attack=.001, decay=18.0)
            place(s, kick, .62)
        if pos in (4, 12):
            count = frames(.24)
            snare = band(noise(count), 1, 5) * env(count, attack=.001, decay=13.0)
            snare += .35 * sine(190, count) * env(count, attack=.001, decay=30.0)
            place(s, snare, .34)
        if pos % 4 == 2:
            count = frames(.04)
            place(s, band(noise(count), 1, 2) * env(count, attack=.0005, decay=70.0), .12)
    # The engine bed: two beating pulses under the mix, tracked by volume on device.
    # Whole cycles per loop keep the bed seamless at the wrap point.
    t = np.arange(total) / RATE
    seconds = total / RATE
    whole = lambda f: max(1.0, round(f * seconds)) / seconds
    wobble = whole(.5)
    bed = square(whole(bed_hz) * (1 + .01 * np.sin(2 * np.pi * wobble * t)), total, .4) + \
        square(whole(bed_hz * 1.503), total, .5) * .5
    bed = smooth(np.concatenate([bed[-18:], bed]), 18)[18:]
    out += bed * .07 * (1 + .25 * np.sin(2 * np.pi * whole(11) * t))
    return out


def bars(*rows: str) -> list[str]:
    return " ".join(rows).split()


write("race_neon.wav", render_loop(
    118, [("A", "min"), ("F", "maj"), ("C", "maj"), ("G", "maj")] * 2,
    bars("A4 . . . C5 . E5 . . . D5 . C5 . . .",
         "C5 . . . A4 . . . F4 . . . - . . .",
         "G4 . . . C5 . E5 . . . G5 . E5 . . .",
         "D5 . . . . . B4 . . . G4 . - . . .",
         "A4 . . . C5 . E5 . . . A5 . G5 . . .",
         "F5 . . . E5 . C5 . . . A4 . C5 . . .",
         "E5 . . . G5 . C6 . . . B5 . G5 . . .",
         "D5 . . . . . . . B4 . . . - . . ."),
    4, 47.0, True, False), .74)

write("race_sunset.wav", render_loop(
    108, [("D", "maj"), ("B", "min"), ("G", "maj"), ("A", "maj")] * 2,
    bars("F#5 . . . . . A5 . . . F#5 . E5 . D5 .",
         "D5 . . . B4 . . . . . - . . . . .",
         "B4 . . . D5 . G5 . . . F#5 . E5 . D5 .",
         "E5 . . . . . C#5 . . . A4 . - . . .",
         "F#5 . . . . . A5 . . . D6 . C#6 . A5 .",
         "B5 . . . . . F#5 . . . D5 . F#5 . . .",
         "G5 . . . F#5 . E5 . . . D5 . B4 . . .",
         "A4 . . . . . C#5 . . . E5 . - . . ."),
    4, 41.0, True, False), .74)

write("race_polar.wav", render_loop(
    128, [("E", "min"), ("C", "maj"), ("D", "maj"), ("B", "min")] * 2,
    bars("B4 . . . . . . . E5 . . . G5 . F#5 .",
         "E5 . . . . . . . - . . . . . . .",
         "D5 . . . . . F#5 . . . A5 . . . F#5 .",
         "F#5 . . . . . . . - . . . . . . .",
         "B4 . . . . . . . E5 . . . B5 . A5 .",
         "G5 . . . . . E5 . . . . . - . . .",
         "F#5 . . . A5 . . . D6 . . . C#6 . A5 .",
         "B5 . . . . . . . - . . . . . . ."),
    5, 52.0, False, True), .74)
