from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ENGINE = Path(__file__).resolve().parents[1]


class RuntimeStatsTests(unittest.TestCase):
    def test_portable_stats_and_clock_wrap(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "runtime_stats.exe" if os.name == "nt" else "runtime_stats")
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                f"-I{ENGINE / 'include/raylib_lite'}",
                str(ENGINE / "tests/test_runtime_stats.c"),
                str(ENGINE / "src/runner/raylib_lite_runtime_stats.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "runtime stats: ok")


if __name__ == "__main__":
    unittest.main()
