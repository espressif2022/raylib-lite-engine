from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]
PROJECT = ENGINE / "examples/last_zone_extraction"


class LastZoneModelTests(unittest.TestCase):
    def test_wall_rays_use_camera_plane_and_edge_budget(self) -> None:
        source = (PROJECT / "main/last_zone_view.c").read_text(encoding="utf-8")
        self.assertIn("#define LAST_ZONE_EDGE_RAY_BUDGET 64", source)
        self.assertIn("s_camera_dir_x + s_camera_plane_x * camera_x", source)
        self.assertIn("s_view_stats.rays_cast", source)
        self.assertNotIn("cosf(ray - game->angle)", source)
        self.assertNotIn("LAST_ZONE_REFINEMENT_BUDGET", source)

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
                str(ENGINE / "tests/test_last_zone_model.c"),
                str(PROJECT / "main/last_zone_game.c"),
                "-lm", "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "last zone model: ok")


if __name__ == "__main__":
    unittest.main()
