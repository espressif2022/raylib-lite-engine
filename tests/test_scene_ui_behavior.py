# SPDX-License-Identifier: Apache-2.0
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class SceneUiBehaviorTests(unittest.TestCase):
    def run_behavior(self, module):
        with tempfile.TemporaryDirectory() as directory:
            executable=str(Path(directory)/module)
            command=[os.environ.get('CC','cc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic']
            command+=shlex.split(os.environ.get('CFLAGS',''))
            for include in ['include/raylib_lite','compat/raylib/include','host/include']:
                command+=['-I',str(ROOT/include)]
            command+=[str(ROOT/f'tests/test_{module}_behavior.c'),
                      str(ROOT/f'src/{module}/raylib_lite_{module}.c'),'-o',executable]
            subprocess.run(command,check=True)
            result=subprocess.run([executable],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(result.stdout.strip(),f'{module} behavior: ok')

    def test_scene_callback_order_capacity_and_cleanup(self):
        self.run_behavior('scene')

    def test_ui_pointer_focus_capacity_and_draw_contract(self):
        self.run_behavior('ui')


if __name__=='__main__':unittest.main()
