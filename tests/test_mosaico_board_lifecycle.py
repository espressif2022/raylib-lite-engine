"""Fault-inject the actual Mosaico Board's cleanup and input overflow paths."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MosaicoBoardLifecycleTests(unittest.TestCase):
    def test_cleanup_can_retry_and_touch_release_survives_queue_overflow(self):
        cc = shutil.which('cc')
        if not cc:
            self.skipTest('C compiler unavailable')
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            (folder / 'esp_err.h').write_text('typedef int esp_err_t;\n')
            source = (ROOT / 'examples/boards/esp-mosaico/board.c').read_text()
            # Substitute hardware headers, keeping every implementation function.
            (folder / 'board_under_test.inc').write_text('\n'.join(
                line for line in source.splitlines() if not line.startswith('#include ')))
            binary = folder / 'board'
            result = subprocess.run([
                cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-I', str(folder), '-I', str(ROOT / 'tests/fakes/mosaico_board'),
                '-I', str(ROOT / 'include/raylib_lite'),
                '-I', str(ROOT / 'examples/common_components/examples_common/include'),
                str(ROOT / 'tests/test_mosaico_board_lifecycle.c'), '-o', str(binary),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
