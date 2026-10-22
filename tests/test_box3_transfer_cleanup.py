"""Inject presenter submission failures and delayed quiescence into BOX-3 video."""
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
                [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", "-pthread",
                 "-I", str(FAKES), "-I", str(BOARD),
                 "-I", str(ROOT / "tests/fakes"),
                 "-I", str(ROOT / "tests/fakes/esp_mosaico_port/strip_present"),
                 "-I", str(ROOT / "examples/common_components/examples_common/include"),
                 "-I", str(ROOT / "include/raylib_lite"),
                 str(ROOT / "tests/test_box3_transfer_cleanup.c"),
                 str(BOARD / "box3_video.c"),
                 str(ROOT / "examples/common_components/examples_common/example_async_video.c"),
                 str(ROOT / "src/runner/raylib_lite_runtime_stats.c"),
                 "-o", str(output)],
                check=True, capture_output=True, text=True)
            result = subprocess.run(
                [str(output)], check=True, capture_output=True, text=True)
            self.assertEqual(result.stdout.strip(),
                             "BOX-3 LCD pending-transfer cleanup: ok")


if __name__ == "__main__":
    unittest.main()
