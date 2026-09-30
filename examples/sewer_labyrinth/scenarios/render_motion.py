"""Capture actual Host frames for action review, using the full-route replay."""
import io
import json
from pathlib import Path
import sys
import tempfile

from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
ENGINE = PROJECT.parents[1]
sys.path.insert(0, str(ENGINE / "host"))
from run_game import GenericHostRuntime, prepare_project_assets  # noqa: E402


def look(runtime, direction):
    start, end = (460, 264) if direction < 0 else (264, 460)
    runtime.pointer(99, start, 275, True)
    runtime.pointer(99, end, 275, True)
    runtime.pointer(99, end, 275, False)


def main():
    prepare_project_assets(PROJECT)
    events = json.loads((PROJECT / "scenarios/full-route.json").read_text())["events"]
    by_frame = {}
    for event in events:
        by_frame.setdefault(event["frame"], []).append(event)
    checkpoints = json.loads((PROJECT / "scenarios/checkpoints.json").read_text())["full-route.json"]
    clips, current, max_faces = {}, 0, 0
    names = {1: "pickup", 3: "starter", 4: "valve"}
    with tempfile.TemporaryDirectory() as directory:
        runtime = GenericHostRuntime(PROJECT, Path(directory))
        try:
            (PROJECT / "docs/dispatch.png").write_bytes(runtime.frame())
            for frame in range(checkpoints["west"] + 2):
                for event in by_frame.get(frame, []):
                    runtime.action(event["code"], event["pressed"])
                runtime.api.mosaico_host_game_update_v1(runtime.context)
                runtime.frames += 1
                if frame==2:
                    (PROJECT / "docs/entrance.png").write_bytes(runtime.frame())
                state = runtime.metadata()
                action = state["action"]
                if action in names and action not in clips:
                    clips[action] = []
                    current = action
                    if current in (1, 3):
                        look(runtime, -1)
                if current and action != current:
                    if current in (1, 3):
                        look(runtime, 1)
                    current = 0
                if current and frame % 2 == 0:
                    clips[current].append(Image.open(io.BytesIO(runtime.frame())).convert("RGB"))
                    rendered = runtime.metadata()
                    max_faces = max(max_faces, rendered["faces"])
                    if rendered["faces_dropped"]:
                        raise RuntimeError("Geometry budget overflow in action capture")
                if frame + 1 == checkpoints["drained"]:
                    (PROJECT / "docs/pump-drained.png").write_bytes(runtime.frame())
            if set(clips) != set(names):
                raise RuntimeError("Replay did not trigger all three showcase actions")
            for action, images in clips.items():
                images[len(images)//2].save(PROJECT / f"docs/action-{names[action]}.png")
                palette_images=[image.quantize(colors=256,dither=0) for image in images]
                palette_images[0].save(PROJECT / f"docs/action-{names[action]}.gif", save_all=True,
                                      append_images=palette_images[1:], duration=67, loop=0)
            print(json.dumps({"clips": {names[k]: len(v) for k, v in clips.items()},
                              "max_action_faces": max_faces,
                              "west_open": runtime.metadata()["west"]}))
            if not runtime.metadata()["west"]:
                raise RuntimeError("Replay failed to reach the west valve")
        finally:
            runtime.close()


if __name__ == "__main__":
    main()
