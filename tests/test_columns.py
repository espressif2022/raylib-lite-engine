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
            source = os.environ.get('M2D_TEST_SOURCE', str(ROOT / 'components/mosaico_game_2d/mosaico_game_2d.c'))
            command = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror']
            command += shlex.split(os.environ.get('CFLAGS', ''))
            command += [str(ROOT / 'tests/test_columns.c'), str(ROOT / 'host/host_asset_runtime.c'), source]
            for include in ['host/include', 'host', 'components/mosaico_game_assets/include', 'components/mosaico_game_2d/include']:
                command += ['-I', str(ROOT / include)]
            command += ['-lm', '-o', str(temp / 'columns')]
            subprocess.run(command, check=True)
            subprocess.run([str(temp / 'columns'), str(temp)], check=True)

if __name__ == '__main__':
    unittest.main()
