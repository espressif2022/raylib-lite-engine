# SPDX-License-Identifier: Apache-2.0
"""Include maintained games' local suites in the repository test command."""
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load_tests(loader, standard_tests, pattern):
    suite = unittest.TestSuite([standard_tests])
    for game in sorted((ROOT / "examples").iterdir()):
        if game.name.endswith("_dev") or not (game / "game.sim.json").is_file():
            continue
        for path in sorted((game / "tests").glob("test_*.py")):
            name = f"example_tests_{game.name}_{path.stem}"
            spec = importlib.util.spec_from_file_location(name, path)
            module = importlib.util.module_from_spec(spec)
            sys.modules[name] = module
            spec.loader.exec_module(module)
            suite.addTests(loader.loadTestsFromModule(module))
    return suite
