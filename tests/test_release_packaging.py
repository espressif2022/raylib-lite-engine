# SPDX-License-Identifier: Apache-2.0
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from prepare_release import assemble, validate, GAMES
from check_release import check
from prepare_release_consumer import prepare


class ReleasePackagingTests(unittest.TestCase):
    def test_games_are_independent_and_inventory_covers_supplied_assets(self):
        with tempfile.TemporaryDirectory() as folder:
            release = Path(folder) / "release"
            stage = assemble(release)
            self.assertEqual({p.name for p in (stage / "examples").iterdir()}, {*GAMES, "minimal"})
            for game in GAMES:
                example = stage / "examples" / game
                self.assertTrue((example / "shared/boards/esp-mosaico/board.c").is_file())
                self.assertTrue((example / "shared/common_components/examples_common/native_module_main.c").is_file())
                self.assertNotIn("../common_components/", (example / "CMakeLists.txt").read_text())
                self.assertNotIn("override_path", (example / "main/idf_component.yml").read_text())
                assets = json.loads((example / "ASSET_PROVENANCE.json").read_text())["files"]
                self.assertTrue(assets, game)
            with contextlib.redirect_stdout(io.StringIO()):
                check(release)
            (stage / "examples/sky_hop/main/CMakeLists.txt").write_text('include("${CMAKE_CURRENT_LIST_DIR}/../../outside.cmake")')
            with self.assertRaisesRegex(ValueError, "external CMake"):
                validate(stage)

    def test_inventory_rejects_modified_staged_file(self):
        with tempfile.TemporaryDirectory() as folder:
            release = Path(folder) / "release"
            stage = assemble(release, ())
            (stage / "src/idf/clock_esp.c").write_text("changed")
            with self.assertRaisesRegex(ValueError, "hashes changed"):
                check(release)

    def test_consumer_rejects_archive_traversal(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            archive = root / "unsafe.tgz"
            with tarfile.open(archive, "w:gz") as stream:
                member = tarfile.TarInfo("../../escaped")
                member.size = 1
                stream.addfile(member, io.BytesIO(b"x"))
            with self.assertRaisesRegex(ValueError, "Unsafe archive"):
                prepare(archive, "minimal", root / "consumer")
            self.assertFalse((root / "escaped").exists())

    def test_existing_output_and_source_are_preserved(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "existing"
            output.mkdir()
            (output / "keep").write_text("original")
            with self.assertRaisesRegex(ValueError, "already exists"):
                assemble(output)
            self.assertEqual((output / "keep").read_text(), "original")
        with self.assertRaisesRegex(ValueError, "source checkout"):
            assemble(ROOT)


if __name__ == "__main__":
    unittest.main()
