# SPDX-License-Identifier: Apache-2.0
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class RenderPerfTests(unittest.TestCase):
    def test_fixed_work_fit_and_incomplete_capture(self):
        analyze = load('analyze_raster_bench').analyze
        lines = []
        for repeat in range(3):
            for width in (16, 32, 64, 128, 256):
                spans = 4096 // width * 128
                lines.append(f'raster_bench: storage=internal format=2 varying=1 repeat={repeat} '
                             f'width={width} pixels=262144 spans={spans} triangles=128 '
                             f'us={1000+spans} setup_us=100 raster_us={900+spans}')
        result = analyze('\n'.join(lines))[0]
        self.assertTrue(result['valid_fixed_work'])
        self.assertEqual(result['us_per_span_slope'], 1)
        self.assertEqual(result['r_squared'], 1)
        partial = analyze(lines[0])[0]
        self.assertFalse(partial['valid_fixed_work'])
        self.assertIn('error', partial)
        # Fifteen duplicated cases must not masquerade as a complete experiment.
        duplicate = analyze('\n'.join(lines[:5]*3))[0]
        self.assertFalse(duplicate['valid_fixed_work'])

    def test_store_coverage_is_not_unique_overdraw(self):
        summarize = load('analyze_game_perf').summarize
        old = summarize('fb_runs=480 fb_pixels=230400')
        self.assertFalse(old['raster']['includes_primitives'])
        new = summarize('fb_runs=960 fb_pixels=460800\nraster_path primitive_pixels=230400 clear_pixels=230400')
        self.assertTrue(new['raster']['includes_primitives'])
        self.assertFalse(new['raster']['unique_coverage_measured'])
        self.assertEqual(new['raster']['writes_per_screen'], 2)
        self.assertEqual(new['paths']['primitive_pixels']['mean'], 230400)
        busy = summarize('frame=5 dropped=2 busy=2 superseded=0 errors=0 inflight=3/3')
        self.assertTrue(busy['render_samples_may_include_busy_attempts'])
        self.assertFalse(new['render_samples_may_include_busy_attempts'])
        self.assertFalse(new['accepted'])  # No FPS samples were captured.


if __name__ == '__main__':
    unittest.main()
