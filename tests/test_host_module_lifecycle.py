# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class HostModuleLifecycleTests(unittest.TestCase):
    def test_partial_initialization_rolls_back_and_can_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            binary=Path(directory)/'lifecycle'
            command=['cc','-std=c11','-Wall','-Wextra','-Werror']
            for inc in ['host','host/include','include/raylib_lite','compat/raylib/include',
                        'examples/common_components/examples_common/include']:
                command+=['-I',str(ROOT/inc)]
            command += [str(ROOT/'tests/test_host_module_lifecycle.c'),str(ROOT/'host/host_module_bridge.c'),'-o',str(binary)]
            subprocess.run(command,check=True)
            subprocess.run([str(binary)],check=True)
