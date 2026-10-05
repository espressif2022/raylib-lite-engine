# Contributing to Raylib Lite Engine

[简体中文](CONTRIBUTING.CN.md) · [Documentation](docs/README.EN.md)

Start with the [quickstart](docs/quickstart.EN.md), then choose the affected build path from the [matrix](docs/build-matrix.EN.md). Keep gameplay models and shared views portable; concrete native-example board services belong under `examples/boards/<board>/`, and game sources depend only on the shared example-board contract. Product-specific integration remains external. Use existing public APIs from the [capability catalog](components/README.md) before adding a game-local helper.

For a change, describe the trigger, resulting behavior, and target path. Add an independent correctness check when changing pixels, asset formats, or protocol output. Use `python3 tools/check_markdown_links.py`, `git diff --check`, and the relevant tests; [AGENTS.md](AGENTS.md) maps change types to checks. Record device measurements separately from Host timings. Do not include generated `build/`, `managed_components/`, `sdkconfig`, or local `docs/debug/` files.

New source files should carry an SPDX identifier and follow the existing C/Python formatting in their component. Do not copy another engine's implementation into this repository. Keep original algorithm and data-layout decisions reviewable in the PR or commit description.
