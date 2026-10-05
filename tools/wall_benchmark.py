#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build/audit/time actual wall raster variants; never infer device FPS from host."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VARIANTS = {
    'legacy': (0, 16, .25), 'exact': (1, 16, .25),
    'fixed4': (2, 4, .25), 'fixed8': (2, 8, .25),
    'fixed16': (2, 16, .25), 'fixed32': (2, 32, .25),
    'adaptive0125': (3, 16, .125), 'adaptive025': (3, 16, .25),
    'adaptive05': (3, 16, .5),
}
CASES = ('front', 'shallow', 'oblique', 'near', 'reverse', 'pitch', 'clip',
         'triangles', 'affine_compat')
POLICY = {'version': 1, 'uv_limit_texels': .26, 'exact_uv_limit_texels': .01,
          'max_texel_error': 1, 'coverage_errors': 0,
          'weights': {'correctness': 30, 'quality': 30, 'speed': 30, 'repeatability': 10},
          'max_timing_relative_range': .20,
          'scope': 'synthetic INDEX8 raster only; no game/display/near-clip acceptance'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def percentile(values, p):
    if not values or any(not math.isfinite(x) or x <= 0 for x in values):
        raise ValueError('timings must be finite and positive')
    return sorted(values)[max(0, math.ceil(len(values)*p)-1)]


def parse(text):
    rows = []
    for line in text.splitlines():
        if 'WALLBENCH {' in line:
            rows.append(json.loads(line[line.index('WALLBENCH ')+10:]))
    if [r['case'] for r in rows] != list(CASES):
        raise ValueError('missing, reordered, or duplicate benchmark cases')
    for row in rows:
        for key in ('uv_max', 'uv_sum', 'pixels', 'uv_samples', 'coverage_errors',
                    'texel_errors', 'max_texel_error', 'segments', 'uv_reciprocals'):
            if not math.isfinite(row[key]) or row[key] < 0:
                raise ValueError(f'invalid {key}')
        if row['pixels'] <= 0 or row['texel_errors'] > row['pixels'] or len(row['times_us']) != 7:
            raise ValueError('empty coverage or incomplete samples')
        percentile(row['times_us'], .95)
    if len({(r['mode'], r['fixed'], r['bound']) for r in rows}) != 1:
        raise ValueError('mixed renderer configurations')
    return rows


def validate_variant(rows, label):
    expected = VARIANTS[label]
    if any((r['mode'], r['fixed'], r['bound']) != expected for r in rows):
        raise ValueError(f'{label}: unexpected compiled macros')


def quality(rows, exact=False):
    limit = POLICY['exact_uv_limit_texels'] if exact else POLICY['uv_limit_texels']
    failures = []
    for row in rows:
        # Every actual sample must have been observed, not just selected pixels.
        if row['uv_samples'] != row['pixels']:
            failures.append(f"{row['case']}: incomplete UV audit")
        if row['coverage_errors'] or row['max_texel_error'] > POLICY['max_texel_error'] or row['uv_max'] > limit:
            failures.append(f"{row['case']}: coverage/texel/UV gate")
    return failures


def summarize(audits, timings):
    base = {case: percentile(timings['legacy'][case], .95) for case in CASES}
    stability_limit = POLICY['max_timing_relative_range']
    baseline_stable = all((max(v)-min(v))/statistics.median(v) <= stability_limit
                          for v in timings['legacy'].values())
    result = {}
    for label, audit in audits.items():
        failures = quality(audit, exact=label == 'exact')
        case_metrics = {}
        ratios, spreads = [], []
        for row in audit:
            case = row['case']; values = timings[label][case]
            med = statistics.median(values); p95 = percentile(values, .95)
            spread = (max(values)-min(values))/med
            ratios.append(base[case]/p95); spreads.append(spread)
            case_metrics[case] = dict(row, times_us=values, p50_us=med, p95_us=p95,
                                     max_us=max(values), relative_range=spread)
        # Equal case weighting avoids large scenes hiding a small pathological case.
        speed = math.exp(sum(math.log(v) for v in ratios)/len(ratios))
        max_uv = max(r['uv_max'] for r in audit)
        parts = {'correctness': 30., 'quality': 30*max(0, 1-max_uv),
                 'speed': 30*min(1, speed),
                 'repeatability': 10*max(0, 1-max(spreads)/.20)}
        stable = max(spreads) <= stability_limit
        accepted = not failures and stable and baseline_stable
        result[label] = {'accepted': accepted, 'quality_passed': not failures, 'failures': failures,
                         'baseline_timing_stable': baseline_stable,
                         'host_micro_score': round(sum(parts.values()), 2) if accepted else None,
                         'score_parts': parts if not failures else None,
                         'p95_speed_ratio_to_legacy': speed,
                         'timing_stable': stable,
                         'cases': case_metrics}
    return result


def check_comparable(old, new):
    for key in ('schema', 'policy', 'workload_sha256', 'environment', 'protocol'):
        if old[key] != new[key]:
            raise ValueError(f'incomparable reports: {key} differs')
    if old['results'].keys() != new['results'].keys():
        raise ValueError('incomparable variant matrix')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--cpu', type=int, help='pin host process and children; default first allowed CPU')
    args = parser.parse_args()
    if args.rounds < 3:
        parser.error('at least 3 interleaved rounds required')
    run_lock = None
    if hasattr(os, 'sched_getaffinity'):
        allowed = os.sched_getaffinity(0)
        cpu = min(allowed) if args.cpu is None else args.cpu
        if cpu not in allowed: parser.error('CPU is outside allowed affinity')
        os.sched_setaffinity(0, {cpu})
        import fcntl
        run_lock = open(Path(tempfile.gettempdir())/f'raylib-bench-{os.getuid()}-{cpu}.lock', 'a')
        fcntl.flock(run_lock, fcntl.LOCK_EX)
    compiler = shutil.which(args.cc)
    if not compiler:
        parser.error('C compiler not found')
    previous = json.loads(args.baseline.read_text()) if args.baseline else None
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=False)
    sources = [ROOT/'components/mosaico_game_2d'/name for name in
               ('mosaico_wall_bench.c', 'mosaico_game_2d.c', 'mosaico_game_2d_raylib.c', 'mosaico_rgb565.c')]
    sources += [ROOT/'host/host_asset_runtime.c']
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-DWALL_BENCH_HOST=1']
    for folder in ('host/include', 'host', 'components/mosaico_game_assets/include',
                   'components/mosaico_game_2d/include'):
        flags += ['-I', str(ROOT/folder)]
    environment = {'platform': platform.platform(), 'machine': platform.machine(),
                   'compiler': subprocess.check_output([compiler, '--version'], text=True),
                   'compiler_sha256': digest(Path(compiler).read_bytes()),
                   'cpu': Path('/proc/cpuinfo').read_text().split('\n\n')[0].split('flags')[0]
                   if Path('/proc/cpuinfo').exists() else platform.processor(),
                   'affinity': sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None,
                   'flags': flags}
    # Dynamic cpu MHz is not an identity and must not invalidate cross-run comparisons.
    environment['cpu'] = '\n'.join(line for line in environment['cpu'].splitlines()
                                    if not line.startswith('cpu MHz'))
    inputs = set(sources)
    dependencies = subprocess.check_output([compiler, *flags, '-MM', *map(str, sources)], text=True)
    for line in dependencies.replace('\\\n', ' ').splitlines():
        inputs.update(Path(x).resolve() for x in shlex.split(line.split(':', 1)[1]))
    inputs.add(Path(__file__).resolve())
    manifest = {}
    for source in sorted(inputs):
        name = str(source.relative_to(ROOT)); data = source.read_bytes()
        manifest[name] = digest(data)
        dest = out/'source'/name; dest.parent.mkdir(parents=True, exist_ok=True);dest.write_bytes(data)
    (out/'source-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (out/'working-tree.patch').write_bytes(subprocess.check_output(['git', 'diff', '--binary'], cwd=ROOT))
    report = {'schema': 'wall-benchmark/v1', 'policy': POLICY, 'environment': environment,
              'protocol': {'rounds': args.rounds, 'warmup_replays': 2, 'frames': 8,
                           'samples_per_round': 7, 'replays_per_sample': 4,
                           'clock': 'monotonic', 'timing': 'microseconds/draw; batched, no clear/oracle/display'},
              'workload_sha256': manifest['components/mosaico_game_2d/mosaico_wall_bench.c'],
              'git_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
              'source_manifest_sha256': digest(json.dumps(manifest, sort_keys=True).encode()),
              'device_score': None, 'full_game_acceptance': 'pending', 'binaries': {}, 'commands': []}
    if previous is not None:
        probe = dict(report, results={label: {} for label in VARIANTS})
        check_comparable(previous, probe)
    audits, timings = {}, {}
    for label, (mode, fixed, error) in VARIANTS.items():
        defs = [f'-DM2D_WALL_MODE={mode}', f'-DM2D_WALL_FIXED_PIXELS={fixed}',
                f'-DM2D_WALL_ERROR_TEXELS={error}f']
        timings[label] = {case: [] for case in CASES}
        for audit in (True, False):
            binary = out/(label+('-audit' if audit else '-timing'))
            command = [compiler, *flags, *defs, *(['-DM2D_WALL_AUDIT=1'] if audit else []),
                       *map(str, sources), '-lm', '-o', str(binary)]
            report['commands'].append(command);subprocess.run(command, check=True)
            report['binaries'][binary.name] = digest(binary.read_bytes())
            if audit:
                log = subprocess.check_output([str(binary)], text=True)
                (out/(binary.name+'.log')).write_text(log);audits[label] = parse(log)
                validate_variant(audits[label], label)
        print(f'compiled and audited {label}', flush=True)
    for repeat in range(args.rounds):
        labels = list(VARIANTS)
        # Alternate order each round; retain exact order in report.
        if repeat % 2: labels.reverse()
        for label in labels:
            log = subprocess.check_output([str(out/(label+'-timing'))], text=True)
            (out/f'{label}-timing-{repeat}.log').write_text(log)
            rows = parse(log)
            validate_variant(rows, label)
            for row, audit in zip(rows, audits[label]):
                if row['uv_samples'] != 0:
                    raise ValueError('audit instrumentation in timing build')
                if row['hash'] != audit['hash']:
                    raise ValueError('audit/timing or repeat framebuffer mismatch')
                timings[label][row['case']].extend(row['times_us'])
        print(f'timing round {repeat+1}/{args.rounds}', flush=True)
    report['results'] = summarize(audits, timings)
    if args.baseline:
        check_comparable(previous, report)
        report['comparison'] = {}
        for label in VARIANTS:
            a=previous['results'][label];b=report['results'][label]
            ratios=[a['cases'][c]['p95_us']/b['cases'][c]['p95_us'] for c in CASES]
            # Only claim improvement if all per-case sample ranges separate.
            improved = a['accepted'] and b['accepted'] and all(
                max(b['cases'][c]['times_us']) < min(a['cases'][c]['times_us']) for c in CASES)
            report['comparison'][label] = {'p95_ratios': dict(zip(CASES, ratios)),
                                            'improvement_supported': improved}
    (out/'report.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    lines=['# Wall raster benchmark', '', 'Host microbenchmark only. Device score: pending.', '',
           '| Variant | Quality gate | Host score /100 | P95 ratio vs legacy | Timing stable |',
           '|---|---|---:|---:|---|']
    for label, value in report['results'].items():
        lines.append(f"| {label} | {'PASS' if value['quality_passed'] else 'FAIL'} | {value['host_micro_score']} | {value['p95_speed_ratio_to_legacy']:.3f} | {value['timing_stable']} |")
    lines += ['', 'Failed quality gates cannot be offset by speed; unstable timing receives no total score. P95 is over batch averages, not individual frame latency.',
              'Raw logs, build commands, binary/source hashes and source snapshots are alongside report.json.']
    (out/'report.md').write_text('\n'.join(lines)+'\n')
    print('\n'.join(lines))
    # Legacy/fixed/loose candidates may legitimately fail. Required references must pass.
    if run_lock is not None: run_lock.close()
    return 0 if all(report['results'][label]['quality_passed'] for label in
                    ('exact', 'adaptive0125', 'adaptive025')) else 1


if __name__ == '__main__':
    sys.exit(main())
