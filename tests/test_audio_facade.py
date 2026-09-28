from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ENGINE = Path(__file__).resolve().parents[1]
PORT = ENGINE / "ports/esp_mosaico"
FAKES = ENGINE / "tests/fakes/esp_mosaico_port"

class AudioFacadeTests(unittest.TestCase):
    def test_stale_raylib_handle_is_safe(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            exe = Path(directory) / ("audio_facade.exe" if os.name == "nt" else "audio_facade")
            cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
            self.assertIsNotNone(cc)
            audio = PORT / "mosaico_game_audio"
            subprocess.run([
                cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{FAKES / 'audio'}",
                f"-I{ENGINE / 'components/mosaico_game_audio/include'}",
                f"-I{audio}",
                f"-I{ENGINE / 'components/mosaico_game_assets/include'}",
                f"-I{PORT / 'platform_esp_audio/include'}",
                f"-I{ENGINE / 'components/raylib_lite_platform/include'}",
                f"-I{ENGINE / 'examples/tomb_explorer/managed_components/georgik__raylib/raylib/src'}",
                f"-I{ENGINE / 'host/include'}",
                str(ENGINE / "tests/test_audio_facade.c"),
                str(audio / "mosaico_game_audio.c"),
                str(ENGINE / "components/mosaico_game_audio/raylib_lite_audio_decode.c"),
                str(ENGINE / "components/mosaico_game_audio/raylib_lite_audio_mixer.c"),
                "-o", str(exe),
            ], check=True)
            result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "audio facade: ok")

if __name__ == "__main__":
    unittest.main()
