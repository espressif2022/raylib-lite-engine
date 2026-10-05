from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RendererCoreTests(unittest.TestCase):
    def test_core_is_raylib_free_and_releases_asset_leases(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "renderer_core.exe" if os.name == "nt" else "renderer_core")
            command = [
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                f"-I{ROOT / 'components/mosaico_game_2d/include'}",
                f"-I{ROOT / 'components/mosaico_game_2d'}",
                f"-I{ROOT / 'components/mosaico_game_assets/include'}",
                f"-I{ROOT / 'components/raylib_lite_platform/include'}",
                str(ROOT / "tests/test_renderer_core.c"),
                str(ROOT / "components/mosaico_game_2d/mosaico_game_2d.c"),
                str(ROOT / "components/mosaico_game_2d/mosaico_rgb565.c"),
                "-lm", "-o", str(executable),
            ]
            subprocess.run(command, check=True)
            result = subprocess.run(
                [str(executable)], check=True, text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "renderer core: ok")


if __name__ == "__main__":
    unittest.main()
