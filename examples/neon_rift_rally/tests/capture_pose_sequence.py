#!/usr/bin/env python3
"""Capture the five motorcycle poses from the real Host module."""
from pathlib import Path
import sys
import tempfile

from PIL import Image, ImageDraw


PROJECT = Path(__file__).resolve().parents[1]
ENGINE = PROJECT.parents[1]
sys.path.insert(0, str(ENGINE / "host"))
from run_game import GenericHostRuntime  # noqa: E402


def start(runtime: GenericHostRuntime) -> None:
    runtime.control(3)
    runtime.action(2, True)
    for _ in range(92):
        runtime.api.raylib_lite_host_game_update_v1(runtime.context)


def capture(runtime: GenericHostRuntime, target: int) -> Image.Image:
    start(runtime)
    if target:
        runtime.action(0 if target < 0 else 1, True)
        for _ in range(30):
            runtime.api.raylib_lite_host_game_update_v1(runtime.context)
            pose = int(runtime.metadata()["view_pose"])
            if pose == target:
                break
        assert pose == target, (target, pose)
    pixels, width, height = runtime.snapshot_rgb565()
    return Image.frombytes("RGB", (width, height), pixels, "raw", "BGR;16")


def main() -> None:
    output = PROJECT / "build-host/five-pose-contact-sheet.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="rally_pose_capture_") as directory:
        runtime = GenericHostRuntime(PROJECT, Path(directory))
        try:
            frames = [capture(runtime, pose) for pose in (-2, -1, 0, 1, 2)]
        finally:
            runtime.close()
    sheet = Image.new("RGB", (480 * 5, 510), "#05081d")
    draw = ImageDraw.Draw(sheet)
    labels = ("HEAVY LEFT", "LIGHT LEFT", "STRAIGHT", "LIGHT RIGHT", "HEAVY RIGHT")
    for index, (frame, label) in enumerate(zip(frames, labels)):
        sheet.paste(frame, (index * 480, 30))
        draw.text((index * 480 + 10, 8), label, fill="#dcecff")
    sheet.save(output)
    print(output)


if __name__ == "__main__":
    main()
