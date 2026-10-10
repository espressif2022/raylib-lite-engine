from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[3]
PROJECT = ENGINE / "examples/last_zone_extraction"


class LastZoneModelTests(unittest.TestCase):

    def test_armor_and_explosive_barrel(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "last-zone-test.exe" if os.name == "nt" else "last-zone-test"
            )
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{PROJECT / 'main'}",
                str(Path(__file__).with_name("test_last_zone_model.c")),
                str(PROJECT / "main/last_zone_game.c"),
                "-lm", "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "last zone model: ok")


if __name__ == "__main__":
    unittest.main()
