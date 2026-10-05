from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ENGINE = Path(__file__).resolve().parents[1]


class GameSaveTests(unittest.TestCase):
    def test_core_storage_contract_version_crc_and_debounce(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "game_save.exe" if os.name == "nt" else "game_save")
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                f"-I{ENGINE / 'include/raylib_lite'}",
                str(ENGINE / "tests/test_game_save.c"),
                str(ENGINE / "src/save/mosaico_game_save.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "game save: ok")


if __name__ == "__main__":
    unittest.main()
