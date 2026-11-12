# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / 'examples/last_zone_extraction'

class LastZoneFloorTests(unittest.TestCase):
    def test_hidden_columns_preserve_visible_material_and_samples(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            ids = sorted(set(re.findall(r'RAYLIB_LITE_ASSET_ID_[A-Z0-9_]+',
                                       (PROJECT/'main/last_zone_view.c').read_text())))
            (temp/'assets_ids.h').write_text('\n'.join(
                f'#define {name} {i+1}U' for i, name in enumerate(ids)))
            command = [os.environ.get('CC', 'cc'), '-std=c11', '-O2',
                       '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-fdata-sections']
            command += shlex.split(os.environ.get('CFLAGS', ''))
            command += [str(Path(__file__).with_suffix('.c')),
                        str(PROJECT/'main/last_zone_game.c')]
            for include in [temp, PROJECT/'main', ROOT/'host/include', ROOT/'include/raylib_lite',
                            ROOT/'compat/raylib/include',
                            ROOT/'examples/common_components/examples_common/include']:
                command += ['-I', str(include)]
            command += ['-Wl,--gc-sections', '-lm', '-o', str(temp/'floor')]
            subprocess.run(command, check=True)
            subprocess.run([str(temp/'floor')], check=True)
