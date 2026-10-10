#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check local Markdown link targets in maintained repository documents."""
from __future__ import annotations

from pathlib import Path
import re
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"(?<!!)\[[^\]]+\]\((<[^>]+>|[^\s)]+)(?:\s+\"[^\"]*\")?\)")


def main() -> int:
    failures: list[str] = []
    paths = [ROOT / name for name in ("README.md", "README.CN.md", "README.EN.md", "API.md",
                                        "CONTRIBUTING.md", "CONTRIBUTING.CN.md",
                                        "CONTRIBUTING.EN.md", "THIRD_PARTY_NOTICES.md")]
    paths += sorted((ROOT / "docs").rglob("*.md"))
    paths += sorted((ROOT / "release").rglob("*.md"))
    paths += sorted((ROOT / "components").rglob("README.md"))
    paths += sorted((ROOT / "examples").glob("*/README.md"))
    paths += sorted((ROOT / "host").rglob("README.md"))
    paths += sorted((ROOT / "tests").rglob("README.md"))
    for path in paths:
        if "debug" in path.relative_to(ROOT).parts or not path.is_file():
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for match in LINK.finditer(line):
                raw = match.group(1).strip("<>")
                if raw.startswith("#") or urlsplit(raw).scheme:
                    continue
                target = unquote(raw.split("#", 1)[0])
                if not target or target.startswith("/"):
                    continue
                if not (path.parent / target).exists():
                    failures.append(f"{path.relative_to(ROOT)}:{number}: {raw}")
    if failures:
        print("Missing local Markdown targets:\n" + "\n".join(failures))
        return 1
    print(f"Checked local Markdown links in {len(paths)} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
