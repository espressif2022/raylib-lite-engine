#!/usr/bin/env python3
"""Exercise actual scene clipping with an independent area/visibility oracle."""
from pathlib import Path
import subprocess
import tempfile
project = Path(__file__).resolve().parents[1]
engine = project.parents[1]
with tempfile.TemporaryDirectory(prefix='rally_world_') as directory:
    binary = Path(directory) / 'check'
    subprocess.run(['cc', '-O2', '-ffunction-sections', '-fdata-sections',
                    '-I'+str(project/'main'), '-I'+str(project/'assets/generated'),
                    '-I'+str(engine/'include/raylib_lite'),
                    '-I'+str(engine/'host/include'),
                    '-I'+str(engine/'compat/raylib/include'),
                    str(project/'tests/check_world_projection.c'),
                    '-Wl,--gc-sections', '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
