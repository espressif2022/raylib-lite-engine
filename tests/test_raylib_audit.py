# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import shutil
import re
import tempfile
import unittest

from tools.audit_raylib import ROOT, check_frozen, check_upstream, consumers, release_games, source_references


class RaylibAuditTests(unittest.TestCase):
    def test_vendor_inventory_and_frozen_names(self):
        self.assertEqual(check_upstream()["local_files"], ["src/config.h"])
        self.assertTrue(check_frozen())

    def test_unregistered_vendor_patch_and_extra_file_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            vendor = root / "third_party/raylib"
            shutil.copytree(ROOT / "third_party/raylib", vendor)
            source = vendor / "src/rtext.c"
            original = source.read_bytes()
            source.write_bytes(original + b"\n/* unexpected patch */\n")
            with self.assertRaisesRegex(ValueError, "src/rtext.c"):
                check_upstream(root)
            source.write_bytes(original)
            (vendor / "src/config.h").write_text("/* locally owned configuration */")
            check_upstream(root)
            (vendor / "src/extra.c").write_text("/* unexpected source */")
            with self.assertRaisesRegex(ValueError, "src/extra.c"):
                check_upstream(root)

    def test_new_mapping_is_rejected_even_when_documentation_could_be_updated(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copytree(ROOT / "compat/raylib", root / "compat/raylib")
            facade = root / "compat/raylib/include/raylib_lite_raylib.h"
            facade.write_text(facade.read_text() + "\n#define LoadFont raylib_lite_load_font\n")
            with self.assertRaisesRegex(ValueError, "LoadFont"):
                check_frozen(root)

    def test_consumer_locations_ignore_comments_and_strings(self):
        source = '// DrawText("comment");\n/* BeginMode2D\n ignored */\nconst char *s="DrawText";\nDrawText("ok",0,0,10,WHITE);\n'
        references = list(source_references(source))
        self.assertEqual([line for line, symbol in references if symbol == "DrawText"], [5])
        self.assertFalse(any(symbol == "BeginMode2D" for _, symbol in references))

    def test_inventory_covers_release_games_without_claiming_runtime_frequency(self):
        report = consumers()
        self.assertEqual(set(report), set(release_games()))
        self.assertIn("BeginMode2D", report["sky_hop"]["mapping_names"])
        self.assertTrue(all("sky_hop_view.c:" in site for site in
                            report["sky_hop"]["references"]["BeginMode2D"]))

    def test_maintained_games_do_not_include_engine_private_headers(self):
        private_headers = {p.name for p in (ROOT / "src").rglob("*.h")}
        for game in release_games():
            for path in (ROOT / "examples" / game / "main").rglob("*"):
                if path.suffix not in (".c", ".h"):
                    continue
                source = re.sub(r"/\*.*?\*/|//[^\n]*", "", path.read_text(), flags=re.DOTALL)
                for header in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', source, re.MULTILINE):
                    with self.subTest(game=game, file=path.name, header=header):
                        self.assertNotIn(Path(header).name, private_headers,
                                         "games must consume public Engine/compat headers")
                for line, symbol in source_references(path.read_text()):
                    with self.subTest(game=game, file=path.name, line=line):
                        self.assertFalse(symbol.startswith("raylib_lite_primitive_"),
                                         "private raster helpers are not game APIs")


if __name__ == "__main__":
    unittest.main()
