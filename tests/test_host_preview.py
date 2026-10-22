"""Preview transport tests run the real HTTP handlers without binding a port."""
from __future__ import annotations

import io
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import unittest
from unittest.mock import patch

from host import run_game


class Runtime:
    def __init__(self, locked: bool = True):
        self.lock = threading.Lock()
        self.locked = locked
        self.calls = []
        self.closed = False

    def check(self):
        if self.locked:
            assert self.lock.locked(), "runtime used outside the preview transaction"

    def metadata(self):
        self.check()
        return {"tick_hz": 30, "width": 320, "height": 240, "max_pointers": 2}

    def action(self, *args):
        self.check(); self.calls.append(("action", *args))

    def pointer(self, *args):
        self.check(); self.calls.append(("pointer", *args))

    def imu(self, *args):
        self.check(); self.calls.append(("imu", *args))

    def control(self, *args):
        self.check(); self.calls.append(("control", *args))

    def step(self, *args):
        self.check(); self.calls.append(("step", *args))

    def frame(self):
        return b"png fixture"

    def close(self):
        self.closed = True


class PreviewTests(unittest.TestCase):
    def setUp(self):
        self.runtime = Runtime()
        handlers = []
        class Server:
            def __init__(self, address, handler):
                handlers.append(handler)
            def serve_forever(self):
                pass
        with patch.object(run_game, "ThreadingHTTPServer", Server), \
                patch.object(run_game.threading.Thread, "start"), \
                patch("builtins.print"):
            run_game.serve_interactive_preview("localhost", 0, self.runtime)
        self.handler = handlers[0]

    def request(self, path, value=None, raw=None, content_length=None):
        handler = object.__new__(self.handler)
        handler.path = path
        payload = raw if raw is not None else json.dumps(value).encode()
        handler.headers = {"Content-Length": str(len(payload) if content_length is None else content_length)}
        handler.rfile, handler.wfile = io.BytesIO(payload), io.BytesIO()
        status = []
        handler.send_response = status.append
        handler.send_error = lambda code: status.append(code)
        handler.send_header = lambda *args: None
        handler.end_headers = lambda: None
        if value is None and raw is None:
            handler.do_GET()
        else:
            handler.do_POST()
        return status[0], handler.wfile.getvalue()

    def test_invalid_requests_do_not_partially_apply_or_record(self):
        self.assertEqual(self.request("/api/v1/control", {"command": "record"})[0], 200)
        for value in ([], {"left": "false"}, {"left": True, "pointers": [{"x": 1}]},
                      {"left": True, "pointers": [{"x": 1, "y": 2, "track": 3}] * 2},
                      {"left": True, "imu": {"x": float("nan")}},
                      {"left": True, "actions": [{"code": -1}]},
                      {"left": True, "actions": [{"code": 0, "pressed": 1}]},
                      {"left": True, "pointers": [{"x": 2**40, "y": 0}]},
                      {"left": True, "pointers": [{"x": 0, "y": 0}] * 3}):
            with self.subTest(value=value):
                self.assertEqual(self.request("/api/v1/input", value)[0], 400)
                self.assertEqual(self.runtime.calls, [])
                recording = json.loads(self.request("/api/v1/recording")[1])
                self.assertEqual(recording["events"], [])
        for length in ("oops", -1, 65537):
            self.assertEqual(self.request("/api/v1/input", {}, content_length=length)[0], 400)
        self.assertEqual(self.request("/api/v1/control", {"command": []})[0], 400)
        self.assertEqual(self.request("/api/v1/control", {"command": "speed:nan"})[0], 400)

    def test_quick_edges_and_all_actions_survive_recording_replay(self):
        self.request("/api/v1/control", {"command": "step"})
        self.runtime.calls.clear()
        self.request("/api/v1/control", {"command": "record"})
        self.request("/api/v1/input", {name: True for name in run_game.HELD_ACTIONS})
        self.request("/api/v1/input", {})
        self.request("/api/v1/input", {"actions": [{"code": 4, "pressed": True},
                                                    {"code": 4, "pressed": False}]})
        original = list(self.runtime.calls)
        self.assertEqual([call for call in original if call[1] == 0],
                         [("action", 0, True), ("action", 0, False)])
        recording = json.loads(self.request("/api/v1/recording")[1])
        self.assertTrue(all(event["frame"] == 0 for event in recording["events"]))
        replay_runtime = Runtime(locked=False)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "replay.json"
            # Exercise both exported numeric and older human-readable recordings.
            names = {code: name for name, code in run_game.ACTION_CODES.items()}
            for event in recording["events"]:
                event["code"] = names[event["code"]]
            path.write_text(json.dumps(recording))
            replay = run_game.load_replay(path)
            with patch.object(run_game, "GenericHostRuntime", return_value=replay_runtime):
                run_game.run_generic(Path(directory), Path(directory), 1,
                                     Path(directory) / "frame.png", replay)
        self.assertEqual(replay_runtime.calls[:-1], original)
        self.assertTrue(replay_runtime.closed)

    def test_pointer_release_step_pause_and_initial_held_state(self):
        self.request("/api/v1/input", {"left": True, "pointers": [{"track": 9, "x": 12.7, "y": 20}]})
        self.request("/api/v1/control", {"command": "pause"})
        self.request("/api/v1/control", {"command": "record"})
        self.request("/api/v1/control", {"command": "step"})
        self.request("/api/v1/input", {})
        recording = json.loads(self.request("/api/v1/recording")[1])
        self.assertEqual(recording["events"][:3], [
            {"frame": 0, "type": "pause"},
            {"frame": 0, "type": "action", "code": 0, "pressed": True},
            {"frame": 0, "type": "pointer", "track": 9, "x": 12, "y": 20, "pressed": True},
        ])
        self.assertIn(("step", True, False, False), self.runtime.calls)
        self.assertEqual(recording["events"][-1],
                         {"frame": 1, "type": "pointer", "track": 9, "x": 12, "y": 20, "pressed": False})
        self.assertIn(("pointer", 9, 12, 20, False), self.runtime.calls)

    def test_game_specific_numeric_action_codes_remain_supported(self):
        self.assertEqual(self.request("/api/v1/input", {"actions": [{"code": 12}]})[0], 200)
        self.assertEqual(self.runtime.calls, [("action", 12, True)])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "replay.json"
            path.write_text(json.dumps([{"frame": 0, "type": "action", "code": 12}]))
            self.assertEqual(run_game.load_replay(path)[0]["code"], 12)

    def test_replay_failure_releases_module(self):
        runtime = Runtime(locked=False)
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(run_game, "GenericHostRuntime", return_value=runtime):
            with self.assertRaises(ValueError):
                run_game.run_generic(Path(directory), Path(directory), 1,
                    Path(directory) / "frame.png", [{"frame": 0, "type": "action", "code": "bad"}])
        self.assertTrue(runtime.closed)

    @unittest.skipUnless(shutil.which("node"), "Node.js is required to execute preview browser handlers")
    def test_browser_queue_blur_coordinates_and_screenshot(self):
        html = self.request("/")[1].decode()
        script = html.split("<script>", 1)[1].split("</script>", 1)[0]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "preview.js"
            path.write_text(script)
            subprocess.run([shutil.which("node"), str(Path(__file__).with_suffix(".js")), str(path)],
                           check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
