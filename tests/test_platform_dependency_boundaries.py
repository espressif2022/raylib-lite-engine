from pathlib import Path
import csv
import json
import re
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]
ENGINE_SOURCE_ROOTS = ("src", "include/raylib_lite", "compat/raylib")
ESP_IMPLEMENTATIONS = {
    "src/idf/raylib_lite_assets_mmap.c",
    "src/runtime/raylib_lite_debug.c",
    "src/idf/raylib_lite_save_nvs.c",
    "src/renderer/raylib_lite_renderer_esp.c",
    "src/renderer/raylib_lite_raster_bench.c",
    "src/renderer/raylib_lite_wall_bench.c",
    "src/renderer/raster_log.c",
    "src/renderer/raster_esp_config.h",
    "src/idf/clock_esp.c",
    "src/idf/raylib_lite_rcore_posix.c",
}
ESP_BENCHMARK_SOURCES = {
    "benchmark_main.c", "core_bench.c", "stack_bench.c", "render_preview.c",
    "render_display_esp.c",
}


class PlatformDependencyBoundaryTests(unittest.TestCase):
    def test_runtime_frame_backpressure_is_not_terminal(self):
        source = (ENGINE / "src/runtime/raylib_lite_game_app.c").read_text()
        for legacy in ("MosaicoGameInit", "MosaicoGameShutdown",
                       "MosaicoGamePollDeviceEvent", "MosaicoGamePostDeviceEvent"):
            self.assertNotIn(legacy, source)

        source = (ENGINE / "src/runtime/raylib_lite_game_app.c").read_text()
        fatal = source[source.index("static bool frame_result_is_fatal"):
                       source.index("static void poll_input")]
        for transient in ("RAYLIB_LITE_NOT_READY", "RAYLIB_LITE_BUSY",
                          "RAYLIB_LITE_TIMEOUT", "RAYLIB_LITE_IO_ERROR",
                          "RAYLIB_LITE_PLATFORM_ERROR"):
            self.assertNotIn(transient, fatal)
        self.assertIn("if (frame_result_is_fatal(frame_result))", source)
        self.assertIn("raylib_lite_runtime_stats_record_timing(runtime->update_us", source)
        self.assertIn("raylib_lite_runtime_stats_record_render(", source)
        self.assertNotIn("raylib_lite_runtime_stats_record_frame(", source)

    def test_portable_app_stops_after_failed_start(self) -> None:
        source = (ENGINE / "src/runtime/raylib_lite_game_app.c").read_text(
            encoding="utf-8")
        entered = source.index("started = true;", source.index("if (app->on_start)"))
        called = source.index("result = app->on_start", entered)
        branch = source.index("if (result != RAYLIB_LITE_OK)", called)
        self.assertLess(entered, called)
        self.assertLess(called, branch)
        self.assertIn("if (started && app->on_stop)", source)

    def test_engine_components_do_not_depend_on_concrete_boards(self) -> None:
        forbidden = (
            "esp-mosaico-bsp",
            "mosaico_board_platform",
            "ports/esp_mosaico",
            "MOSAICO_BSP_ROOT",
            "esp_iris",
        )
        roots = [ENGINE / name for name in ENGINE_SOURCE_ROOTS]
        roots.extend((ENGINE / "CMakeLists.txt", ENGINE / "Kconfig"))
        for root in roots:
            paths = [root] if root.is_file() else root.rglob("*")
            for path in paths:
                if not path.is_file():
                    continue
                if path.suffix not in {".c", ".h", ".S", ".txt", ".cmake"} and path.name not in {"CMakeLists.txt", "Kconfig"}:
                    continue
                text = path.read_text(encoding="utf-8")
                with self.subTest(path=path.relative_to(ENGINE)):
                    for token in forbidden:
                        self.assertNotIn(token, text)

    def test_esp_implementations_are_scoped_to_explicit_backends(self) -> None:
        forbidden_include = re.compile(
            r'#\s*include\s*[<"](?:esp_|freertos/|nvs|bsp/|driver/|sdkconfig)')
        for path in (ENGINE / "src").rglob("*"):
            if not path.is_file() or path.suffix not in {".c", ".h"}:
                continue
            relative = path.relative_to(ENGINE).as_posix()
            if relative in ESP_IMPLEMENTATIONS:
                continue
            with self.subTest(path=path.relative_to(ENGINE)):
                source = path.read_text(encoding="utf-8")
                self.assertIsNone(forbidden_include.search(source))
                for token in ("heap_caps_", "esp_timer_", "bsp_",
                              "platform_esp_", "ESP_PLATFORM"):
                    self.assertNotIn(token, source)

    def test_engine_has_no_fixed_board_resolution_or_task_stack_policy(self) -> None:
        forbidden = ("MOSAICO_GAME_WIDTH", "MOSAICO_GAME_HEIGHT",
                     "game_task_stack")
        for root_name in ("src", "include/raylib_lite"):
            for path in (ENGINE / root_name).rglob("*"):
                if not path.is_file() or path.suffix not in {".c", ".h"}:
                    continue
                source = path.read_text(encoding="utf-8")
                with self.subTest(path=path.relative_to(ENGINE)):
                    for token in forbidden:
                        self.assertNotIn(token, source)

    def test_single_engine_component_registration(self) -> None:
        root_cmake = (ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")
        root_manifest = (ENGINE / "idf_component.yml").read_text(encoding="utf-8")
        self.assertEqual(root_cmake.count("idf_component_register("), 1)
        self.assertTrue((ENGINE / "Kconfig").is_file())
        self.assertTrue((ENGINE / "tools/cmake/raylib_lite_native_assets.cmake").is_file())

        for token in (
            "description:",
            "license: Apache-2.0",
            "repository: https://github.com/espressif2022/raylib-lite-engine.git",
        ):
            self.assertIn(token, root_manifest)

        for cmake in (ENGINE / "examples").glob("*/main/CMakeLists.txt"):
            if cmake.parent.parent.name in {"render_benchmark", "frame_compare"}:
                continue
            source = cmake.read_text(encoding="utf-8")
            manifest = cmake.parent / "idf_component.yml"
            top = cmake.parent.parent / "CMakeLists.txt"
            with self.subTest(path=cmake.relative_to(ENGINE)):
                self.assertTrue(manifest.is_file())
                manifest_source = manifest.read_text(encoding="utf-8")
                self.assertIn("espressif2022/raylib-lite-engine:", manifest_source)
                self.assertIn("override_path: ../../..", manifest_source)
                self.assertNotIn("esp-mosaico:", manifest_source)
                self.assertNotIn("../../boards/", manifest_source)
                self.assertNotIn("georgik/raylib", manifest_source)
                if "raylib_lite_native_assets.cmake" in source:
                    self.assertIn("idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT", source)
                    self.assertIn("raylib-lite-engine COMPONENT_DIR", source)
                self.assertNotIn("support/native", source)
                self.assertNotIn("RAYLIB_LITE_EXAMPLE_COMMON_DIR", source)
                self.assertNotIn("../../common", source)
                self.assertNotIn("native_module_main.c", source)
                self.assertIn("examples_common", source)
                self.assertNotRegex(source, r"REQUIRES[^)]*raylib-lite-engine")
                top_source = top.read_text(encoding="utf-8")
                self.assertNotIn("raylib_lite_native_project.cmake", top_source)
                self.assertIn("../common_components/examples_common/project.cmake", top_source)
                self.assertNotIn("../../cmake/", top_source)
                self.assertIn("${RAYLIB_LITE_BOARD}", source)
                self.assertNotIn("${RAYLIB_LITE_ENGINE_ROOT}/cmake/raylib_lite_native_assets.cmake", source)
                if "raylib_lite_native_assets.cmake" in source:
                    self.assertIn("${RAYLIB_LITE_ENGINE_ROOT}/tools/cmake/raylib_lite_native_assets.cmake", source)

    def test_save_core_uses_storage_contract_not_nvs(self) -> None:
        core = (ENGINE / "src/save/raylib_lite_save.c").read_text(
            encoding="utf-8")
        header = (ENGINE / "include/raylib_lite/raylib_lite_save.h").read_text(
            encoding="utf-8")
        self.assertNotIn('#include "nvs.h"', core)
        for symbol in ("nvs_open", "nvs_get_blob", "nvs_set_blob", "nvs_commit"):
            self.assertNotIn(symbol, core)
        self.assertIn("raylib_lite_save_storage_t", header)
        root_cmake = (ENGINE / "CMakeLists.txt").read_text()
        self.assertIn("src/idf/raylib_lite_save_nvs.c", root_cmake)
        self.assertIn("PRIV_REQUIRES esp_timer esp_system heap freertos esp_mmap_assets",
                      root_cmake)
        self.assertIn("nvs_flash log", root_cmake)

    def test_asset_core_uses_backing_contract_not_mmap_implementation(self) -> None:
        core = (ENGINE / "src/assets/raylib_lite_assets.c").read_text(encoding="utf-8")
        header = (ENGINE / "include/raylib_lite/raylib_lite_assets.h").read_text(encoding="utf-8")
        backend = (ENGINE / "src/idf/raylib_lite_assets_mmap.c").read_text(encoding="utf-8")
        cmake = (ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")
        manifest = (ENGINE / "idf_component.yml").read_text(encoding="utf-8")

        for token in ("esp_mmap_assets.h", "mmap_assets_new",
                      "mmap_assets_get_mem", "mmap_assets_get_name"):
            self.assertNotIn(token, core)
        self.assertIn("raylib_lite_asset_backing_t", header)
        self.assertIn("raylib_lite_assets_mount_backing", header)
        self.assertIn("raylib_lite_asset_stream_open", header)
        self.assertIn('#include "esp_mmap_assets.h"', backend)
        private_requires = re.search(r"PRIV_REQUIRES\s+([^)]*)", cmake, re.S)
        self.assertIsNotNone(private_requires)
        self.assertIn("esp_mmap_assets", private_requires.group(1).split())
        self.assertIn("espressif/esp_mmap_assets", manifest)

        for game_manifest in (ENGINE / "examples").glob("*/main/idf_component.yml"):
            with self.subTest(path=game_manifest.relative_to(ENGINE)):
                self.assertNotIn("esp_mmap_assets", game_manifest.read_text())

    def test_result_numeric_abi_is_stable(self) -> None:
        header = ENGINE / "include/raylib_lite/raylib_lite_result.h"
        source = f'''#include "{header}"
_Static_assert(RAYLIB_LITE_OK == 0, "result ABI");
_Static_assert(RAYLIB_LITE_INVALID_ARGUMENT == 1, "result ABI");
_Static_assert(RAYLIB_LITE_INVALID_STATE == 2, "result ABI");
_Static_assert(RAYLIB_LITE_NOT_SUPPORTED == 3, "result ABI");
_Static_assert(RAYLIB_LITE_NO_MEMORY == 4, "result ABI");
_Static_assert(RAYLIB_LITE_NOT_READY == 5, "result ABI");
_Static_assert(RAYLIB_LITE_BUSY == 6, "result ABI");
_Static_assert(RAYLIB_LITE_TIMEOUT == 7, "result ABI");
_Static_assert(RAYLIB_LITE_IO_ERROR == 8, "result ABI");
_Static_assert(RAYLIB_LITE_PLATFORM_ERROR == 9, "result ABI");
_Static_assert(RAYLIB_LITE_NOT_FOUND == 10, "result ABI extension");
_Static_assert(RAYLIB_LITE_INVALID_SIZE == 11, "result ABI extension");
_Static_assert(RAYLIB_LITE_INVALID_CRC == 12, "result ABI extension");
_Static_assert(RAYLIB_LITE_INVALID_VERSION == 13, "result ABI extension");
int main(void) {{ return 0; }}
'''
        with tempfile.TemporaryDirectory() as directory:
            output = str(Path(directory) / "result-abi.o")
            subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", "-pedantic",
                            "-x", "c", "-c", "-o", output, "-"],
                           input=source, text=True, check=True)

    def test_neutral_public_headers_do_not_expose_esp_or_raylib_types(self) -> None:
        self.assertFalse((ENGINE / "include/raylib_lite/raylib_lite_compat.h").exists())
        forbidden = ("esp_err_t", "ESP_ERR_", '#include "raylib.h"',
                     "Texture2D", "Rectangle", "Vector2", "Color")
        for header in (ENGINE / "include/raylib_lite").glob("*.h"):
            source = header.read_text(encoding="utf-8")
            with self.subTest(path=header.relative_to(ENGINE)):
                for token in forbidden:
                    self.assertNotIn(token, source)

    def test_engine_namespace_is_neutral(self) -> None:
        for root_name in ("include", "src", "compat"):
            for path in (ENGINE / root_name).rglob("*"):
                if not path.is_file() or path.suffix not in {".c", ".h", ".S"}:
                    continue
                source = path.read_text(encoding="utf-8")
                with self.subTest(path=path.relative_to(ENGINE)):
                    self.assertNotIn("mosaico_", source)
                    self.assertNotIn("Mosaico", source)

    def test_wall_config_header_compiles_with_strict_c11(self) -> None:
        header = ENGINE / "include/raylib_lite/raylib_lite_wall_config.h"
        source = f'''#define RAYLIB_LITE_WALL_FIXED_PIXELS 1024
#include "{header}"
int main(void) {{ return RAYLIB_LITE_WALL_MODE; }}
'''
        with tempfile.TemporaryDirectory() as directory:
            output = str(Path(directory) / "wall-config.o")
            subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", "-pedantic",
                            "-x", "c", "-c", "-o", output, "-"],
                           input=source, text=True, check=True)

    def test_raylib_name_compatibility_is_explicit(self) -> None:
        fast = (ENGINE / "compat/raylib/include/raylib_lite_raylib_impl.h").read_text()
        compat = (ENGINE / "compat/raylib/include/raylib_lite_raylib.h").read_text()
        for macro in ("InitWindow", "BeginDrawing", "DrawRectangle",
                      "DrawTexture", "DrawText"):
            self.assertNotIn(f"#define {macro} ", fast)
            self.assertIn(f"#define {macro} raylib_lite_raylib_", compat)

        for root in (ENGINE / "examples").glob("*/main"):
            if root.parent.name == "render_benchmark":
                continue
            for path in root.iterdir():
                if path.suffix not in {".c", ".h"}:
                    continue
                source = path.read_text(encoding="utf-8")
                with self.subTest(path=path.relative_to(ENGINE)):
                    self.assertNotIn('#include "raylib_lite_raylib_impl.h"', source)

        for relative in ("src/runtime/raylib_lite_game_app.c",
                         "src/ui/raylib_lite_ui.c"):
            source = (ENGINE / relative).read_text(encoding="utf-8")
            self.assertNotIn('#include "raylib_lite_raylib.h"', source)

    def test_renderer_core_is_raylib_type_neutral(self) -> None:
        renderer = ENGINE / "src/renderer"
        core = (renderer / "raylib_lite_renderer.c").read_text(encoding="utf-8")
        neutral = (ENGINE / "include/raylib_lite/raylib_lite_renderer.h").read_text(encoding="utf-8")
        legacy = (ENGINE / "compat/raylib/include/raylib_lite_2d.h").read_text(encoding="utf-8")
        adapter = (renderer / "raylib_lite_renderer_raylib.c").read_text(encoding="utf-8")
        cmake = (ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")

        for source in (core, neutral):
            for token in ("raylib.h", "Texture2D", "Rectangle", "Vector2",
                          "Color", "PIXELFORMAT_"):
                self.assertNotIn(token, source)
        self.assertIn('#include "raylib_lite_renderer.h"', core)
        self.assertIn('#include "raylib.h"', legacy)
        self.assertIn("Texture2D", legacy)
        self.assertIn("to_renderer_texture", adapter)
        self.assertIn("raylib_lite_renderer_raylib.c", cmake)
        # Upstream raylib is built in-component with the Raylib Lite rcore
        # platform; a second raylib component would duplicate its symbols.
        self.assertIn("src/rcore/raylib_lite_rcore.c", cmake)
        self.assertNotRegex(cmake, r"REQUIRES[^)]*\braylib\b(?!-)")
        self.assertNotIn("georgik/raylib",
                         (ENGINE / "idf_component.yml").read_text(encoding="utf-8"))
        # The IDF component builds the ESP wrapper, which includes the portable
        # core together with the strong ESP raster logger. Building the core as
        # a second archive member would let the weak debug fallback win.
        self.assertIn("src/renderer/raylib_lite_renderer_esp.c", cmake)
        self.assertEqual(cmake.count("src/renderer/raylib_lite_renderer.c\""), 0)
        legacy_header = (ENGINE / "compat/raylib/include/raylib_lite_2d.h").read_text(
            encoding="utf-8")
        for hot in ("raylib_lite_2d_draw_texture_pro", "raylib_lite_2d_draw_textured_triangle",
                    "raylib_lite_2d_draw_textured_quad", "raylib_lite_2d_draw_column",
                    "raylib_lite_2d_draw_span", "raylib_lite_2d_draw_floor_row",
                    "raylib_lite_2d_draw_floor_rows", "raylib_lite_2d_draw_raycast_walls"):
            self.assertIn(f"static inline void {hot}", legacy_header)
            self.assertNotIn(f"void {hot}(", adapter)

    def test_s31_rgb565_acceleration_is_arch_scoped(self) -> None:
        renderer = ENGINE / "src/renderer"
        cmake = (ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")
        s31 = ENGINE / "src/arch/esp32s31/raylib_lite_rgb565_pie.S"
        s3 = ENGINE / "src/arch/esp32s3/raylib_lite_rgb565_pie.S"
        bench = (ENGINE / "examples/render_benchmark/main/sources.cmake").read_text(
            encoding="utf-8")
        self.assertTrue(s31.is_file())
        self.assertTrue(s3.is_file())
        self.assertFalse((renderer / "raylib_lite_rgb565_pie.S").exists())
        self.assertIn('src/renderer/raylib_lite_rgb565.c', cmake)
        self.assertIn('src/arch/esp32s31/raylib_lite_rgb565_pie.S', cmake)
        self.assertIn('IDF_TARGET STREQUAL "esp32s31"', cmake)
        self.assertNotIn('src/arch/esp32s3/raylib_lite_rgb565_pie.S', cmake)
        self.assertIn('src/arch/esp32s3/raylib_lite_rgb565_pie.S', bench)
        self.assertIn('IDF_TARGET STREQUAL "esp32s3"', bench)

    def test_product_app_integrations_are_external(self) -> None:
        examples = (("raylib_shooter", "shooter"),
                    ("tower_defense", "tower"), ("sky_hop", "sky_hop"))
        for directory, stem in examples:
            with self.subTest(example=directory):
                main = ENGINE / "examples" / directory / "main"
                for suffix in (".c", ".h"):
                    self.assertFalse((main / f"{stem}_app{suffix}").exists())
                    self.assertFalse((main / f"{stem}_mosaico_app{suffix}").exists())
                self.assertTrue((main / "game_module.c").is_file())

    def test_example_sources_do_not_include_device_sdks(self) -> None:
        feedback = (ENGINE / "examples/common_components/examples_common/include/native_feedback.h").read_text()
        self.assertNotIn("mosaico_native_feedback_", feedback)

        forbidden = re.compile(
            r'#\s*include\s*[<"](?:esp_|freertos/|nvs|bsp/|driver/|sdkconfig|mmap_generate)')
        for main in (ENGINE / "examples").glob("*/main"):
            for source_path in main.iterdir():
                if source_path.suffix in {".c", ".h"}:
                    if source_path.name == "main.c" or (
                            main.parent.name == "render_benchmark" and
                            source_path.name in ESP_BENCHMARK_SOURCES) or (
                            main.parent.name == "frame_compare" and
                            source_path.name == "app_main.c"):
                        continue
                    with self.subTest(path=source_path.relative_to(ENGINE)):
                        self.assertIsNone(forbidden.search(source_path.read_text()))

    def test_host_contract_namespace_and_v1_values_are_stable(self) -> None:
        host = ENGINE / "host"
        self.assertFalse((host / "include/mosaico_host_game.h").exists())
        self.assertFalse((host / "include/mosaico_game_module.h").exists())
        self.assertFalse((host / "include/mosaico_game.h").exists())
        self.assertTrue((host / "include/raylib_lite_host_game.h").is_file())
        self.assertFalse((host / "include/raylib_lite_game_module.h").exists())
        self.assertFalse((ENGINE / "include/raylib_lite/raylib_lite_host_game.h").exists())
        self.assertFalse((ENGINE / "include/raylib_lite/raylib_lite_game_module.h").exists())
        self.assertTrue((ENGINE / "examples/common_components/examples_common/include/raylib_lite_game_module.h").is_file())
        header = (host / "include/raylib_lite_host_game.h").read_text()
        for token in ("mosaico_host_", "MOSAICO_HOST_"):
            self.assertNotIn(token, header)
        self.assertIn("RAYLIB_LITE_HOST_GAME_ABI_V1 1U", header)
        self.assertIn("RAYLIB_LITE_HOST_INPUT_ACTION = 1", header)
        self.assertIn("RAYLIB_LITE_HOST_INPUT_POINTER = 2", header)
        self.assertIn("RAYLIB_LITE_HOST_INPUT_CONTROL = 3", header)
        self.assertIn("RAYLIB_LITE_HOST_INPUT_IMU = 4", header)
        self.assertIn("RAYLIB_LITE_HOST_CONTROL_PAUSE = 1", header)

        for source_path in host.rglob("*"):
            if not source_path.is_file() or source_path.suffix not in {".c", ".h", ".py"}:
                continue
            source = source_path.read_text(encoding="utf-8")
            with self.subTest(path=source_path.relative_to(ENGINE)):
                self.assertNotIn("mosaico_host_", source)
                self.assertNotIn("MOSAICO_HOST_", source)

    def test_product_runtime_abi_mapping_is_isolated_to_example_bridge(self) -> None:
        bridge = ENGINE / "examples/common_components/examples_common/include/raylib_lite_game_module_contract.h"
        module_header = ENGINE / "examples/common_components/examples_common/include/raylib_lite_game_module.h"
        source = bridge.read_text(encoding="utf-8")
        module_source = module_header.read_text(encoding="utf-8")
        self.assertNotIn("raylib_lite_host_", module_source)
        self.assertNotIn("RAYLIB_LITE_HOST_", module_source)
        self.assertNotIn("MOSAICO_GAME_ELF", source)
        self.assertNotIn("mosaico_runtime_v1.h", source)
        self.assertIn("raylib_lite_game_module.h", source)
        for module in (ENGINE / "examples").glob("*/main/game_module.c"):
            text = module.read_text(encoding="utf-8")
            with self.subTest(path=module.relative_to(ENGINE)):
                self.assertNotIn("mosaico_host_", text)
                self.assertNotIn("MOSAICO_HOST_", text)
                self.assertNotIn("mosaico_game_module_v1_t", text)
                self.assertNotIn("mosaico_runtime_v1_t", text)
                for token in ("mosaico_runtime_", "MosaicoHaptic", "MosaicoPerformance", "MosaicoJpeg"):
                    self.assertNotIn(token, text)
                self.assertIn("raylib_lite_game_module_v1_t", text)
                self.assertNotIn("raylib_lite_host_input_v1_t", text)
                self.assertNotIn("RAYLIB_LITE_HOST_INPUT_", text)
                self.assertNotIn("RAYLIB_LITE_HOST_CONTROL_", text)

    def test_reference_examples_have_host_and_direct_entries(self) -> None:
        for directory in (
            "raylib_shooter", "tower_defense", "sky_hop", "living_worlds",
            "last_zone_extraction", "tomb_raycast", "neon_rift_rally",
        ):
            root = ENGINE / "examples" / directory
            with self.subTest(example=directory):
                self.assertTrue((root / "CMakeLists.txt").is_file())
                self.assertTrue((root / "main/CMakeLists.txt").is_file())
                self.assertTrue((root / "main/idf_component.yml").is_file())
                self.assertTrue((root / "game.sim.json").is_file())

    def test_required_game_matrix_uses_same_module_source_for_host_and_board(self) -> None:
        required = (
            "raylib_shooter", "tower_defense", "sky_hop", "living_worlds",
            "last_zone_extraction", "tomb_raycast", "neon_rift_rally",
        )
        for directory in required:
            root = ENGINE / "examples" / directory
            sim_manifest = json.loads((root / "game.sim.json").read_text())
            cmake = (root / "main/CMakeLists.txt").read_text()
            top = (root / "CMakeLists.txt").read_text()
            component_manifest = (root / "main/idf_component.yml").read_text()
            with self.subTest(game=directory):
                self.assertEqual(sim_manifest.get("schema"), "raylib-lite-game-sim/v1")
                self.assertIn("main/game_module.c", sim_manifest.get("sources", []))
                self.assertTrue(
                    '"game_module.c"' in cmake or
                    ("file(GLOB example_sources" in cmake and
                     "SRCS ${example_sources}" in cmake))
                self.assertNotIn("support/native", cmake)
                self.assertNotIn("../../common", cmake)
                self.assertNotIn("native_module_main.c", cmake)
                self.assertIn("examples_common", cmake)
                self.assertTrue((ENGINE / "examples/common_components/examples_common/include/raylib_lite_game_module_contract.h").is_file())
                self.assertIn("RAYLIB_LITE_GAME_NATIVE=1", cmake)
                self.assertIn("espressif2022/raylib-lite-engine:", component_manifest)
                self.assertNotIn("esp-mosaico:", component_manifest)
                self.assertNotIn("../../boards/", component_manifest)
                self.assertIn("${RAYLIB_LITE_BOARD}", cmake)
                self.assertIn("../common_components/examples_common/project.cmake", top)
                self.assertNotIn("raylib_lite_native_project.cmake", top)

        living_root = ENGINE / "examples/living_worlds"
        living = living_root / "main"
        native = living / "native"
        self.assertFalse((living_root / "native").exists())
        self.assertFalse((living / "main.c").exists())
        self.assertFalse((living / "living_worlds_native.c").exists())
        self.assertFalse((living / "living_worlds_native_assets.c").exists())
        self.assertTrue((living / "living_worlds_native_assets.h").is_file())
        self.assertTrue((native / "living_worlds_native_assets.c").is_file())
        native_source = (native / "living_worlds_native_assets.c").read_text()
        self.assertIn('#include "driver/jpeg_decode.h"', native_source)
        for token in ("esp-mosaico", "esp_mosaico", "esp_iris", "bsp/"):
            self.assertNotIn(token, native_source)
        living_manifest = (living / "idf_component.yml").read_text()
        self.assertNotIn("living_worlds:", living_manifest)
        self.assertNotIn("../../boards/", living_manifest)
        living_cmake = (living / "CMakeLists.txt").read_text()
        self.assertIn("REQUIRES examples_common ${RAYLIB_LITE_BOARD} esp_driver_jpeg log",
                      living_cmake)
        self.assertNotIn("${RAYLIB_LITE_BOARD} living_worlds", living_cmake)
        self.assertIn("raylib_lite_native_embed_assets", living_cmake)
        self.assertIn("EXCLUDE aurora.atlas ocean.atlas sunrise.atlas rainforest.atlas",
                      living_cmake)
        self.assertIn("EXTRA_FILES aurora.jpg ocean.jpg sunrise.jpg rainforest.jpg",
                      living_cmake)
        extension = ENGINE / "examples/boards/esp-mosaico/extensions/living_worlds"
        self.assertFalse((extension / "CMakeLists.txt").exists())
        self.assertFalse((extension / "idf_component.yml").exists())
        self.assertFalse((extension / "living_worlds_native_assets.c").exists())

    def test_examples_common_owns_launcher_and_native_fps(self) -> None:
        common = ENGINE / "examples/common_components/examples_common"
        self.assertFalse((ENGINE / "examples/common").exists())
        cmake = (common / "CMakeLists.txt").read_text()
        self.assertIn("RAYLIB_LITE_EXAMPLE_TARGET_FPS", cmake)
        self.assertIn("RAYLIB_LITE_NATIVE_TARGET_FPS=${RAYLIB_LITE_EXAMPLE_TARGET_FPS}", cmake)

        for game, fps in (("tomb_raycast", 50),):
            top = (ENGINE / "examples" / game / "CMakeLists.txt").read_text()
            main = (ENGINE / "examples" / game / "main/CMakeLists.txt").read_text()
            with self.subTest(game=game):
                self.assertIn(f"set(RAYLIB_LITE_EXAMPLE_TARGET_FPS {fps})", top)
                self.assertNotIn("RAYLIB_LITE_NATIVE_TARGET_FPS", main)

    def test_native_examples_select_board_adapter(self) -> None:
        board = ENGINE / "examples/boards/esp-mosaico"
        board_component = board
        self.assertFalse((ENGINE / "examples/common").exists())
        self.assertFalse((ENGINE / "ports/esp_mosaico").exists())
        self.assertFalse((board / "board.cmake").exists())
        for name in ("board_info.yaml", "board_devices.yaml",
                     "board_peripherals.yaml", "project.cmake",
                     "partitions.csv", "sdkconfig.defaults"):
            with self.subTest(path=name):
                self.assertTrue((board / ("bmgr/esp_mosaico" if name.endswith(".yaml") else "") / name).is_file())
        for name in ("CMakeLists.txt", "idf_component.yml", "board.c"):
            with self.subTest(component_path=name):
                self.assertTrue((board_component / name).is_file())
        self.assertFalse((board / "esp_mosaico_iris.c").exists())
        self.assertFalse((board / "esp_mosaico_iris.h").exists())

        board_manifest = (board_component / "idf_component.yml").read_text()
        self.assertIn('version: "0.1.0"', board_manifest)
        self.assertIn('espressif2022/raylib-lite-engine: "^0.1.0"', board_manifest)
        self.assertIn("espressif/esp_board_manager:", board_manifest)
        self.assertNotIn("esp-mosaico-bsp", board_manifest)
        self.assertNotIn("esp_iris:", board_manifest)
        self.assertNotIn("esp-mosaico-utils", board_manifest)
        project = (board / "project.cmake").read_text()
        self.assertNotIn("FetchContent", project)
        board_cmake = (board_component / "CMakeLists.txt").read_text()
        self.assertNotIn("esp_iris", board_cmake)
        self.assertNotIn("esp_mosaico_app_recovery", board_cmake)
        self.assertNotIn("esp_mosaico_iris.c", board_cmake)
        self.assertNotIn('"../../common"', board_cmake)
        self.assertIn("examples_common", board_cmake)
        self.assertIn("raylib-lite-engine", board_cmake)
        self.assertNotIn("native_module_main.c", board_cmake)
        self.assertNotIn("native_feedback.c", board_cmake)
        self.assertNotIn("support/native", board_cmake)
        self.assertNotIn("RAYLIB_LITE_ENGINE_ROOT", board_cmake)

        common_component = ENGINE / "examples/common_components/examples_common"
        self.assertTrue((common_component / "CMakeLists.txt").is_file())
        common_cmake = (common_component / "CMakeLists.txt").read_text()
        self.assertIn('"native_module_main.c"', common_cmake)
        self.assertIn('"native_feedback.c"', common_cmake)
        self.assertIn('"include"', common_cmake)
        self.assertIn("raylib-lite-engine", common_cmake)
        selector = (common_component / "project.cmake").read_text()
        self.assertIn("gen_board_metadata.yaml", selector)
        self.assertIn('string(REPLACE "_" "-"', selector)
        self.assertIn('set(RAYLIB_LITE_BOARD "${_raylib_lite_selected_board}" CACHE STRING', selector)
        self.assertIn("${_raylib_lite_boards_root}/${_raylib_lite_selected_board}", selector)
        self.assertNotIn("components/${_raylib_lite_selected_board}", selector)
        self.assertIn("RAYLIB_LITE_BOARD_PACKAGE_DIR", selector)
        self.assertIn('"${CMAKE_CURRENT_LIST_DIR}"', selector)
        self.assertIn('"${RAYLIB_LITE_BOARD_DIR}"', selector)
        self.assertNotIn("extensions/${RAYLIB_LITE_GAME_NAME}", selector)
        self.assertIn("was not found under", selector)
        self.assertIn("sdkconfig.defaults", selector)
        self.assertIn("project.cmake", selector)

        board_project = (board / "project.cmake").read_text()
        self.assertIn("gen_bmgr_codes", board_project)
        self.assertNotIn("RAYLIB_LITE_UTILS_DIR", board_project)

        for root in (ENGINE / "examples").iterdir():
            manifest_path = root / "main/idf_component.yml"
            if not manifest_path.is_file() or root.name == "render_benchmark":
                continue
            manifest = manifest_path.read_text()
            top = (root / "CMakeLists.txt").read_text()
            main_cmake = (root / "main/CMakeLists.txt").read_text()
            with self.subTest(game=root.name):
                self.assertNotIn("esp-mosaico:", manifest)
                self.assertNotIn("../../boards/", manifest)
                self.assertIn("../common_components/examples_common/project.cmake", top)
                self.assertIn("examples_common", main_cmake)
                self.assertIn("${RAYLIB_LITE_BOARD}", main_cmake)
                self.assertNotIn("raylib_lite_native_project.cmake", top)

        defaults = (board / "sdkconfig.defaults").read_text()
        self.assertNotIn("ESP_IRIS", defaults)
        partitions = (board / "partitions.csv").read_text()
        self.assertIn("factory", partitions)
        self.assertNotIn("ota_0", partitions)
        for path in board.glob("*"):
            if path.suffix in {".c", ".h", ".yml", ".cmake"}:
                self.assertNotIn("esp_iris", path.read_text())

    def test_shared_example_glue_does_not_include_concrete_bsp(self) -> None:
        forbidden = re.compile(r'#\s*include\s*[<"](?:bsp/|esp_mosaico)')
        common = ENGINE / "examples/common_components/examples_common"
        for source_path in common.rglob("*"):
            if not source_path.is_file() or source_path.suffix not in {".c", ".h"}:
                continue
            with self.subTest(path=source_path.relative_to(ENGINE)):
                self.assertIsNone(forbidden.search(source_path.read_text()))

    def test_audio_core_is_engine_owned_and_games_do_not_get_api_from_board(self) -> None:
        audio = ENGINE / "src/audio"
        cmake = (ENGINE / "CMakeLists.txt").read_text()
        self.assertTrue((audio / "raylib_lite_audio_decode.c").is_file())
        self.assertTrue((audio / "raylib_lite_audio_mixer.c").is_file())
        self.assertIn("src/audio/raylib_lite_audio_decode.c", cmake)
        self.assertIn("src/audio/raylib_lite_audio_mixer.c", cmake)

        board_cmake = (ENGINE / "examples/boards/esp-mosaico/CMakeLists.txt").read_text()
        board_manifest = (ENGINE / "examples/boards/esp-mosaico/idf_component.yml").read_text()
        shared_cmake = (ENGINE / "examples/common_components/examples_audio/CMakeLists.txt").read_text()
        self.assertIn("game_audio.c", shared_cmake)
        self.assertIn("platform_esp_audio.c", shared_cmake)
        self.assertIn("examples_audio", board_cmake)
        self.assertIn("mosaico_audio_codec.c", board_cmake)
        self.assertNotIn("mosaico_game_audio.c", board_cmake)
        self.assertNotIn('"game_audio.c"', board_cmake)
        self.assertNotIn("bsp/esp_mosaico", (ENGINE / "examples/common_components/examples_audio/platform_esp_audio.c").read_text())
        self.assertIn("espressif2022/raylib-lite-engine", board_manifest)
        self.assertNotIn("raylib_lite_audio_decode.c", board_cmake)
        self.assertNotIn("raylib_lite_audio_mixer.c", board_cmake)

        box3 = ENGINE / "examples/boards/esp32-s3-box-3"
        box3_component = box3
        box3_cmake = (box3_component / "CMakeLists.txt").read_text()
        box3_defaults = (box3 / "sdkconfig.defaults").read_text()
        self.assertIn("examples_audio", box3_cmake)
        self.assertIn("box3_audio_codec.c", box3_cmake)
        self.assertIn("CONFIG_PARTITION_TABLE_CUSTOM=y", box3_defaults)
        self.assertIn('CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="../boards/esp32-s3-box-3/partitions.csv"', box3_defaults)
        self.assertIn("CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y", box3_defaults)
        self.assertNotIn("CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y", box3_defaults)
        # Flash layout belongs to the selected Board, independently of Game.

        def partition_bytes(raw: str) -> int:
            value = raw.strip().lower()
            if value.endswith("k"):
                return int(value[:-1], 0) * 1024
            if value.endswith("m"):
                return int(value[:-1], 0) * 1024 * 1024
            return int(value, 0)

        partition_file = box3 / "partitions.csv"
        with partition_file.open(encoding="utf-8", newline="") as stream:
            entries = list(csv.reader(line for line in stream
                if line.strip() and not line.lstrip().startswith("#")))
        factory = [row for row in entries if row[0].strip() == "factory"]
        self.assertEqual(len(factory), 1, "expected one factory app partition")
        self.assertGreaterEqual(len(factory[0]), 5)
        self.assertEqual(factory[0][1].strip(), "app")
        self.assertEqual(factory[0][2].strip(), "factory")
        offset = partition_bytes(factory[0][3])
        size = partition_bytes(factory[0][4])
        self.assertEqual(offset, 0x10000)
        self.assertEqual(size, 15 * 1024 * 1024)
        self.assertLessEqual(offset + size, 16 * 1024 * 1024)

        users = set()
        for source_path in (ENGINE / "examples").glob("*/main/*"):
            if source_path.is_file() and source_path.suffix in {".c", ".h"} and                     '#include "raylib_lite_game_audio.h"' in source_path.read_text(errors="ignore"):
                users.add(source_path.parent)
        self.assertTrue(users)
        for main in users:
            with self.subTest(path=main.relative_to(ENGINE)):
                manifest = (main / "idf_component.yml").read_text()
                self.assertIn("espressif2022/raylib-lite-engine", manifest)

    def test_audio_join_precedes_game_assets_and_board_teardown(self) -> None:
        native = (ENGINE / "examples/common_components/examples_common/native_module_main.c").read_text()
        native_shutdown = native[native.index("error = to_esp_result(raylib_lite_game_app_run(&app))"):]
        self.assertLess(native_shutdown.index("raylib_lite_game_audio_shutdown(3000)"),
                        native_shutdown.index("heap_caps_free(game.state)"))
        self.assertLess(native_shutdown.index("raylib_lite_game_audio_shutdown(3000)"),
                        native_shutdown.index("raylib_lite_assets_unmount()"))

        box3 = (ENGINE / "examples/boards/esp32-s3-box-3/board.c").read_text()
        board_cleanup = box3.split("esp_err_t raylib_lite_example_board_retry_cleanup", 1)[1]
        self.assertLess(board_cleanup.index("raylib_lite_game_audio_shutdown(timeout_ms)"),
                        board_cleanup.index("esp_board_manager_deinit()"))
        mosaico = (ENGINE / "examples/boards/esp-mosaico/board.c").read_text()
        mosaico_cleanup = mosaico.split("esp_err_t raylib_lite_example_board_retry_cleanup", 1)[1]
        self.assertLess(mosaico_cleanup.index("raylib_lite_game_audio_shutdown(timeout_ms)"),
                        mosaico_cleanup.index("mosaico_video_close(p->video, timeout_ms)"))

    def test_game_manifests_do_not_own_board_dependencies(self) -> None:
        forbidden = ("esp_display_present", "lvgl/lvgl", "esp-mosaico-bsp", "esp_iris", "esp-mosaico:")
        for manifest in (ENGINE / "examples").glob("*/main/idf_component.yml"):
            if manifest.parent.parent.name == "render_benchmark":
                continue
            source = manifest.read_text(encoding="utf-8")
            with self.subTest(path=manifest.relative_to(ENGINE)):
                for token in forbidden:
                    self.assertNotIn(token, source)
                self.assertNotIn("../../boards/", source)

        benchmark_top = (ENGINE / "examples/render_benchmark/CMakeLists.txt").read_text()
        self.assertIn("MOSAICO_BSP_COMPONENT_DIR", benchmark_top)
        self.assertFalse((ENGINE / "examples/render_benchmark/main/idf_component.yml").exists())

    def test_game_sources_do_not_depend_on_concrete_boards(self) -> None:
        forbidden = ("mosaico_board_platform", "bsp/esp_mosaico",
                     "esp-mosaico-bsp")
        for main in (ENGINE / "examples").glob("*/main"):
            if main.parent.name == "render_benchmark":
                continue
            for path in main.iterdir():
                if path.suffix not in {".c", ".h"}:
                    continue
                source = path.read_text(encoding="utf-8")
                with self.subTest(path=path.relative_to(ENGINE)):
                    for token in forbidden:
                        self.assertNotIn(token, source)

    def test_native_games_expose_only_the_neutral_lifecycle_hooks(self) -> None:
        product = re.compile(r"iris|MOSAICO_NATIVE_PRODUCT|esp_mosaico_app", re.IGNORECASE)
        for cmake in (ENGINE / "examples").glob("*/main/CMakeLists.txt"):
            with self.subTest(path=cmake.relative_to(ENGINE)):
                self.assertIsNone(product.search(cmake.read_text()))
        launcher = ENGINE / "examples/common_components/examples_common/native_module_main.c"
        text = launcher.read_text()
        for call in ("s_services->boot()", "s_services->first_present()", "s_services->attach(", "s_services->detach()") :
            with self.subTest(path=launcher.relative_to(ENGINE), call=call):
                self.assertIn(call, text)

        for root in (ENGINE / "examples/boards").glob("*/extensions/*"):
            for source_path in root.glob("*.c"):
                source = source_path.read_text(encoding="utf-8")
                with self.subTest(path=source_path.relative_to(ENGINE)):
                    for token in ("app_main(", "raylib_lite_native_boot()",
                                  "raylib_lite_native_first_present()"):
                        self.assertNotIn(token, source)

if __name__ == "__main__":
    unittest.main()
