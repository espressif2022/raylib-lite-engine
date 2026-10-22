from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]


class GameplayImprovementTests(unittest.TestCase):
    def test_campaigns_projectiles_gestures_and_purchases(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "gameplay_improvements.exe" if os.name == "nt" else "gameplay_improvements"
            )
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                *shlex.split(os.environ.get("CFLAGS", "")),
                f"-I{ENGINE / 'examples/tower_defense/main'}",
                f"-I{ENGINE / 'examples/raylib_shooter/main'}",
                str(ENGINE / "tests/test_gameplay_improvements.c"),
                str(ENGINE / "examples/tower_defense/main/tower_game.c"),
                str(ENGINE / "examples/raylib_shooter/main/shooter_game.c"),
                "-lm", "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "gameplay improvements: ok")


if __name__ == "__main__":
    unittest.main()
