"""Inject rejected and delayed LCD transfers into the real BOX-3 presenter."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "examples/boards/esp32-s3-box-3"
FAKES = ROOT / "tests/fakes/box3_video"


class Box3TransferCleanupTests(unittest.TestCase):
    def test_rejected_strips_and_inflight_timeouts_have_distinct_cleanup(self):
        cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if cc is None:
            self.skipTest("C compiler not installed")
        with tempfile.TemporaryDirectory(prefix="rle_box3_transfer_") as directory:
            output = Path(directory) / "box3_lcd_transfer_test"
            subprocess.run(
                [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                 "-I", str(FAKES), "-I", str(BOARD),
                 "-I", str(ROOT / "include/raylib_lite"),
                 str(ROOT / "tests/test_box3_transfer_cleanup.c"),
                 str(BOARD / "box3_video.c"),
                 "-o", str(output)],
                check=True, capture_output=True, text=True)
            result = subprocess.run(
                [str(output)], check=True, capture_output=True, text=True)
            self.assertEqual(result.stdout.strip(),
                             "BOX-3 LCD pending-transfer cleanup: ok")


if __name__ == "__main__":
    unittest.main()
