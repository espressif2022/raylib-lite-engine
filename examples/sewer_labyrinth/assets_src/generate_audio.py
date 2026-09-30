"""Original, deterministic short mechanical cues; mono PCM, no music loop."""
import math
from pathlib import Path
import random
import struct
import wave

ROOT = Path(__file__).resolve().parent
RATE = 16000


def write(name, duration, frequency, noise, gain):
    rng = random.Random(1701)
    frames = []
    count = round(RATE * duration)
    for i in range(count):
        t = i / RATE
        envelope = min(1, t / .008) * (1 - i / count) ** 1.7
        sweep = frequency * t + (frequency * .20 / duration) * t * t
        tone = math.sin(2 * math.pi * sweep)
        metal = math.sin(2 * math.pi * sweep * 2.73) * .18
        value = gain * envelope * ((tone + metal) * (1-noise) + rng.uniform(-1, 1) * noise)
        frames.append(struct.pack('<h', round(max(-1, min(1, value)) * 32767)))
    with wave.open(str(ROOT / (name + '.wav')), 'wb') as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(b''.join(frames))


if __name__ == '__main__':
    for spec in [('step', .12, 90, .65, .22), ('click', .16, 510, .45, .28),
                 ('power', .45, 160, .10, .30), ('pump', .65, 65, .22, .32),
                 ('alert', .35, 720, .02, .25), ('fail', .6, 120, .18, .28),
                 ('win', .6, 520, .01, .24)]:
        write(*spec)
