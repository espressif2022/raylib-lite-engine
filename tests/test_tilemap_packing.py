# SPDX-License-Identifier: Apache-2.0
"""MTM packing validates Tiled GIDs independently of the built-in terrain art."""
import copy
import json
from pathlib import Path
import struct
import tempfile
import unittest

from tools import pack_game_assets as packer

ROOT = Path(__file__).resolve().parents[1]


class TilemapPackingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / "level.tmj"
        self.destination = self.root / "level.map"
        self.map = {
            "orientation": "orthogonal", "infinite": False,
            "width": 3, "height": 1, "tilewidth": 16, "tileheight": 16,
            "tileset": "custom.atlas",
            "tilesets": [{"firstgid": 100, "tilecount": 8,
                          "tilewidth": 16, "tileheight": 16}],
            "layers": [{"type": "tilelayer", "name": "terrain", "data": [0, 100, 107]}],
        }

    def pack(self, data=None, atlases=None):
        self.source.write_text(json.dumps(self.map if data is None else data))
        report = packer.write_tilemap(self.source, self.destination, atlases)
        return report, self.destination.read_bytes()

    def test_first_gid_becomes_one_based_runtime_id_and_all_tiles_are_supported(self):
        report, blob = self.pack()
        self.assertEqual(struct.unpack_from("<3H", blob, 32), (0, 1, 8))
        self.assertEqual(report["file"], "level.map")
        self.assertEqual(struct.unpack_from("<I", blob, 28)[0], packer.asset_id("custom.atlas"))

    def test_external_json_tileset(self):
        (self.root / "tiles.tsj").write_text(json.dumps({"tilecount": 8, "tilewidth": 16, "tileheight": 16}))
        self.map["tilesets"] = [{"firstgid": 100, "source": "tiles.tsj"}]
        _, blob = self.pack()
        self.assertEqual(struct.unpack_from("<3H", blob, 32), (0, 1, 8))

    def test_unknown_flip_bits_and_noninteger_ids_fail_before_writing(self):
        for invalid in (1, 99, 108, -1, 2**32, True, 100.0, "100",
                        100 | 0x80000000, 100 | 0x40000000,
                        100 | 0x20000000, 100 | 0x10000000):
            with self.subTest(invalid=invalid):
                self.map["layers"][0]["data"] = [0, 100, invalid]
                with self.assertRaisesRegex(ValueError, "tile|Tiled"):
                    self.pack()
                self.assertFalse(self.destination.exists())

    def test_unsupported_tilesets_and_atlas_layout_fail_explicitly(self):
        for replacement in ([], [{"firstgid": 1, "tilecount": 0}],
                            [{"firstgid": 1, "tilecount": 65536}],
                            [{"firstgid": 100, "tilecount": 8, "tilewidth": 32}],
                            [{"firstgid": 1, "tilecount": 4}, {"firstgid": 5, "tilecount": 4}]):
            with self.subTest(tilesets=replacement):
                data = copy.deepcopy(self.map)
                data["tilesets"] = replacement
                with self.assertRaises(ValueError):
                    self.pack(data)
        with self.assertRaisesRegex(ValueError, "absent from manifest"):
            self.pack(atlases={})
        with self.assertRaisesRegex(ValueError, "horizontal strip"):
            self.pack(atlases={"custom.atlas": {"width": 64, "height": 32}})
        self.pack(atlases={"custom.atlas": {"width": 128, "height": 16}})

    def test_builtin_terrain_binary_is_unchanged(self):
        import hashlib
        packer.write_tilemap(ROOT / "examples/tower_defense/assets_src/level01.tmj", self.destination)
        self.assertEqual(hashlib.sha256(self.destination.read_bytes()).hexdigest(),
                         "aac7b4e76ad7307a84ea5d08c7a3101f18f18bc9df5c5b9660c3078bf1c0ceaf")

    def test_compile_manifest_names_and_atlas_reference(self):
        self.map.pop("tilesets")
        self.map["tileset"] = "terrain.atlas"
        self.map["layers"][0]["data"] = [0, 1, 3]
        self.map["tilewidth"] = self.map["tileheight"] = 32
        self.source.write_text(json.dumps(self.map))
        manifest = self.root / "game_assets.json"
        manifest.write_text(json.dumps({
            "schema": "mosaico-game-assets/v1",
            "atlases": [{"generator": "terrain", "output": "terrain.atlas"}],
            "maps": [{"source": "level.tmj", "output": "level.map"}],
        }))
        output = self.root / "generated"
        packer.compile_assets(self.root, output, manifest)
        self.assertTrue((output / "terrain.atlas").is_file())
        self.assertTrue((output / "level.map").is_file())
        self.assertEqual(struct.unpack_from("<I", (output / "level.map").read_bytes(), 28)[0],
                         packer.asset_id("terrain.atlas"))


if __name__ == "__main__":
    unittest.main()
