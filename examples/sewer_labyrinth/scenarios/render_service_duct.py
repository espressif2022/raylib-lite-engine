"""Capture the real flooded/exposed duct and an entire survey outing."""
import json
from pathlib import Path
import sys
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT.parents[1] / "host"))
from run_game import GenericHostRuntime, prepare_project_assets  # noqa: E402


def main():
    prepare_project_assets(PROJECT)
    checkpoints = json.loads((PROJECT / "scenarios/checkpoints.json").read_text())
    summaries = {}
    for name, shots in [("submerged-duct.json", {"mouth": "duct-flooded"}),
                        ("service-duct.json", {"mouth": "duct-exposed", "inside": "duct-crouch",
                                               "log": "duct-log", "end": "duct-complete"})]:
        events = json.loads((PROJECT / "scenarios" / name).read_text())["events"]
        by_frame = {}
        for event in events:
            by_frame.setdefault(event["frame"], []).append(event)
        captures = {checkpoints[name][key]: filename for key, filename in shots.items()}
        with tempfile.TemporaryDirectory() as directory:
            runtime = GenericHostRuntime(PROJECT, Path(directory))
            try:
                for frame in range(checkpoints[name]["end"]):
                    for event in by_frame.get(frame, []):
                        runtime.action(event["code"], event["pressed"])
                    runtime.api.mosaico_host_game_update_v1(runtime.context)
                    runtime.frames += 1
                    if frame + 1 in captures:
                        image = runtime.frame()
                        state = runtime.metadata()
                        if state["faces_dropped"]:
                            raise RuntimeError("Geometry buffer overflow")
                        (PROJECT / "docs" / (captures[frame + 1] + ".png")).write_bytes(image)
                        summaries[captures[frame + 1]] = {key: state[key] for key in
                            ("phase", "cell_x", "cell_z", "logs", "duct_open", "duct_used", "faces")}
                if name == "service-duct.json" and runtime.metadata()["phase"] != "won":
                    raise RuntimeError("Survey route did not complete: " + str(runtime.metadata()))
            finally:
                runtime.close()
    print(json.dumps(summaries, indent=2))


if __name__ == "__main__":
    main()
