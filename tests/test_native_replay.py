# SPDX-License-Identifier: Apache-2.0
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from tools import native_replay

ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / "examples/common_components/examples_common"


def trace():
    return {"duration_ticks": 3, "events": [
        {"tick": 2, "type": "action", "code": 4, "pressed": True},
        {"frame": 0, "type": 1, "code": 0, "pressed": True},
        {"frame": 0, "type": "ACTION", "code": 0, "pressed": False},
        {"tick": 0, "type": "pointer", "track_id": 2, "x": 12, "y": 8, "pressed": True},
        {"tick": 0, "type": "pointer", "track_id": 2, "x": 12, "y": 8, "pressed": False},
        {"tick": 0, "type": "imu", "x": .25, "y": -.5, "z": 1},
        {"tick": 1, "type": "control", "code": "pause"},
        {"tick": 1, "type": 3, "code": 2},
        {"tick": 2, "type": "action", "code": 4, "pressed": False},
    ]}


class NativeReplayTests(unittest.TestCase):
    def test_invalid_inputs(self):
        invalid = [None, {}, {"events": []}, {"duration_ticks": 0, "events": []},
                   {"duration_ticks": True, "events": []}, {"duration_ticks": 2**32, "events": []}]
        for field, value in [("tick", 3), ("tick", -1), ("type", "tap"), ("type", True),
                             ("code", -1), ("code", "restart"), ("pressed", 1), ("tick", 1.5)]:
            data = trace()
            data["events"][0][field] = value
            invalid.append(data)
        data = trace(); data["events"][5]["x"] = float("nan"); invalid.append(data)
        data = trace(); data["events"][5]["y"] = float("inf"); invalid.append(data)
        data = trace(); data["events"][5]["z"] = 1e40; invalid.append(data)
        data = trace(); data["events"][3]["x"] = 1.5; invalid.append(data)
        data = trace(); data["events"][0]["frame"] = 2; invalid.append(data)
        data = trace(); data["events"][6]["code"] = 4; invalid.append(data)
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(ValueError):
                native_replay.normalize(value)
        with self.assertRaisesRegex(ValueError, "disagrees"):
            native_replay.normalize(trace(), 4)

    def test_hashes_are_reproducible_and_identify_source_and_event_semantics(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "trace.json", root / "trace.c"
            data = trace()
            source.write_text(json.dumps(data))
            first = native_replay.generate(source, output)
            content = output.read_bytes()
            self.assertEqual(first["source_sha256"], hashlib.sha256(source.read_bytes()).hexdigest())
            self.assertEqual(native_replay.generate(source, output), first)
            self.assertEqual(content, output.read_bytes())
            source.write_text(json.dumps(data, indent=2))
            second = native_replay.generate(source, output)
            self.assertEqual(first["events_sha256"], second["events_sha256"])
            self.assertNotEqual(first["source_sha256"], second["source_sha256"])
            data["events"][1]["pressed"] = False
            source.write_text(json.dumps(data))
            self.assertNotEqual(first["events_sha256"], native_replay.generate(source, output)["events_sha256"])

    def test_empty_trace_and_tick_override(self):
        normalized = native_replay.normalize({"events": []}, 10)
        self.assertEqual(normalized, {"duration_ticks": 10, "events": []})
        data = trace()
        rows = native_replay.normalize(data)["events"]
        self.assertEqual([r["tick"] for r in rows], [0, 0, 0, 0, 0, 1, 1, 2, 2])
        self.assertEqual([r["pressed"] for r in rows[:2]], [True, False])

    def test_cmake_replay_is_opt_in_and_validates_generated_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            generated = root / "replay.c"
            generated.write_text("/* generated fixture */")
            script = root / "check.cmake"
            script.write_text(f'''set(CMAKE_SOURCE_DIR "{root}")
function(idf_component_register)
    set(COMPONENT_LIB fixture PARENT_SCOPE)
endfunction()
function(target_sources target scope source)
    set_property(GLOBAL PROPERTY replay_source "${{source}}")
endfunction()
function(target_compile_definitions target scope)
    set_property(GLOBAL PROPERTY replay_defines "${{ARGN}}")
endfunction()
include("{COMMON / 'CMakeLists.txt'}")
get_property(source GLOBAL PROPERTY replay_source)
get_property(defines GLOBAL PROPERTY replay_defines)
if(RAYLIB_LITE_EXAMPLE_REPLAY)
    if(NOT source STREQUAL "{generated}" OR NOT defines STREQUAL "RAYLIB_LITE_NATIVE_REPLAY_ENABLED=1")
        message(FATAL_ERROR "Replay source/define not registered")
    endif()
elseif(source OR defines)
    message(FATAL_ERROR "Production defaults unexpectedly enable replay")
endif()
''')
            for options in ([], ["-DRAYLIB_LITE_EXAMPLE_REPLAY=replay.c"]):
                subprocess.run(["cmake", *options, "-P", str(script)], check=True)
            result = subprocess.run(["cmake", "-DRAYLIB_LITE_EXAMPLE_REPLAY=missing.c",
                                     "-P", str(script)], text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("must name generated replay C", result.stderr)

    def test_native_launcher_edges_finish_and_default_production_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, generated = root / "trace.json", root / "trace.c"
            source.write_text(json.dumps(trace()))
            native_replay.generate(source, generated)
            (root / "esp_err.h").write_text('''#pragma once
                typedef int esp_err_t;
                enum { ESP_OK, ESP_FAIL, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE,
                       ESP_ERR_NO_MEM, ESP_ERR_TIMEOUT, ESP_ERR_NOT_SUPPORTED };
                static inline const char *esp_err_to_name(esp_err_t error) { (void)error; return "fake"; }
            ''')
            (root / "esp_heap_caps.h").write_text('''#pragma once
                #include <stdlib.h>
                enum { MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2 };
                static inline void *heap_caps_calloc(size_t n, size_t size, unsigned caps)
                { (void)caps; return calloc(n, size); }
                #define heap_caps_free free
            ''')
            (root / "esp_log.h").write_text('''#pragma once
                #include <stdio.h>
                #define ESP_LOGI(tag, ...) do { (void)(tag); printf(__VA_ARGS__); putchar('\\n'); } while (0)
                #define ESP_LOGE ESP_LOGI
                #define ESP_LOGW ESP_LOGI
            ''')
            sources = [ROOT / path for path in [
                "tests/test_native_replay.c", "host/host_video_backend.c",
                "src/runtime/raylib_lite_raylib_port.c", "host/host_asset_runtime.c", "src/assets/raylib_lite_assets.c",
                "src/renderer/raylib_lite_renderer.c", "src/renderer/raylib_lite_renderer_raylib.c",
                "src/renderer/raylib_lite_rgb565.c", "src/renderer/raylib_lite_raylib_impl.c"]]
            includes = [root, COMMON / "include", ROOT / "examples/common_components/examples_audio"]
            includes += [ROOT / path for path in ["src/assets", "host/include", "host", "include/raylib_lite", "compat/raylib/include"]]
            for enabled in (False, True):
                with self.subTest(enabled=enabled):
                    executable = root / f"launcher-{enabled}"
                    command = [os.environ.get("CC", "cc"), "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                               *shlex.split(os.environ.get("CFLAGS", ""))]
                    if enabled:
                        command += ["-DRAYLIB_LITE_NATIVE_REPLAY_ENABLED=1"]
                    for include in includes:
                        command += ["-I", str(include)]
                    command += [str(COMMON / "native_module_main.c"), str(generated), *map(str, sources),
                                "-lm", "-o", str(executable)]
                    subprocess.run(command, check=True)
                    result = subprocess.run([str(executable)], check=True, text=True, capture_output=True)
                    self.assertIn("native replay launcher: ok", result.stdout)
                    self.assertEqual("replay shutdown" in result.stdout, enabled)
                    if enabled:
                        self.assertIn('replay state_json={"ticks":3}', result.stdout)
                        self.assertIn("replay complete ticks=3", result.stdout)


if __name__ == "__main__":
    unittest.main()
