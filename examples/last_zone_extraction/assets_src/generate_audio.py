#!/usr/bin/env python3
"""Generate deterministic tactical feedback sounds and two music loops.

The cues are short procedural one-shots; the stealth and combat loops are
rendered with numpy so Host launches stay fast.
"""
from pathlib import Path
import math, random, struct, wave

import numpy as np

RATE = 24000
ROOT = Path(__file__).resolve().parent

def save(name, samples):
    pcm = b"".join(struct.pack("<h", max(-32767, min(32767, int(v * 32767))))
                   for v in samples)
    with wave.open(str(ROOT / name), "wb") as out:
        out.setnchannels(1); out.setsampwidth(2); out.setframerate(RATE)
        out.writeframes(pcm)

def rifle():
    rng = random.Random(724); count = int(RATE * .56); result = []; low = 0.0
    for i in range(count):
        t = i / RATE; noise = rng.uniform(-1, 1); low = low * .90 + noise * .10
        crack = noise * math.exp(-t * 74.0)
        body = math.sin(2 * math.pi * (92 - 38 * t) * t) * math.exp(-t * 10.5)
        pressure = low * math.exp(-t * 12.0)
        echo = 0.0
        for delay, gain in ((.085, .18), (.165, .11), (.285, .065)):
            if t > delay:
                local = t - delay
                echo += math.sin(2 * math.pi * (132 - 28 * local) * local) * \
                        math.exp(-local * 9.0) * gain
        result.append(.56 * crack + .46 * body + .20 * pressure + echo)
    return result

def impact():
    rng = random.Random(762); count = int(RATE * .11); result = []; low = 0.0
    for i in range(count):
        t = i / RATE; low = low * .68 + rng.uniform(-1, 1) * .32
        result.append((.35 * low + .22 * math.sin(2 * math.pi * 145 * t)) *
                      math.exp(-t * 31.0))
    return result

def confirm():
    count = int(RATE * .22); result = []
    for i in range(count):
        t = i / RATE; freq = 660 if t < .085 else 880
        local = t if t < .085 else t - .085
        result.append(.20 * math.sin(2 * math.pi * freq * t) * math.exp(-local * 13.0))
    return result

def hurt():
    rng = random.Random(311); count = int(RATE * .28); result = []; low = 0.0
    for i in range(count):
        t = i / RATE
        low = low * .88 + rng.uniform(-1, 1) * .12
        thump = math.sin(2 * math.pi * (74 - 22 * t) * t) * math.exp(-t * 10.0)
        crack = low * math.exp(-t * 19.0)
        result.append(.48 * thump + .25 * crack)
    return result

def empty():
    rng = random.Random(404); count = int(RATE * .09); result = []
    for i in range(count):
        t = i / RATE
        click = rng.uniform(-1, 1) * math.exp(-t * 70.0)
        ring = math.sin(2 * math.pi * 2100 * t) * math.exp(-t * 38.0)
        result.append(.22 * click + .18 * ring)
    return result

def pickup():
    count = int(RATE * .18); result = []
    for i in range(count):
        t = i / RATE
        freq = 520 if t < .07 else 780
        local = t if t < .07 else t - .07
        result.append(.22 * math.sin(2 * math.pi * freq * t) * math.exp(-local * 16.0))
    return result

def step(seed, pitch):
    rng = random.Random(seed)
    count = int(RATE * .10)
    result = []
    low = 0.0
    for i in range(count):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low = low * .70 + noise * .30
        thud = math.sin(2 * math.pi * (pitch - 36 * t) * t) * math.exp(-t * 26.0)
        grit = noise * math.exp(-t * 58.0) * (1.0 if t < .016 else .14)
        result.append(.40 * thud + .26 * grit + .14 * low * math.exp(-t * 20.0))
    return result

def alert():
    count = int(RATE * .20); result = []
    for i in range(count):
        t = i / RATE
        freq = 980 if t < .08 else 620
        local = t if t < .08 else t - .08
        result.append(.24 * math.sin(2 * math.pi * freq * t) * math.exp(-local * 14.0))
    return result

def whizz():
    """A round passing close: band-passed noise with a falling Doppler pitch."""
    rng = random.Random(907); count = int(RATE * .24); result = []; low = band = 0.0
    for i in range(count):
        t = i / RATE; noise = rng.uniform(-1, 1)
        low = low * .55 + noise * .45; band = band * .80 + (noise - low) * .20
        pitch = 2600 - 1900 * t / .24
        swell = math.sin(math.pi * min(1.0, t / .24)) ** 2
        result.append((.55 * band + .16 * math.sin(2 * math.pi * pitch * t)) * swell)
    return result

def radio():
    """A squelch click, two short clipped chirps and a release burst."""
    rng = random.Random(613); count = int(RATE * .34); result = []
    for i in range(count):
        t = i / RATE; noise = rng.uniform(-1, 1)
        value = noise * .30 * math.exp(-t * 90.0)
        for start, freq in ((.05, 1180), (.14, 1460)):
            local = t - start
            if 0 <= local < .07:
                tone = math.sin(2 * math.pi * freq * local + 3 * math.sin(2 * math.pi * 31 * local))
                value += max(-.6, min(.6, tone)) * .32
        if t > .26:
            value += noise * .22 * math.exp(-(t - .26) * 40.0)
        result.append(value)
    return result

def explode():
    rng = random.Random(501); count = int(RATE * .38); result = []; low = 0.0
    for i in range(count):
        t = i / RATE
        low = low * .90 + rng.uniform(-1, 1) * .10
        boom = math.sin(2 * math.pi * (52 - 18 * t) * t) * math.exp(-t * 6.5)
        grit = low * math.exp(-t * 9.0)
        result.append(.42 * boom + .28 * grit)
    return result

def extract():
    count = int(RATE * .36); result = []
    for i in range(count):
        t = i / RATE
        freq = 420 + 260 * t
        result.append(.18 * math.sin(2 * math.pi * freq * t) * math.exp(-t * 5.2))
    return result

def door():
    rng = random.Random(907); count = int(RATE * .42); result = []; low = 0.0
    for i in range(count):
        t = i / RATE; low = low * .94 + rng.uniform(-1, 1) * .06
        servo = math.sin(2 * math.pi * (180 + 140 * min(t / .3, 1.0)) * t)
        clunk = math.sin(2 * math.pi * 70 * t) * math.exp(-max(0.0, t - .32) * 40.0) \
            if t > .32 else 0.0
        result.append(.16 * servo * math.exp(-t * 3.0) + .9 * low * math.exp(-t * 4.0) +
                      .45 * clunk)
    return result

def death():
    count = int(RATE * 1.1); result = []
    for i in range(count):
        t = i / RATE
        freq = 220 * math.exp(-t * 1.4)
        beat = math.sin(2 * math.pi * freq * t) + .6 * math.sin(2 * math.pi * freq * 1.06 * t)
        thump = math.sin(2 * math.pi * 48 * t) * math.exp(-t * 7.0)
        result.append((.22 * beat * math.exp(-t * 2.2) + .4 * thump))
    return result

def deploy():
    count = int(RATE * .5); result = []
    for i in range(count):
        t = i / RATE
        freq = 440 if t < .12 else (587.33 if t < .24 else 880)
        local = t % .12 if t < .24 else t - .24
        result.append(.2 * math.sin(2 * math.pi * freq * t) * math.exp(-local * 9.0) +
                      .06 * math.sin(2 * math.pi * freq * 2 * t) * math.exp(-local * 14.0))
    return result


# --- music (numpy) -----------------------------------------------------------

NRNG = np.random.default_rng(0x4C5A)


def note_hz(name):
    names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    return 440.0 * 2 ** ((names.index(name[:-1]) - 9) / 12 + int(name[-1]) - 4)


def osc_phase(freq, count):
    return np.cumsum(np.broadcast_to(np.asarray(freq, float), (count,))) / RATE


def sine(freq, count):
    return np.sin(2 * np.pi * osc_phase(freq, count))


def saw(freq, count):
    return 2.0 * (osc_phase(freq, count) % 1.0) - 1.0


def smooth(samples, width):
    padded = np.concatenate([np.zeros(width), samples])
    acc = np.cumsum(padded)
    return (acc[width:] - acc[:-width]) / width


def smooth_loop(samples, width):
    """Circular smoothing: the filtered loop has no warm-up step at the wrap."""
    return smooth(np.concatenate([samples[-width:], samples]), width)[width:]


def decay_env(count, attack=.003, decay=8.0):
    t = np.arange(count) / RATE
    return np.minimum(1.0, t / attack) * np.exp(-t * decay)


class Loop:
    def __init__(self, bpm, bars):
        self.step = 60.0 / bpm / 4.0 * RATE
        self.total = int(round(self.step * 16 * bars))
        self.out = np.zeros(self.total)
        self.seconds = self.total / RATE

    def whole(self, freq):
        """Round a frequency to whole cycles per loop so the wrap is seamless."""
        return max(1.0, round(freq * self.seconds)) / self.seconds

    def place(self, at_step, samples, gain):
        start = int(round(at_step * self.step)) % self.total
        end = start + samples.size
        if end <= self.total:
            self.out[start:end] += samples * gain
        else:
            head = self.total - start
            self.out[start:] += samples[:head] * gain
            self.out[:end - self.total] += samples[head:] * gain

    def kick(self, at_step, gain, low=40.0):
        count = int(RATE * .2)
        body = sine(np.geomspace(120, low, count), count) * decay_env(count, .001, 14.0)
        self.place(at_step, body, gain)

    def noise_hit(self, at_step, gain, seconds, decay, width):
        count = int(RATE * seconds)
        hit = NRNG.uniform(-1, 1, count)
        hit = hit - smooth(hit, width)
        self.place(at_step, hit * decay_env(count, .0005, decay), gain)


def stealth_loop():
    loop = Loop(76, 6)
    t = np.arange(loop.total) / RATE
    swell = .6 + .4 * np.sin(2 * np.pi * loop.whole(1 / 9.0) * t)
    drone = smooth_loop(saw(loop.whole(note_hz("D2")), loop.total) +
                        .7 * saw(loop.whole(note_hz("D2") * 1.004), loop.total), 40)
    fifth = sine(loop.whole(note_hz("A2")), loop.total)
    loop.out += (drone * .22 + fifth * .10) * swell
    # A minor-second rub that swells in and out keeps the room uneasy.
    rub = sine(loop.whole(note_hz("D#3")), loop.total) * .05
    loop.out += rub * np.clip(np.sin(2 * np.pi * loop.whole(1 / 19.0) * t), 0, 1) ** 2
    wind = smooth_loop(NRNG.uniform(-1, 1, loop.total), 60)
    loop.out += wind * .5 * (.5 + .5 * np.sin(2 * np.pi * loop.whole(1 / 6.0) * t))
    for bar in range(6):
        base = bar * 16
        loop.kick(base, .55, 36)
        loop.kick(base + 3, .32, 34)
        for eighth in range(8):
            if (bar * 8 + eighth) % 5 != 3:
                loop.noise_hit(base + eighth * 2 + 1, .05, .03, 90.0, 3)
    pings = [(0, "D5"), (24, "F5"), (48, "A4"), (72, "C5")]
    for at, name in pings:
        count = int(RATE * 1.4)
        ping = sine(note_hz(name), count) + .3 * sine(note_hz(name) * 2.76, count)
        for echo, gain in ((0, .16), (6, .08), (12, .04)):
            loop.place(at + echo, ping * decay_env(count, .002, 4.0), gain)
    return loop.out


def combat_loop():
    loop = Loop(124, 6)
    roots = ["D2", "D2", "A#1", "C2", "D2", "A1"]
    for bar, root in enumerate(roots):
        base = bar * 16
        freq = note_hz(root)
        for s in range(16):
            count = int(loop.step * .9)
            octave = 2.0 if s in (3, 7, 11, 14) else 1.0
            tone = smooth(saw(freq * octave, count) + .5 * saw(freq * octave * 1.005, count), 6)
            loop.place(base + s, tone * decay_env(count, .002, 16.0), .30)
        stab = [freq * 4, freq * 4 * 2 ** (3 / 12), freq * 4 * 2 ** (7 / 12)]
        for at in (2, 10):
            count = int(loop.step * 1.6)
            chord = sum(smooth(saw(f, count), 4) for f in stab)
            loop.place(base + at, chord * decay_env(count, .004, 9.0), .07)
        for beat in range(4):
            loop.kick(base + beat * 4, .7)
        loop.noise_hit(base + 4, .42, .2, 16.0, 6)
        loop.noise_hit(base + 12, .42, .2, 16.0, 6)
        for s in range(16):
            loop.noise_hit(base + s, .07 if s % 2 else .11, .03, 80.0, 2)
    count = int(loop.step * 16)
    riser = NRNG.uniform(-1, 1, count) * np.linspace(0, 1, count) ** 2
    loop.place(5 * 16, riser - smooth(riser, 8), .16)
    t = np.arange(loop.total) / RATE
    loop.out += sine(loop.whole(note_hz("D1")), loop.total) * .12
    return loop.out


def save_np(name, samples, peak=.8):
    samples = np.asarray(samples, float)
    samples *= peak / (float(np.max(np.abs(samples))) or 1.0)
    save(name, samples.tolist())

save("rifle.wav", rifle())
save("impact.wav", impact())
save("confirm.wav", confirm())
save("hurt.wav", hurt())
save("empty.wav", empty())
save("pickup.wav", pickup())
save("alert.wav", alert())
save("step_l.wav", step(19, 88))
save("step_r.wav", step(41, 104))
save("explode.wav", explode())
save("extract.wav", extract())
save("door.wav", door())
save("death.wav", death())
save("deploy.wav", deploy())
save("whizz.wav", whizz())
save("radio.wav", radio())
save_np("music.wav", stealth_loop(), .62)
save_np("music_combat.wav", combat_loop(), .7)
