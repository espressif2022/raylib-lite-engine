from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ENGINE = Path(__file__).resolve().parents[1]
CORE_COMPONENTS = (
    "raylib_lite_platform",
    "raylib_lite_runner",
    "mosaico_game_app",
    "mosaico_raylib_port",
    "mosaico_raylib_fast",
    "mosaico_game_audio",
    "mosaico_game_input",
)
ESP_IMPLEMENTATIONS = {
    "mosaico_game/mosaico_game.c",
    "mosaico_game_assets/mosaico_game_assets.c",
    "mosaico_game_debug/mosaico_game_debug.c",
    "mosaico_game_save/mosaico_game_save.c",
    "mosaico_game_2d/mosaico_game_2d_esp.c",
    "mosaico_game_2d/mosaico_raster_bench.c",
    "mosaico_game_2d/raster_log.c",
    "mosaico_game_2d/raster_esp_config.h",
    "raylib_lite_platform/clock_esp.c",
}


class PlatformDependencyBoundaryTests(unittest.TestCase):
    def test_runtime_frame_backpressure_is_not_terminal(self):
        source = (ENGINE / "components" / "mosaico_game_app" /
                  "mosaico_game_app.c").read_text()
        fatal = source[source.index("static bool frame_result_is_fatal"):
                       source.index("static mosaico_device_event_type_t")]
        for transient in ("RAYLIB_LITE_NOT_READY", "RAYLIB_LITE_BUSY",
                          "RAYLIB_LITE_TIMEOUT", "RAYLIB_LITE_IO_ERROR",
                          "RAYLIB_LITE_PLATFORM_ERROR"):
            self.assertNotIn(transient, fatal)
        self.assertIn("if (frame_result_is_fatal(frame_result))", source)

    def test_portable_app_stops_after_failed_start(self) -> None:
        source = (ENGINE / "components/mosaico_game_app/mosaico_game_app.c").read_text(
            encoding="utf-8")
        entered = source.index("started = true;", source.index("if (app->on_start)"))
        called = source.index("result = app->on_start", entered)
        branch = source.index("if (result != RAYLIB_LITE_OK)", called)
        self.assertLess(entered, called)
        self.assertLess(called, branch)
        self.assertIn("if (started && app->on_stop)", source)

    def test_core_does_not_depend_on_legacy_mosaico_launcher(self) -> None:
        for component in CORE_COMPONENTS:
            root = ENGINE / "components" / component
            for path in root.rglob("*"):
                if not path.is_file():
                    continue
                if path.suffix not in {".c", ".h", ".txt"} and path.name != "CMakeLists.txt":
                    continue
                text = path.read_text(encoding="utf-8")
                with self.subTest(path=path.relative_to(ENGINE)):
                    self.assertNotIn("platform_mosaico_launcher", text)

    def test_mosaico_launcher_has_been_removed(self) -> None:
        self.assertFalse((ENGINE / "components/platform_mosaico_launcher").exists())
        self.assertFalse((ENGINE / "cmake/mosaico_game_example.cmake").exists())

    def test_esp_implementations_are_scoped_to_their_components(self) -> None:
        self.assertFalse((ENGINE / "components/platform_esp_audio").exists())
        self.assertTrue((ENGINE / "cmake/raylib_lite_esp.cmake").is_file())
        forbidden_include = re.compile(
            r'#\s*include\s*[<"](?:esp_|freertos/|nvs|bsp/|driver/|sdkconfig)')
        for path in (ENGINE / "components").rglob("*"):
            if not path.is_file():
                continue
            relative = path.relative_to(ENGINE / "components").as_posix()
            if relative in ESP_IMPLEMENTATIONS:
                continue
            with self.subTest(path=path.relative_to(ENGINE)):
                if path.name == "CMakeLists.txt":
                    self.assertIn("idf_component_register", path.read_text())
                if path.suffix not in {".c", ".h"}:
                    continue
                if path.name == "raylib_lite_compat.h":
                    continue  # Frozen ABI typedefs; no platform services.
                source = path.read_text(encoding="utf-8")
                self.assertIsNone(forbidden_include.search(source))
                for token in ("heap_caps_", "esp_timer_", "bsp_",
                              "platform_esp_", "ESP_PLATFORM"):
                    self.assertNotIn(token, source)

    def test_error_compatibility_header_compiles_without_sdk(self) -> None:
        header = ENGINE / "components/raylib_lite_platform/include/raylib_lite_compat.h"
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
                    with self.subTest(path=path.relative_to(ENGINE)):
                        self.assertIsNone(forbidden.search(path.read_text()))

    def test_reference_examples_are_host_or_portable_sources_only(self) -> None:
        for directory in (
            "raylib_shooter", "tower_defense", "sky_hop", "living_worlds",
            "last_zone_extraction", "tomb_explorer",
        ):
            root = ENGINE / "examples" / directory
            with self.subTest(example=directory):
                self.assertFalse((root / "CMakeLists.txt").exists())
                self.assertFalse((root / "main/CMakeLists.txt").exists())
                self.assertFalse((root / "main/main.c").exists())
                self.assertFalse((root / "main/idf_component.yml").exists())
                self.assertTrue((root / "game.sim.json").is_file())


if __name__ == "__main__":
    unittest.main()
