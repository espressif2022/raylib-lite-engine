from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "examples/boards/esp-mosaico/project.cmake"

class BoardOverrideTests(unittest.TestCase):
    def configure(self, folder, bsp=None):
        script = folder / "configure.cmake"
        text = f'set(CMAKE_SOURCE_DIR "{folder}")\n'
        text += f'include("{BOARD}")\n'
        text += f'file(WRITE "{folder}/components.txt" "${{EXTRA_COMPONENT_DIRS}}")\n'
        text += f'file(WRITE "{folder}/defaults.txt" "${{SDKCONFIG_DEFAULTS}}")\n'
        script.write_text(text)
        return subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)

    def test_standalone_configuration_needs_no_utils(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            result = self.configure(folder)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((folder / "components.txt").read_text(), "")

    def test_project_generated_profile_is_accepted_and_defaults_are_loaded(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            generated = folder / "components/gen_bmgr_codes"
            generated.mkdir(parents=True)
            defaults = generated / "board_manager.defaults"
            defaults.write_text("CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT=y\n")
            (generated / "gen_board_device_custom.h").write_text("/* custom factories */\n")
            result = self.configure(folder)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(str(defaults), (folder / "defaults.txt").read_text())

    def test_local_bsp_environment_does_not_register_another_hardware_owner(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            result = self.configure(folder, folder / "bsp")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((folder / "components.txt").read_text(), "")

    def test_official_profile_is_rejected_with_regeneration_command(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            generated = folder / "components/gen_bmgr_codes"
            generated.mkdir(parents=True)
            (generated / "board_manager.defaults").write_text("")
            result = self.configure(folder)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("custom Board Manager profile", result.stderr)
            self.assertIn("-b esp_mosaico", result.stderr)
