from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "examples/boards/esp-mosaico/project.cmake"

class BoardOverrideTests(unittest.TestCase):
    def configure(self, folder, bsp=None):
        script = folder / "configure.cmake"
        text = f'set(RAYLIB_LITE_BSP_DIR "{bsp or ""}")\n'
        text += f'include("{BOARD}")\n'
        text += f'file(WRITE "{folder}/components.txt" "${{EXTRA_COMPONENT_DIRS}}")\n'
        script.write_text(text)
        return subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True)

    def test_standalone_configuration_needs_no_utils(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            result = self.configure(folder)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((folder / "components.txt").read_text(), "")

    def test_local_bsp_does_not_register_product_splash(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            for name in ["esp-mosaico-bsp", "mosaico_boot_splash"]:
                p = folder / "bsp/components" / name
                p.mkdir(parents=True)
                (p / "CMakeLists.txt").write_text("# component\n")
                (p / "idf_component.yml").write_text('version: "0.1.0"\n')
            result = self.configure(folder, folder / "bsp")
            self.assertEqual(result.returncode, 0, result.stderr)
            components = (folder / "components.txt").read_text().split(";")
            self.assertIn(str(folder / "bsp/components/esp-mosaico-bsp"), components)
            self.assertNotIn(str(folder / "bsp/components/mosaico_boot_splash"), components)
