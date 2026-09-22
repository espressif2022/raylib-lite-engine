"""The engine Host renders only the modules selected by public manifests."""
from pathlib import Path
import unittest

ENGINE = Path(__file__).resolve().parents[1]


class HostBoundariesTests(unittest.TestCase):
    def test_host_runtime_has_no_project_specific_renderer(self) -> None:
        source = (ENGINE / "host/run_game.py").read_text(encoding="utf-8")
        for project_name in ("tower_defense", "sky_hop", "raylib_shooter"):
            self.assertNotIn(project_name, source)
        self.assertFalse((ENGINE / "host/tower_host_renderer.c").exists())


if __name__ == "__main__":
    unittest.main()
