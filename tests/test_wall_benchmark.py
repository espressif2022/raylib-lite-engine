# SPDX-License-Identifier: Apache-2.0
"""Fail-closed acceptance/report tests. Actual C oracle is tools/wall_benchmark.py."""
import copy
import importlib.util
import json
import sys
import tempfile
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('wall_benchmark', ROOT/'tools/wall_benchmark.py')
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)
sys.path.insert(0, str(ROOT/'tools'))
import wall_benchmark_device as device


def rows():
    return [dict(case=name, mode=3, fixed=16, bound=.25, coverage_errors=0,
                 texel_errors=1, max_texel_error=1, pixels=100, uv_max=.2,
                 uv_sum=5., uv_samples=100, segments=10, uv_reciprocals=20,
                 hash=123, times_us=[10.]*7) for name in bench.CASES]


def encoded(value):
    return '\n'.join('WALLBENCH '+json.dumps(r) for r in value)


class WallBenchmarkTests(unittest.TestCase):
    def test_complete_log_and_compiled_configuration(self):
        value=bench.parse(encoded(rows()))
        bench.validate_variant(value, 'adaptive025')
        with self.assertRaises(ValueError): bench.validate_variant(value, 'exact')

    def test_incomplete_duplicate_nan_and_bad_samples_rejected(self):
        for mutate in (lambda v: v.pop(), lambda v: v.append(v[0]),
                       lambda v: v[0].update(uv_max=float('nan')),
                       lambda v: v[0].update(times_us=[0.]*7),
                       lambda v: v[0].update(times_us=[1.]*6),
                       lambda v: v[0].update(mode=1)):
            value=rows();mutate(value)
            with self.assertRaises(ValueError): bench.parse(encoded(value))

    def test_quality_gate_cannot_be_bought_with_speed(self):
        for mutation in ({'coverage_errors': 1}, {'uv_max': .27},
                         {'max_texel_error': 2}, {'uv_samples': 99}):
            audit=rows();audit[0].update(mutation)
            timing={'legacy': {c:[100.]*21 for c in bench.CASES},
                    'candidate': {c:[1.]*21 for c in bench.CASES}}
            report=bench.summarize({'candidate':audit},timing)['candidate']
            self.assertFalse(report['accepted']);self.assertIsNone(report['host_micro_score'])

    def test_unstable_timing_withholds_score(self):
        timing={'legacy': {c:[10.]*21 for c in bench.CASES},
                'candidate': {c:[1.,10.,30.]*7 for c in bench.CASES}}
        report=bench.summarize({'candidate':rows()},timing)['candidate']
        self.assertTrue(report['quality_passed']);self.assertFalse(report['accepted'])
        self.assertFalse(report['timing_stable'])
        self.assertIsNone(report['host_micro_score'])

    def test_unstable_baseline_withholds_candidate_score(self):
        timing={'legacy': {c:[1.,10.,30.]*7 for c in bench.CASES},
                'candidate': {c:[10.]*21 for c in bench.CASES}}
        report=bench.summarize({'candidate':rows()},timing)['candidate']
        self.assertTrue(report['timing_stable']);self.assertFalse(report['baseline_timing_stable'])
        self.assertFalse(report['accepted']);self.assertIsNone(report['host_micro_score'])

    def test_strict_reference_and_comparability(self):
        self.assertFalse(bench.quality(rows()))
        self.assertTrue(bench.quality(rows(), exact=True))
        report={k:{} for k in ('schema','policy','workload_sha256','environment','protocol','results')}
        for key in ('policy','workload_sha256','environment','protocol','results'):
            other=copy.deepcopy(report);other[key]={'changed':1}
            with self.assertRaises(ValueError):bench.check_comparable(report,other)


class DeviceImportTests(unittest.TestCase):
    def fixture(self, directory):
        folder=Path(directory)
        (folder/'workload.c').write_text('synthetic test fixture, not measurement')
        (folder/'audit.bin').write_bytes(b'audit-fixture')
        (folder/'timing.bin').write_bytes(b'timing-fixture')
        (folder/'config').write_text('fixture config')
        env={k:'fixture' for k in ('board','chip','cpu_hz','psram_hz','toolchain',
             'idf_commit','framebuffer_storage','texture_storage','lut_storage',
             'power_mode','background_tasks')}
        env.update(display_active=False,raster_profile=False,cpu_hz=320000000,psram_hz=80000000)
        manifest={'schema':'wall-device-input/v1','workload_source':'workload.c',
                  'environment':env,'variants':{}}
        for label,(mode,fixed,bound) in bench.VARIANTS.items():
            audit=rows()
            for row in audit:row.update(mode=mode,fixed=fixed,bound=bound,uv_max=.001)
            audit_path=label+'-audit.log';(folder/audit_path).write_text(encoded(audit))
            logs=[]
            for repeat in range(3):
                timing=copy.deepcopy(audit)
                for row in timing:row.update(uv_samples=0,times_us=[10+repeat*.1]*7)
                path=f'{label}-{repeat}.log';logs.append(path)
                (folder/path).write_text(encoded(timing))
            manifest['variants'][label]={'audit_image':'audit.bin','timing_image':'timing.bin',
                'audit_sdkconfig':'config','timing_sdkconfig':'config',
                'audit_log':audit_path,'timing_logs':logs}
        path=folder/'manifest.json';path.write_text(json.dumps(manifest))
        return path,manifest

    def test_device_import_and_no_host_score(self):
        with tempfile.TemporaryDirectory() as directory:
            path,_=self.fixture(directory);report=device.import_run(path)
            result=report['results']['adaptive025']
            self.assertIn('device_micro_score',result)
            self.assertNotIn('host_micro_score',result)
            self.assertEqual(report['full_game_acceptance'],'pending')
            self.assertIsNone(report['display_fps'])

    def test_partial_or_instrumented_device_runs_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path,manifest=self.fixture(directory)
            manifest['variants']['exact']['timing_logs'].pop()
            path.write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):device.import_run(path)
        with tempfile.TemporaryDirectory() as directory:
            path,manifest=self.fixture(directory)
            row=manifest['variants']['adaptive025']
            row['timing_logs'][0]=row['audit_log']
            path.write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):device.import_run(path)


if __name__=='__main__':unittest.main()
