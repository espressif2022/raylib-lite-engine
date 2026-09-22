# SPDX-License-Identifier: Apache-2.0
"""Run standalone raster correctness tests; benchmark is informational only."""
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ColumnTests(unittest.TestCase):
    def test_exact_sampling(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            pixels = struct.pack('<64H', *[(i * 997 + 123) & 65535 for i in range(64)])
            (temp / 'test.atlas').write_bytes(struct.pack('<4sHHHHII', b'MSA1', 8, 8, 0, 0, 128, 0) + pixels)
            light_lut = struct.pack('<4096H', *[
                (level * 257 + index * 997 + 31) & 65535
                for level in range(16) for index in range(256)])
            indices = bytes(y * 8 + x for x in range(8) for y in range(8))
            (temp / 'test.wall').write_bytes(
                struct.pack('<4sHHHHII', b'MSW1', 8, 8, 0, 16, 256, 64) +
                light_lut + indices)
            row_indices = bytes(y * 8 + x for y in range(8) for x in range(8))
            (temp / 'test_row.wall').write_bytes(
                struct.pack('<4sHHHHII', b'MSW2', 8, 8, 0, 16, 256, 64) +
                light_lut + row_indices)
            source = os.environ.get('M2D_TEST_SOURCE', str(ROOT / 'components/mosaico_game_2d/mosaico_game_2d.c'))
            command = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror']
            command += shlex.split(os.environ.get('CFLAGS', ''))
            command += [str(ROOT / 'tests/test_columns.c'), str(ROOT / 'host/host_asset_runtime.c'), source]
            rgb565 = ROOT / 'components/mosaico_game_2d/mosaico_rgb565.c'
            if Path(source).resolve() != rgb565.resolve():
                command.append(str(rgb565))
            for include in ['host/include', 'host', 'components/mosaico_game_assets/include', 'components/mosaico_game_2d/include']:
                command += ['-I', str(ROOT / include)]
            command += ['-lm', '-o', str(temp / 'columns')]
            subprocess.run(command, check=True)
            subprocess.run([str(temp / 'columns'), str(temp)], check=True)

if __name__ == '__main__':
    unittest.main()
