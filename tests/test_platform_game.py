from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]


class PlatformGameTests(unittest.TestCase):
    def test_model_compiles_and_moves_and_jumps(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / ("platform_game_test.exe" if os.name == "nt" else "platform_game_test")
            compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
            self.assertIsNotNone(compiler, "a C compiler is required")
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{ENGINE / 'examples/sky_hop/main'}",
                str(ENGINE / "tests/test_platform_game.c"),
                str(ENGINE / "examples/sky_hop/main/platform_game.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "platform game model: ok")


if __name__ == "__main__":
    unittest.main()
