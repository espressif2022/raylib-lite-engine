"""Tower Defense skips the full-screen clear because opaque layers cover it.

tower_view_render only paints rows 389-391 with the background colour; the
HUD covers rows 0-68, terrain layer 0 rows 69-388 and the shop rows 392-479.
That is only correct while the terrain layer has no empty tile.
"""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "examples/tower_defense"


class TowerOverdrawTests(unittest.TestCase):
    def test_terrain_layer_covers_the_play_field(self):
        level = json.loads((GAME / "assets_src/level01.tmj").read_text())
        terrain = next(layer for layer in level["layers"] if layer["type"] == "tilelayer")
        self.assertEqual(level["width"] * level["tilewidth"], 480)
        self.assertEqual(level["height"] * level["tileheight"], 320)
        self.assertTrue(all(tile > 0 for tile in terrain["data"]))

    def test_view_layers_match_the_covered_rows(self):
        view = (GAME / "main/tower_view.c").read_text()
        self.assertIn("DrawRectangle(0, 0, 480, 69, C_BG)", view)
        self.assertIn("(field.y + 69) * s_view.scale", view)
        self.assertIn("DrawRectangle(0, 392, 480, 88,", view)
        self.assertIn("DrawRectangle(0, 389, 480, 3, C_GRASS)", view)
        self.assertIsNone(re.search(r"ClearBackground\(", view))


if __name__ == "__main__":
    unittest.main()
