from pathlib import Path
import array
import math
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import wave


ENGINE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ENGINE / "tools"))

import pack_game_assets  # noqa: E402


RATE = 24000


def write_wav(path: Path, samples: list[int]) -> None:
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(array.array("h", samples).tobytes())


def sine(seconds: float, freq: float = 440.0, amplitude: int = 20000) -> list[int]:
    return [int(amplitude * math.sin(2 * math.pi * freq * i / RATE))
            for i in range(int(RATE * seconds))]


def square(seconds: float, freq: float = 180.0, amplitude: int = 26000) -> list[int]:
    return [amplitude if (i * freq / RATE) % 1.0 < .25 else -amplitude
            for i in range(int(RATE * seconds))]


def snr_db(reference: list[int], decoded: list[int]) -> float:
    signal = sum(value * value for value in reference[1:])
    noise = sum((a - b) ** 2 for a, b in zip(reference[1:], decoded[1:]))
    return 10 * math.log10(signal / noise) if noise else 99.0


class SoundPackingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.directory = tempfile.TemporaryDirectory()
        root = Path(cls.directory.name)
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if not compiler:
            raise unittest.SkipTest("a C compiler is required")
        audio = ENGINE / "src/audio"
        cls.decoder = root / ("decode.exe" if os.name == "nt" else "decode")
        subprocess.run([
            compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
            f"-I{audio / 'include'}", f"-I{audio}", f"-I{ENGINE / 'include/raylib_lite'}",
            str(ENGINE / "tests/test_sound_packing.c"),
            str(audio / "raylib_lite_audio_decode.c"),
            "-o", str(cls.decoder),
        ], check=True)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.directory.cleanup()

    def pack(self, name: str, samples: list[int], encoding: str = "auto") -> tuple[dict, list[int]]:
        root = Path(self.directory.name)
        source = root / f"{name}.wav"
        destination = root / f"{name}.sound"
        raw = root / f"{name}.raw"
        write_wav(source, samples)
        report = pack_game_assets.write_sound(source, destination, encoding)
        subprocess.run([str(self.decoder), str(destination), str(raw)], check=True)
        decoded = array.array("h")
        decoded.frombytes(raw.read_bytes())
        return report, list(decoded)

    def test_short_smooth_clip_uses_adpcm_and_device_decoder_matches_report(self) -> None:
        samples = sine(.3)
        report, decoded = self.pack("short_sine", samples)
        self.assertEqual(report["bits"], 4)
        self.assertEqual(len(decoded), len(samples))
        self.assertGreaterEqual(report["adpcm_snr_db"], pack_game_assets.SOUND_ADPCM_MIN_SNR_DB)
        self.assertAlmostEqual(snr_db(samples, decoded), report["adpcm_snr_db"], delta=.1)
        self.assertLess(report["bytes"], len(samples))

    def test_attack_starts_from_a_matching_step_size(self) -> None:
        samples = [0] * 4 + [24000] * 400
        report, decoded = self.pack("attack", samples, "adpcm")
        self.assertGreater(Path(self.directory.name, "attack.sound").read_bytes()[22], 0)
        self.assertLess(max(abs(a - b) for a, b in zip(samples[6:], decoded[6:])), 4000)
        self.assertAlmostEqual(snr_db(samples, decoded), report["adpcm_snr_db"], delta=.1)

    def test_short_hard_edged_clip_stays_pcm16(self) -> None:
        samples = square(.3)
        report, decoded = self.pack("short_square", samples)
        self.assertEqual(report["bits"], 16)
        self.assertLess(report["adpcm_snr_db"], pack_game_assets.SOUND_ADPCM_MIN_SNR_DB)
        self.assertEqual(decoded, samples)

    def test_long_clip_uses_adpcm_and_encoding_overrides_apply(self) -> None:
        self.assertEqual(self.pack("long_square", square(1.0))[0]["bits"], 4)
        report, decoded = self.pack("forced_pcm", sine(1.0), "pcm16")
        self.assertEqual(report["bits"], 16)
        self.assertNotIn("adpcm_snr_db", report)
        self.assertEqual(len(decoded), RATE)
        self.assertEqual(self.pack("forced_adpcm", square(.3), "adpcm")[0]["bits"], 4)
        with self.assertRaises(ValueError):
            self.pack("bad", sine(.1), "mp3")


if __name__ == "__main__":
    unittest.main()
