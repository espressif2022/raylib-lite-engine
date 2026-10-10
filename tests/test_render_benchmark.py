# SPDX-License-Identifier: Apache-2.0
"""Standalone example and fail-closed collection tests."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import render_benchmark as bench


class RenderExampleTests(unittest.TestCase):
    def test_registry_references_and_matrix(self):
        registry=json.loads((bench.EXAMPLE/'config/techniques.json').read_text())
        self.assertEqual(len({x['id'] for x in registry['entries']}),len(registry['entries']))
        for entry in registry['entries']:
            if entry['source']:self.assertTrue((ROOT/entry['source']).is_file(),entry['source'])
        plan=bench.plan(Path('/tmp/render-plan-fixture'))
        self.assertEqual(len(plan['entries']),22)
        self.assertEqual(len({x['name'] for x in plan['entries']}),22)
        self.assertTrue(all(x['captures'] in (1,3) for x in plan['entries']))

    def test_real_core_example_and_rejected_captures(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);build=folder/'build'
            subprocess.run(['cmake','-S',str(bench.EXAMPLE),'-B',str(build),
                            '-DRENDER_BENCH_HOST=ON','-DRENDER_BENCH_SUITE=core'],
                           check=True,stdout=subprocess.DEVNULL)
            subprocess.run(['cmake','--build',str(build),'-j','2'],check=True,stdout=subprocess.DEVNULL)
            logs=[]
            for i in range(3):
                path=folder/f'{i}.log';path.write_bytes(subprocess.check_output([str(build/'render_benchmark')]))
                logs.append(path)
            report=bench.summarize(logs)
            self.assertEqual(len(report['cases']),12)
            self.assertIsNone(report['total_score'])
            self.assertEqual(report['full_game_acceptance'],'pending')
            text=logs[0].read_text()
            for malformed in (text.replace('RENDERBENCH_END','INCOMPLETE'),text+text,
                              text.replace('"errors":0','"errors":1',1),
                              text.replace('"status":0','"status":1'),
                              text.replace('"max_error":0.','"max_error":9.',1)):
                with self.assertRaises(ValueError):bench.read_log(malformed)
            with self.assertRaises(ValueError):bench.summarize([logs[0]]*3)
            changed=copy.deepcopy(report);changed['config']['workload_sha256']='f'*64
            with self.assertRaises(ValueError):bench.compare(report,changed,'pie')
            changed=copy.deepcopy(report);changed['config']['pie']=1
            with self.assertRaises(ValueError):bench.compare(report,changed,'implementation')
            self.assertEqual(len(bench.compare(report,changed,'pie')),12)


if __name__=='__main__':unittest.main()
