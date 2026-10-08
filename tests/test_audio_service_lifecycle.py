"""Host fault injection for Audio Service worker join / backend teardown."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
AUDIO = ROOT / "examples/common_components/examples_audio"
FAKES = ROOT / "tests/fakes/audio_service"


class AudioServiceLifecycleTests(unittest.TestCase):
    def test_backend_stop_error_is_retryable_after_worker_join(self):
        cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if cc is None:
            self.skipTest("C compiler not installed")
        with tempfile.TemporaryDirectory(prefix="rle_audio_service_") as directory:
            output = Path(directory) / "audio_service_lifecycle"
            subprocess.run([
                cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-I", str(FAKES), "-I", str(AUDIO),
                "-I", str(ROOT / "include/raylib_lite"),
                str(ROOT / "tests/test_audio_service_lifecycle.c"),
                str(AUDIO / "platform_esp_audio.c"),
                str(AUDIO / "platform_audio_write_all.c"),
                "-o", str(output),
            ], check=True, capture_output=True, text=True)
            result = subprocess.run([str(output)], check=True,
                                    capture_output=True, text=True)
            self.assertEqual(result.stdout.strip(), "audio service lifecycle: ok")


if __name__ == "__main__":
    unittest.main()
