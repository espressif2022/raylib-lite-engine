from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ENGINE = Path(__file__).resolve().parents[1]


class GameAssetsTests(unittest.TestCase):
    def test_backing_image_stream_release_and_partition_seam(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "game_assets.exe" if os.name == "nt" else "game_assets")
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                f"-I{ENGINE / 'include/raylib_lite'}",
                f"-I{ENGINE / 'src/assets'}",
                str(ENGINE / "tests/test_game_assets.c"),
                str(ENGINE / "src/assets/mosaico_game_assets.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "game assets: ok")


if __name__ == "__main__":
    unittest.main()
