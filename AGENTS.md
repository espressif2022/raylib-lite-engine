# Repository checks for agents and contributors

Choose checks from the files changed. Report which checks ran and which device paths were not exercised. Do not treat Host timing as device FPS or publish a performance claim without a comparable board capture.

| Change | Required local checks |
| --- | --- |
| Maintained Markdown or links | `python3 tools/check_markdown_links.py`; `git diff --check` |
| Portable C, public headers, Host runner, or game CLI | Relevant focused tests, then `python3 -m unittest discover -s tests -v` before a commit or PR |
| Raster implementation | Independent pixel-oracle tests (`tests.test_columns`, `tests.test_primitives`, and the affected benchmark suite); compare quality before performance; report actual board timing separately |
| Asset conversion | Run the affected packer/game tests and verify generated names against the manifest |
| Audio cue or haptic mapping | Run the related model/audio tests; check repeated event consumption and cleanup; listen or feel on the target device when hardware is available |
| Board adapter or display path | Build the affected target; capture device startup, input, present, and shutdown results when hardware is available |

Keep game models and shared views independent of ESP-IDF/BSP headers. Concrete native-example boards live under `examples/boards/<board>/`; game sources depend only on the selected example-board contract/component and must not include a concrete board API. Device-only benchmark adapters are explicit exceptions in the boundary test. Use separate build directories for Host, direct native, and lobby ELF outputs. Flashing, installing, or publishing require their own authorization; a build instruction does not authorize them.
