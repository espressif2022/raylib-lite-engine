# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class RunnerTests(unittest.TestCase):
    def test_fake_clock_and_input_queue(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "runner_test"
            compiler = shutil.which(os.environ.get("CC", "cc"))
            self.assertIsNotNone(compiler, "a C compiler is required")
            command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror"]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            command += [
                str(ROOT / "tests/test_runner.c"),
                str(ROOT / "components/raylib_lite_runner/raylib_lite_runner.c"),
                str(ROOT / "components/raylib_lite_runner/raylib_lite_input_queue.c"),
                "-I", str(ROOT / "components/raylib_lite_runner/include"),
                "-I", str(ROOT / "components/raylib_lite_platform/include"),
                "-pthread", "-o", str(executable),
            ]
            subprocess.run(command, check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "runner and input queue: ok")


if __name__ == "__main__":
    unittest.main()
