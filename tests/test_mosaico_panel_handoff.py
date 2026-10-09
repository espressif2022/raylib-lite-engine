"""Execute the actual Board Manager factory's adoption and later reset paths."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MosaicoPanelHandoffTests(unittest.TestCase):
    def test_adoption_is_once_and_later_reset_uses_complete_initialization(self):
        cc = shutil.which('cc')
        if not cc:
            self.skipTest('C compiler unavailable')
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            for name in ('esp_err.h', 'esp_lcd_co5300.h',
                         'esp_lcd_panel_interface.h', 'esp_lcd_panel_io.h',
                         'esp_lcd_panel_ops.h', 'esp_lcd_touch_cst9220.h',
                         'soc/lp_system_reg.h', 'soc/soc.h'):
                header = folder / name
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text('#include "sdk.h"\n')
            binary = folder / 'panel'
            subprocess.run([
                cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-I', str(folder), '-I', str(ROOT / 'tests/fakes/mosaico_panel'),
                str(ROOT / 'tests/test_mosaico_panel_handoff.c'),
                str(ROOT / 'examples/boards/esp-mosaico/mosaico_setup_device.c'),
                '-o', str(binary),
            ], check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)
