# SPDX-License-Identifier: Apache-2.0
"""Every Raylib-name mapping must be registered in the compatibility status table."""
from pathlib import Path
import re
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
FACADES = (ROOT / "compat/raylib/include/raylib_lite_raylib.h",
           ROOT / "compat/raylib/include/raylib_lite_raylib_audio.h")
RCORE = ROOT / "include/raylib_lite/raylib_lite_rcore.h"
STATUS = ROOT / "docs/raylib-compat.CN.md"
STATES = ("等价", "契约", "差异")


def mapped_names():
    names = []
    for facade in FACADES:
        names += re.findall(r"^#define\s+([A-Z][a-z]\w*)\s+raylib_lite_\w+",
                            facade.read_text(encoding="utf-8"), re.MULTILINE)
    return names


def registered_names():
    names = {}
    for line in STATUS.read_text(encoding="utf-8").splitlines():
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) < 2 or cells[1] not in STATES:
            continue
        for name in re.findall(r"`([A-Z]\w*)`", cells[0]):
            names[name] = cells[1]
    return names


class RaylibCompatPolicyTests(unittest.TestCase):
    def test_every_mapping_has_a_status(self):
        missing = sorted(set(mapped_names()) - set(registered_names()))
        self.assertEqual(missing, [], "register new mappings in docs/raylib-compat.CN.md")

    def test_status_table_has_no_stale_rows(self):
        stale = sorted(set(registered_names()) - set(mapped_names()))
        self.assertEqual(stale, [], "remove retired mappings from docs/raylib-compat.CN.md")

    def test_compat_facade_and_rcore_platform_are_exclusive(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        includes = ["-I", str(ROOT / "include/raylib_lite"),
                    "-I", str(ROOT / "compat/raylib/include"),
                    "-I", str(ROOT / "third_party/raylib/src")]
        orders = ((FACADES[0].name, RCORE.name), (RCORE.name, FACADES[0].name))
        for first, second in orders:
            with self.subTest(first=first):
                source = f'#include "{first}"\n#include "{second}"\nint main(void) {{ return 0; }}\n'
                result = subprocess.run(
                    [compiler, "-std=c11", "-fsyntax-only", *includes, "-x", "c", "-"],
                    input=source, capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("include only one", result.stderr)


if __name__ == "__main__":
    unittest.main()
