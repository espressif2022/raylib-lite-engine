from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[3]
PROJECT = ENGINE / "examples/tomb_raycast"


class TombRaycastTests(unittest.TestCase):
    def test_model_compiles_and_orbits(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "tomb-raycast-test.exe" if os.name == "nt" else "tomb-raycast-test"
            )
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{PROJECT / 'main'}",
                str(PROJECT / "tests/test_tomb_game.c"),
                str(PROJECT / "main/tomb_game.c"),
                str(PROJECT / "main/tomb_level_data.c"),
                "-lm", "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
