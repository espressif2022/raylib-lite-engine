"""Capture the mechanical release and both directions of the real return loop."""
import io
import json
from pathlib import Path
import sys
import tempfile

from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT.parents[1] / "host"))
from run_game import GenericHostRuntime, prepare_project_assets  # noqa: E402


def side_view(runtime, direction):
    start, end = (260, 456) if direction > 0 else (456, 260)
    runtime.pointer(97, start, 275, True)
    runtime.pointer(97, end, 275, True)
    runtime.pointer(97, end, 275, False)


def main():
    prepare_project_assets(PROJECT)
    checkpoints = json.loads((PROJECT / "scenarios/checkpoints.json").read_text())["workshop-loop.json"]
    events = json.loads((PROJECT / "scenarios/workshop-loop.json").read_text())["events"]
    by_frame = {}
    for event in events:
        by_frame.setdefault(event["frame"], []).append(event)
    captures = {checkpoints[k]: "workshop-" + k for k in ("bench", "latch", "north", "return")}
    (PROJECT / "docs").mkdir(exist_ok=True)
    clips, states, maximum = [], {}, 0
    with tempfile.TemporaryDirectory() as directory:
        runtime = GenericHostRuntime(PROJECT, Path(directory))
        try:
            for frame in range(checkpoints["end"]):
                for event in by_frame.get(frame, []):
                    runtime.action(event["code"], event["pressed"])
                if frame + 1 == checkpoints["bench"]:
                    side_view(runtime, 1)
                runtime.api.mosaico_host_game_update_v1(runtime.context)
                runtime.frames += 1
                state = runtime.metadata()
                if frame + 1 in captures or (state["action"] == 14 and frame % 2 == 0):
                    png = runtime.frame()
                    rendered = runtime.metadata()
                    assert not rendered["faces_dropped"]
                    maximum = max(maximum, rendered["faces"])
                    if frame + 1 in captures:
                        (PROJECT / "docs" / (captures[frame + 1] + ".png")).write_bytes(png)
                        states[captures[frame + 1]] = {k: state[k] for k in
                            ("cell_x", "cell_z", "workshop_open", "workshop_used", "relay")}
                    if state["action"] == 14:
                        clips.append(Image.open(io.BytesIO(png)).convert("RGB").quantize(colors=256, dither=0))
                if frame + 1 == checkpoints["latch"]:
                    side_view(runtime, -1)
            state = runtime.metadata()
            assert state["phase"] == "won" and state["workshop_used"] and state["relay"], state
            assert clips
            clips[len(clips) // 2].convert("RGB").save(PROJECT / "docs/workshop-action.png")
            clips[0].save(PROJECT / "docs/workshop-release.gif", save_all=True,
                          append_images=clips[1:], duration=67, loop=0)
            print(json.dumps({"captures": states, "max_faces": maximum, "phase": state["phase"]}, indent=2))
        finally:
            runtime.close()


if __name__ == "__main__":
    main()
