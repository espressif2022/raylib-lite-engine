from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "examples/boards/esp-mosaico/project.cmake"


class BoardOverrideTests(unittest.TestCase):
    def configure(self, folder, utils, bsp=None, recovery=None):
        modules = folder / "modules"
        modules.mkdir(exist_ok=True)
        (modules / "FetchContent.cmake").write_text(
            'macro(FetchContent_Declare)\nendmacro()\n'
            'macro(FetchContent_GetProperties name)\n'
            'set(${name}_POPULATED TRUE)\n'
            'set(${name}_SOURCE_DIR "${FETCHCONTENT_SOURCE_DIR_RAYLIB_LITE_MOSAICO_UTILS}")\n'
            'endmacro()\n')
        script = folder / "configure.cmake"
        text = f'set(CMAKE_MODULE_PATH "{modules}")\nset(RAYLIB_LITE_UTILS_DIR "{utils}")\n'
        if bsp:
            text += f'set(RAYLIB_LITE_BSP_DIR "{bsp}")\n'
        if recovery:
            text += f'set(FETCHCONTENT_SOURCE_DIR_RAYLIB_LITE_MOSAICO_UTILS "{recovery}")\n'
        text += f'include("{BOARD}")\n'
        text += f'file(WRITE "{folder}/components.txt" "${{EXTRA_COMPONENT_DIRS}}")\n'
        script.write_text(text)
        return subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)

    def checkout(self, folder):
        root = folder / "utils"
        iris = root / "ESP-Iris/components/esp_iris"
        recovery = root / "esp-mosaico-recovery/components/esp_mosaico_app_recovery"
        for p in (iris, recovery):
            p.mkdir(parents=True)
            (p / "CMakeLists.txt").write_text("# component\n")
            (p / "idf_component.yml").write_text('version: "0.1.0"\n')
        return root, iris, recovery

    def test_component_input_selects_iris_and_recovery_from_same_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            root, iris, recovery = self.checkout(folder)
            for selection in (root, iris):
                result = self.configure(folder, selection)
                self.assertEqual(result.returncode, 0, result.stderr)
                components = (folder / "components.txt").read_text().split(";")
                self.assertIn(str(iris), components)
                self.assertIn(str(recovery), components)

    def test_partial_utils_cannot_silently_fetch_another_recovery(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            root, iris, recovery = self.checkout(folder)
            (recovery / "CMakeLists.txt").unlink()
            result = self.configure(folder, root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("both Iris and Recovery are required", result.stderr)

    def test_wrong_component_and_mixed_checkout_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            root, iris, recovery = self.checkout(folder)
            wrong = folder / "unrelated_component"
            wrong.mkdir()
            (wrong / "CMakeLists.txt").write_text("# component\n")
            (wrong / "idf_component.yml").write_text('version: "0.1.0"\n')
            result = self.configure(folder, root, bsp=wrong)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Expected local component esp-mosaico-bsp", result.stderr)
            result = self.configure(folder, recovery)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Expected esp_iris component", result.stderr)
            result = self.configure(folder, root, recovery=folder / "other_utils")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("same utils checkout", result.stderr)
