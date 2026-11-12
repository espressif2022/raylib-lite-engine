from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
GAME = ROOT / "examples/sky_hop/main"


class PlatformGameTests(unittest.TestCase):
    def test_model_moves_jumps_and_reports_events_once(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / ("platform_game_test.exe" if os.name == "nt"
                                            else "platform_game_test")
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", f"-I{GAME}",
                str(Path(__file__).with_name("test_platform_game.c")),
                str(GAME / "platform_game.c"), "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "platform game model: ok")


if __name__ == "__main__":
    unittest.main()
