from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]
ENGINE_SOURCE_ROOTS = ("src", "include/raylib_lite", "compat/raylib")
ESP_IMPLEMENTATIONS = {
    "src/idf/mosaico_game_assets_mmap.c",
    "src/runtime/raylib_lite_debug.c",
    "src/idf/mosaico_game_save_nvs.c",
    "src/renderer/mosaico_game_2d_esp.c",
    "src/renderer/mosaico_raster_bench.c",
    "src/renderer/mosaico_wall_bench.c",
    "src/renderer/raster_log.c",
    "src/renderer/raster_esp_config.h",
    "src/idf/clock_esp.c",
}
ESP_BENCHMARK_SOURCES = {
    "benchmark_main.c", "core_bench.c", "render_preview.c",
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

    def test_portable_app_stops_after_failed_start(self) -> None:
        source = (ENGINE / "src/runtime/raylib_lite_game_app.c").read_text(
            encoding="utf-8")
        entered = source.index("started = true;", source.index("if (app->on_start)"))
        called = source.index("result = app->on_start", entered)
        branch = source.index("if (result != RAYLIB_LITE_OK)", called)
        self.assertLess(entered, called)
        self.assertLess(called, branch)
        self.assertIn("if (started && app->on_stop)", source)

    def test_core_does_not_depend_on_legacy_mosaico_launcher(self) -> None:
        for root_name in ENGINE_SOURCE_ROOTS:
            root = ENGINE / root_name
            for path in root.rglob("*"):
                if not path.is_file() or path.suffix not in {".c", ".h", ".S"}:
                    continue
                text = path.read_text(encoding="utf-8")
                with self.subTest(path=path.relative_to(ENGINE)):
                    self.assertNotIn("platform_mosaico_launcher", text)

    def test_engine_components_do_not_depend_on_concrete_boards(self) -> None:
        forbidden = (
            "esp-mosaico-bsp",
            "mosaico_board_platform",
            "ports/esp_mosaico",
            "MOSAICO_BSP_ROOT",
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

    def test_mosaico_launcher_has_been_removed(self) -> None:
        self.assertFalse((ENGINE / "components/platform_mosaico_launcher").exists())
        self.assertFalse((ENGINE / "cmake/mosaico_game_example.cmake").exists())

    def test_esp_implementations_are_scoped_to_explicit_backends(self) -> None:
        self.assertTrue((ENGINE / "cmake/raylib_lite_esp.cmake").is_file())
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

    def test_legacy_input_producer_is_not_built(self) -> None:
        cmake = (ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotIn("mosaico_game_input.c", cmake)
        self.assertFalse((ENGINE / "src/input/mosaico_game_input.c").exists())
        self.assertFalse((ENGINE / "include/raylib_lite/mosaico_game_input.h").exists())
        action = (ENGINE / "include/raylib_lite/raylib_lite_action.h").read_text(
            encoding="utf-8")
        self.assertIn('#include "raylib_lite_input.h"', action)
        self.assertNotIn('#include "mosaico_game.h"', action)

    def test_single_engine_component_registration(self) -> None:
        root_cmake = (ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertEqual(root_cmake.count("idf_component_register("), 1)
        self.assertTrue((ENGINE / "idf_component.yml").is_file())
        self.assertTrue((ENGINE / "Kconfig").is_file())

        self.assertFalse((ENGINE / "components").exists())

        helper = (ENGINE / "cmake/raylib_lite_esp.cmake").read_text(
            encoding="utf-8")
        self.assertIn('"${RAYLIB_LITE_ENGINE_ROOT}"', helper)
        for old in ("mosaico_game_app", "mosaico_game_assets",
                    "mosaico_game_2d", "mosaico_raylib_fast",
                    "raylib_lite_platform", "raylib_lite_runner"):
            self.assertNotIn(f"components/${{{old}}}", helper)

        for cmake in (ENGINE / "examples").glob("*/main/CMakeLists.txt"):
            if cmake.parent.parent.name == "render_benchmark":
                continue
            source = cmake.read_text(encoding="utf-8")
            with self.subTest(path=cmake.relative_to(ENGINE)):
                match = re.search(r"REQUIRES\s+([^)]*)", source, re.S)
                self.assertIsNotNone(match)
                requirements = match.group(1).split()
                self.assertIn("raylib-lite-engine", requirements)
                for old in ("mosaico_game", "mosaico_game_app",
                            "mosaico_game_assets", "mosaico_game_audio",
                            "mosaico_game_2d", "mosaico_raylib_fast",
                            "mosaico_raylib_port", "raylib_lite_platform",
                            "raylib_lite_runner"):
                    self.assertNotIn(old, requirements)

    def test_legacy_runtime_wrapper_has_been_removed(self) -> None:
        self.assertFalse((ENGINE / "src/runtime/mosaico_game.c").exists())
        self.assertFalse((ENGINE / "include/raylib_lite/mosaico_game.h").exists())
        forbidden = ("MosaicoGameInit", "MosaicoGameShutdown",
                     "MosaicoGamePollDeviceEvent", "MosaicoGamePostDeviceEvent",
                     "MosaicoGameGetStats", "MosaicoGameRecord")
        for root_name in ("src", "include/raylib_lite", "examples"):
            for path in (ENGINE / root_name).rglob("*"):
                if not path.is_file() or path.suffix not in {".c", ".h"}:
                    continue
                if "managed_components" in path.parts or any(
                        part.startswith("build") for part in path.parts):
                    continue
                source = path.read_text(encoding="utf-8")
                with self.subTest(path=path.relative_to(ENGINE)):
                    for token in forbidden:
                        self.assertNotIn(token, source)

    def test_save_core_uses_storage_contract_not_nvs(self) -> None:
        core = (ENGINE / "src/save/mosaico_game_save.c").read_text(
            encoding="utf-8")
        header = (ENGINE / "include/raylib_lite/mosaico_game_save.h").read_text(
            encoding="utf-8")
        self.assertNotIn('#include "nvs.h"', core)
        for symbol in ("nvs_open", "nvs_get_blob", "nvs_set_blob", "nvs_commit"):
            self.assertNotIn(symbol, core)
        self.assertIn("mosaico_save_storage_t", header)
        root_cmake = (ENGINE / "CMakeLists.txt").read_text()
        self.assertIn("src/idf/mosaico_game_save_nvs.c", root_cmake)
        self.assertIn("PRIV_REQUIRES esp_timer esp_system heap freertos esp_mmap_assets",
                      root_cmake)
        self.assertIn("nvs_flash log", root_cmake)

    def test_asset_core_uses_backing_contract_not_mmap_implementation(self) -> None:
        core = (ENGINE / "src/assets/mosaico_game_assets.c").read_text(encoding="utf-8")
        header = (ENGINE / "include/raylib_lite/mosaico_game_assets.h").read_text(encoding="utf-8")
        backend = (ENGINE / "src/idf/mosaico_game_assets_mmap.c").read_text(encoding="utf-8")
        cmake = (ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")
        manifest = (ENGINE / "idf_component.yml").read_text(encoding="utf-8")

        for token in ("esp_mmap_assets.h", "mmap_assets_new",
                      "mmap_assets_get_mem", "mmap_assets_get_name"):
            self.assertNotIn(token, core)
        self.assertIn("mosaico_asset_backing_t", header)
        self.assertIn("mosaico_game_assets_mount_backing", header)
        self.assertIn("mosaico_game_asset_stream_open", header)
        self.assertIn('#include "esp_mmap_assets.h"', backend)
        private_requires = re.search(r"PRIV_REQUIRES\s+([^)]*)", cmake, re.S)
        self.assertIsNotNone(private_requires)
        self.assertIn("esp_mmap_assets", private_requires.group(1).split())
        self.assertIn("espressif/esp_mmap_assets", manifest)

        for game_manifest in (ENGINE / "examples").glob("*/main/idf_component.yml"):
            with self.subTest(path=game_manifest.relative_to(ENGINE)):
                self.assertNotIn("esp_mmap_assets", game_manifest.read_text())

    def test_error_compatibility_header_compiles_without_sdk(self) -> None:
        header = ENGINE / "include/raylib_lite/raylib_lite_compat.h"
        source = f'''#include "{header}"
_Static_assert(sizeof(esp_err_t) == sizeof(int), "legacy ABI");
_Static_assert(ESP_OK == 0 && ESP_ERR_INVALID_ARG == 0x102, "error ABI");
_Static_assert(ESP_ERR_INVALID_CRC == 0x109, "save ABI");
int main(void) {{ return ESP_OK; }}
'''
        with tempfile.TemporaryDirectory() as directory:
            output = str(Path(directory) / "compat.o")
            subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", "-x", "c",
                            "-c", "-o", output, "-"], input=source,
                           text=True, check=True)

    def test_wall_config_header_compiles_with_strict_c11(self) -> None:
        header = ENGINE / "include/raylib_lite/mosaico_wall_config.h"
        source = f'''#include "{header}"
int main(void) {{ return M2D_WALL_MODE; }}
'''
        with tempfile.TemporaryDirectory() as directory:
            output = str(Path(directory) / "wall-config.o")
            subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", "-pedantic",
                            "-x", "c", "-c", "-o", output, "-"],
                           input=source, text=True, check=True)

    def test_raylib_name_compatibility_is_explicit(self) -> None:
        fast = (ENGINE / "include/raylib_lite/mosaico_raylib_fast.h").read_text()
        compat = (ENGINE / "compat/raylib/include/raylib_lite_raylib.h").read_text()
        for macro in ("InitWindow", "BeginDrawing", "DrawRectangle",
                      "DrawTexture", "DrawText"):
            self.assertNotIn(f"#define {macro} ", fast)
            self.assertIn(f"#define {macro} MosaicoFast", compat)

        for root in (ENGINE / "examples").glob("*/main"):
            if root.parent.name == "render_benchmark":
                continue
            for path in root.iterdir():
                if path.suffix not in {".c", ".h"}:
                    continue
                source = path.read_text(encoding="utf-8")
                with self.subTest(path=path.relative_to(ENGINE)):
                    self.assertNotIn('#include "mosaico_raylib_fast.h"', source)

        for relative in ("src/runtime/raylib_lite_game_app.c",
                         "src/ui/mosaico_game_ui.c"):
            source = (ENGINE / relative).read_text(encoding="utf-8")
            self.assertNotIn('#include "raylib_lite_raylib.h"', source)

    def test_renderer_core_is_raylib_type_neutral(self) -> None:
        renderer = ENGINE / "src/renderer"
        core = (renderer / "mosaico_game_2d.c").read_text(encoding="utf-8")
        neutral = (ENGINE / "include/raylib_lite/mosaico_renderer.h").read_text(encoding="utf-8")
        legacy = (ENGINE / "include/raylib_lite/mosaico_game_2d.h").read_text(encoding="utf-8")
        adapter = (renderer / "mosaico_game_2d_raylib.c").read_text(encoding="utf-8")
        cmake = (ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")

        for source in (core, neutral):
            for token in ("raylib.h", "Texture2D", "Rectangle", "Vector2",
                          "Color", "PIXELFORMAT_"):
                self.assertNotIn(token, source)
        self.assertIn('#include "mosaico_renderer.h"', core)
        self.assertIn('#include "raylib.h"', legacy)
        self.assertIn("Texture2D", legacy)
        self.assertIn("to_renderer_texture", adapter)
        self.assertIn("mosaico_game_2d_raylib.c", cmake)
        self.assertIn("REQUIRES raylib", cmake)
        legacy_header = (ENGINE / "include/raylib_lite/mosaico_game_2d.h").read_text(
            encoding="utf-8")
        for hot in ("Mosaico2DDrawTexturePro", "Mosaico2DDrawTexturedTriangle",
                    "Mosaico2DDrawTexturedQuad", "Mosaico2DDrawColumn",
                    "Mosaico2DDrawSpan", "Mosaico2DDrawFloorRow",
                    "Mosaico2DDrawFloorRows", "Mosaico2DDrawRaycastWalls"):
            self.assertIn(f"static inline void {hot}", legacy_header)
            self.assertNotIn(f"void {hot}(", adapter)

    def test_s31_rgb565_acceleration_is_arch_scoped(self) -> None:
        renderer = ENGINE / "src/renderer"
        cmake = (ENGINE / "CMakeLists.txt").read_text(encoding="utf-8")
        arch = ENGINE / "src/arch/esp32s31/mosaico_rgb565_pie.S"
        self.assertTrue(arch.is_file())
        self.assertFalse((renderer / "mosaico_rgb565_pie.S").exists())
        self.assertIn('src/renderer/mosaico_rgb565.c', cmake)
        self.assertIn('src/arch/esp32s31/mosaico_rgb565_pie.S', cmake)
        self.assertIn('IDF_TARGET STREQUAL "esp32s31"', cmake)

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
        forbidden = re.compile(
            r'#\s*include\s*[<"](?:esp_|freertos/|nvs|bsp/|driver/|sdkconfig|mmap_generate)')
        for main in (ENGINE / "examples").glob("*/main"):
            for path in main.iterdir():
                if path.suffix in {".c", ".h"}:
                    if path.name in {"main.c", "living_worlds_native.c",
                                     "living_worlds_native.h"} or (
                            main.parent.name == "render_benchmark" and
                            path.name in ESP_BENCHMARK_SOURCES):
                        continue  # ESP-only entry and adapter, separate from gameplay.
                    with self.subTest(path=path.relative_to(ENGINE)):
                        self.assertIsNone(forbidden.search(path.read_text()))

    def test_reference_examples_have_host_and_direct_entries(self) -> None:
        for directory in (
            "raylib_shooter", "tower_defense", "sky_hop", "living_worlds",
            "last_zone_extraction", "tomb_raycast",
        ):
            root = ENGINE / "examples" / directory
            with self.subTest(example=directory):
                self.assertTrue((root / "CMakeLists.txt").is_file())
                self.assertTrue((root / "main/CMakeLists.txt").is_file())
                self.assertTrue((root / "main/idf_component.yml").is_file())
                self.assertTrue((root / "game.sim.json").is_file())

    def test_native_examples_select_board_adapter(self) -> None:
        board = ENGINE / "examples/boards/esp-mosaico"
        self.assertFalse((ENGINE / "ports/esp_mosaico").exists())
        for name in ("CMakeLists.txt", "board.cmake", "board.c",
                     "sdkconfig.defaults"):
            with self.subTest(path=name):
                self.assertTrue((board / name).is_file())

        resolver = (ENGINE / "cmake/raylib_lite_native_project.cmake").read_text()
        self.assertIn("RAYLIB_LITE_BOARD", resolver)
        self.assertIn("examples/boards/${RAYLIB_LITE_BOARD}", resolver)
        for board_token in ("esp-mosaico", "MOSAICO_BSP_ROOT",
                            "mosaico_board_platform", "raylib_lite_esp_add_port"):
            self.assertNotIn(board_token, resolver)

        engine_helper = (ENGINE / "cmake/raylib_lite_esp.cmake").read_text()
        self.assertNotIn("raylib_lite_esp_add_port", engine_helper)
        self.assertNotIn("esp_mosaico", engine_helper)

        for cmake in (ENGINE / "examples").glob("*/main/CMakeLists.txt"):
            if cmake.parent.parent.name == "render_benchmark":
                continue
            with self.subTest(path=cmake.relative_to(ENGINE)):
                source = cmake.read_text()
                self.assertNotIn("RAYLIB_LITE_BOARD_COMPONENT", source)
                self.assertNotIn("RAYLIB_LITE_SELECTED_BOARD", source)

        board_selector = (board / "board.cmake").read_text()
        self.assertIn('"${CMAKE_CURRENT_LIST_DIR}"', board_selector)
        self.assertIn("EXTRA_COMPONENT_DIRS", board_selector)

    def test_shared_example_glue_does_not_include_concrete_bsp(self) -> None:
        forbidden = re.compile(r'#\s*include\s*[<"](?:bsp/|esp_mosaico)')
        for path in (ENGINE / "examples/common").glob("*"):
            if path.suffix not in {".c", ".h"}:
                continue
            with self.subTest(path=path.relative_to(ENGINE)):
                self.assertIsNone(forbidden.search(path.read_text()))

    def test_audio_core_is_engine_owned_and_games_do_not_get_api_from_board(self) -> None:
        audio = ENGINE / "src/audio"
        cmake = (ENGINE / "CMakeLists.txt").read_text()
        self.assertTrue((audio / "raylib_lite_audio_decode.c").is_file())
        self.assertTrue((audio / "raylib_lite_audio_mixer.c").is_file())
        self.assertIn("src/audio/raylib_lite_audio_decode.c", cmake)
        self.assertIn("src/audio/raylib_lite_audio_mixer.c", cmake)

        board_cmake = (ENGINE / "examples/boards/esp-mosaico/CMakeLists.txt").read_text()
        self.assertIn("raylib-lite-engine", board_cmake)
        board_requires = re.search(r"REQUIRES\s+([^)]*)", board_cmake, re.S)
        self.assertIsNotNone(board_requires)
        self.assertNotIn("mosaico_game_audio", board_requires.group(1).split())
        self.assertNotIn("raylib_lite_audio_decode.c", board_cmake)
        self.assertNotIn("raylib_lite_audio_mixer.c", board_cmake)

        users = set()
        for path in (ENGINE / "examples").glob("*/main/*"):
            if path.is_file() and path.suffix in {".c", ".h"} and \
                    '#include "mosaico_game_audio.h"' in path.read_text(errors="ignore"):
                users.add(path.parent)
        self.assertTrue(users)
        for main in users:
            with self.subTest(path=main.relative_to(ENGINE)):
                cmake = (main / "CMakeLists.txt").read_text()
                self.assertIn("raylib-lite-engine", cmake)
                self.assertNotIn("mosaico_game_audio", cmake)

    def test_game_manifests_do_not_own_board_dependencies(self) -> None:
        forbidden = ("esp_display_present", "lvgl/lvgl", "esp-mosaico")
        for manifest in (ENGINE / "examples").glob("*/main/idf_component.yml"):
            source = manifest.read_text(encoding="utf-8")
            with self.subTest(path=manifest.relative_to(ENGINE)):
                for token in forbidden:
                    self.assertNotIn(token, source)

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
        entries = {
            "examples/common/native_module_main.c": ("raylib_lite_native_boot()",
                                                     "raylib_lite_native_first_present()"),
            "examples/living_worlds/main/main.c": ("raylib_lite_native_boot()",),
            "examples/living_worlds/main/living_worlds_native.c": (
                "raylib_lite_native_first_present()",),
        }
        for relative, calls in entries.items():
            text = (ENGINE / relative).read_text()
            for call in calls:
                with self.subTest(path=relative, call=call):
                    self.assertIn(call, text)


if __name__ == "__main__":
    unittest.main()
