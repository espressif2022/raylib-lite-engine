# SPDX-License-Identifier: Apache-2.0
"""Every Raylib-name mapping must be registered in the compatibility status table."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
FACADE = ROOT / "compat/raylib/include/raylib_lite_raylib.h"
STATUS = ROOT / "docs/raylib-compat.CN.md"
STATES = ("等价", "契约", "差异")


def mapped_names():
    return re.findall(r"^#define\s+([A-Z]\w*)\s+raylib_lite_raylib_\w+",
                      FACADE.read_text(encoding="utf-8"), re.MULTILINE)


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


if __name__ == "__main__":
    unittest.main()
