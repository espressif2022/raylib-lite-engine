"""Fault-inject real ESP-Mosaico and BOX-3 Codec backend stop paths."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FAKES = ROOT / "tests/fakes/board_audio"
AUDIO = ROOT / "examples/common_components/examples_audio"
BOARDS = ROOT / "examples/boards"


class BoardAudioCodecTests(unittest.TestCase):
    def test_codec_close_and_board_deinit_failure_can_retry(self):
        cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if cc is None:
            self.skipTest("C compiler not installed")
        with tempfile.TemporaryDirectory(prefix="rle_board_codec_") as directory:
            for board, macro, source in (
                ("ESP-Mosaico", "TEST_MOSAICO", "esp-mosaico/board_audio_codec.c"),
                ("ESP32-S3-BOX-3", "TEST_BOX3", "esp32-s3-box-3/box3_audio_codec.c"),
            ):
                with self.subTest(board=board):
                    output = Path(directory) / macro.lower()
                    subprocess.run([
                        cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-I", str(FAKES), "-I", str(AUDIO),
                        "-I", str(ROOT / "include/raylib_lite"),
                        f"-D{macro}=1",
                        str(ROOT / "tests/test_board_audio_codec.c"),
                        str(BOARDS / source),
                        "-o", str(output),
                    ], check=True, capture_output=True, text=True)
                    result = subprocess.run([str(output)], check=True,
                                            capture_output=True, text=True)
                    self.assertEqual(result.stdout.strip(), "board audio codec lifecycle: ok")


if __name__ == "__main__":
    unittest.main()
