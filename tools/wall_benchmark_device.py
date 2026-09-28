#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Import archived device audit/timing logs. Does not flash or infer host results."""
import argparse
import json
import math
from pathlib import Path
from wall_benchmark import (CASES, POLICY, VARIANTS, digest, parse, summarize,
                            validate_variant)


def import_run(manifest_path):
    manifest_path = Path(manifest_path).resolve()
    data = json.loads(manifest_path.read_text())
    if data['schema'] != 'wall-device-input/v1':
        raise ValueError('unsupported device manifest')
    env = data['environment']
    for key in ('board', 'chip', 'cpu_hz', 'psram_hz', 'toolchain', 'idf_commit',
                'framebuffer_storage', 'texture_storage', 'lut_storage',
                'display_active', 'raster_profile', 'power_mode', 'background_tasks'):
        if key not in env or env[key] is None:
            raise ValueError(f'missing environment: {key}')
    if env['display_active'] is not False or env['raster_profile'] is not False:
        raise ValueError('isolated device microbenchmark requires display and raster profile off')
    for key in ('cpu_hz', 'psram_hz'):
        if not isinstance(env[key], (int, float)) or not math.isfinite(env[key]) or env[key] <= 0:
            raise ValueError(f'invalid frequency: {key}')
    archives = {}
    def read(relative):
        path = (manifest_path.parent/relative).resolve()
        raw = path.read_bytes();archives[relative] = digest(raw)
        return raw
    workload_hash = digest(read(data['workload_source']))
    audits, timings, builds = {}, {}, {}
    if set(data['variants']) != set(VARIANTS):
        raise ValueError('complete variant matrix required')
    for label, value in data['variants'].items():
        for kind in ('audit', 'timing'):
            # Keep actual firmware and sdkconfig bytes, not self-reported hashes alone.
            read(value[kind+'_image'])
            config = read(value[kind+'_sdkconfig']).decode()
            if 'CONFIG_MOSAICO_GAME_RASTER_PROFILE=y' in config.splitlines():
                raise ValueError('raster timing profile enabled in sdkconfig')
        if archives[value['audit_image']] == archives[value['timing_image']]:
            raise ValueError('audit and timing firmware must be separate builds')
        builds[label] = {k: v for k, v in value.items() if k.endswith(('image', 'sdkconfig'))}
        audit = parse(read(value['audit_log']).decode());validate_variant(audit, label)
        audits[label] = audit
        if len(value['timing_logs']) < 3 or len(set(value['timing_logs'])) != len(value['timing_logs']):
            raise ValueError('at least 3 distinct timing rounds required')
        timings[label] = {case: [] for case in CASES}
        for log in value['timing_logs']:
            rows = parse(read(log).decode());validate_variant(rows, label)
            for row, reference in zip(rows, audit):
                if row['uv_samples'] != 0:
                    raise ValueError('audit instrumentation enabled in timing firmware')
                if row['hash'] != reference['hash']:
                    raise ValueError('device audit/timing framebuffer mismatch')
                timings[label][row['case']].extend(row['times_us'])
    rounds = {len(value['timing_logs']) for value in data['variants'].values()}
    if len(rounds) != 1:
        raise ValueError('unequal repeat count across variants')
    results = summarize(audits, timings)
    for result in results.values():
        result['device_micro_score'] = result.pop('host_micro_score')
    return {'schema': 'wall-device-report/v1', 'policy': POLICY,
            'environment': env, 'workload_sha256': workload_hash,
            'protocol': {'rounds': rounds.pop(), 'samples_per_round': 7,
                         'frames': 8, 'replays_per_sample': 4, 'warmup_replays': 2},
            'archive_sha256': archives, 'builds': builds, 'results': results,
            'full_game_acceptance': 'pending', 'display_fps': None,
            'note': 'Environment is supplied by operator; logs and firmware are archived, not remotely attested.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = import_run(args.manifest)
    with args.output.open('x') as stream:
        json.dump(result, stream, indent=2, allow_nan=False);stream.write('\n')
    return 0 if all(result['results'][v]['quality_passed'] for v in
                    ('exact', 'adaptive0125', 'adaptive025')) else 1


if __name__ == '__main__':
    raise SystemExit(main())
