from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ENGINE = Path(__file__).resolve().parents[1]


class GameActionTests(unittest.TestCase):
    def test_raylib_input_edges_and_imu_mapping(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "game_action.exe" if os.name == "nt" else "game_action")
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{ENGINE / 'include/raylib_lite'}",
                f"-I{ENGINE / 'include/raylib_lite'}",
                f"-I{ENGINE / 'include/raylib_lite'}",
                str(ENGINE / "tests/test_game_action.c"),
                str(ENGINE / "src/input/mosaico_game_action.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "game action: ok")


if __name__ == "__main__":
    unittest.main()
