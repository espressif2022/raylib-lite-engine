#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Dedicated example: matrix planning, repeatable host runs, board log collection."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import tempfile
from wall_benchmark import CASES, VARIANTS, parse as parse_wall, quality, percentile

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = ROOT/'examples/render_benchmark'
CORE_CASES = ('copy_rgb565','fill_rgb565','shade_rgb565','columns_rgb565',
              'columns_index8_row','columns_index8_column','span_rgb565',
              'quad_rgb565','quad_index8','span_mtx2','sin_direct','sin_recurrence')


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_log(text):
    records = {key: [] for key in ('RENDERBENCH_BEGIN','RENDERBENCH_DEVICE','RENDERBENCH_END','COREBENCH')}
    if any(x in text for x in ("Guru Meditation", "panic'ed", 'abort() was called')):
        raise ValueError('device failure in capture')
    for line in text.splitlines():
        for prefix in records:
            marker=prefix+' {'
            if marker in line:
                records[prefix].append(json.loads(line[line.index(marker)+len(prefix)+1:]))
    if len(records['RENDERBENCH_BEGIN']) != 1 or len(records['RENDERBENCH_END']) != 1:
        raise ValueError('exactly one complete boot/run required per log')
    begin=records['RENDERBENCH_BEGIN'][0];end=records['RENDERBENCH_END'][0]
    if begin['schema']!='render-example/v1' or end['suite']!=begin['suite'] or end['status']!=0:
        raise ValueError('failed or incompatible run')
    if begin['display_active'] is not False:
        raise ValueError('display must be inactive for isolated raster benchmark')
    if len(begin['workload_sha256'])!=64:
        raise ValueError('missing workload identity')
    devices=records['RENDERBENCH_DEVICE']
    if len(devices)>1:raise ValueError('mixed device identities')
    device=devices[0] if devices else None
    if device:
        for key in ('chip','revision','cpu_mhz','psram_mhz','core','idf','app_sha256'):
            if key not in device:raise ValueError('incomplete device metadata')
    if begin['suite']=='wall':
        rows=parse_wall(text)
        if any((r['mode'],r['fixed'],r['bound'])!=(begin['mode'],begin['fixed'],begin['bound']) for r in rows):
            raise ValueError('compiled mode differs from envelope')
    elif begin['suite']=='core':
        rows=records['COREBENCH']
        if [r['case'] for r in rows]!=list(CORE_CASES):raise ValueError('incomplete core matrix')
        for row in rows:
            if row['case'].startswith('sin_'):
                error=row['max_error']
                if row['points']!=256 or not math.isfinite(error) or error<0 or error>1e-4:
                    raise ValueError('math oracle failed')
            elif row['pixels']!=4096 or row['errors']!=0:
                raise ValueError('pixel oracle failed')
            if len(row['times_us'])!=7:raise ValueError('incomplete timing batch')
            percentile(row['times_us'],.95)
    else:raise ValueError('unknown suite')
    return begin,device,end,rows


def summarize(logs,audit_log=None):
    if len(logs)<3 or len(set(map(str,logs)))!=len(logs):
        raise ValueError('three distinct capture paths required')
    parsed=[read_log(Path(p).read_text()) for p in logs]
    begin,device,_,first=parsed[0]
    if begin['audit']:raise ValueError('audit timings must never be ranked')
    def identity(value):
        if value is None:return None
        return {k:v for k,v in value.items() if k!='psram_free_before'}
    for config,hw,_,rows in parsed:
        if config!=begin or identity(hw)!=identity(device):raise ValueError('mixed configs or firmware across rounds')
        for a,b in zip(rows,first):
            if a.get('hash')!=b.get('hash'):raise ValueError('non-deterministic output')
    reference=None
    if begin['suite']=='wall':
        if audit_log is None:raise ValueError('wall timing needs matching audit log')
        ab,ad,_,reference=read_log(Path(audit_log).read_text())
        if not ab['audit'] or {k:v for k,v in ab.items() if k!='audit'}!={k:v for k,v in begin.items() if k!='audit'}:
            raise ValueError('audit config mismatch')
        if bool(ad)!=bool(device):raise ValueError('host/device audit mismatch')
        if ad:
            for key in ('chip','revision','cpu_mhz','psram_mhz','core','idf'):
                if ad[key]!=device[key]:raise ValueError('audit hardware mismatch')
        if quality(reference,exact=begin['mode']==1):raise ValueError('wall quality gate failed')
        for _,_,_,rows in parsed:
            for timing,audit in zip(rows,reference):
                if timing['uv_samples']!=0 or timing['hash']!=audit['hash']:
                    raise ValueError('audit/timing mismatch')
    metrics={}
    for i,row in enumerate(first):
        values=[v for _,_,_,rows in parsed for v in rows[i]['times_us']]
        median=statistics.median(values);spread=(max(values)-min(values))/median
        metrics[row['case']]={**row,'times_us':values,'p50_us':median,
             'p95_us':percentile(values,.95),'max_us':max(values),
             'relative_range':spread,'stable':spread<=.20}
    paths=list(logs)+([audit_log] if audit_log else [])
    return {'schema':'render-example-report/v1','config':begin,'device':device,
            'scope':'offscreen kernel microbenchmark; no display FPS or game acceptance',
            'rounds':len(logs),'measurement_accepted':all(v['stable'] for v in metrics.values()),
            'total_score':None,'full_game_acceptance':'pending','cases':metrics,
            'audit':reference,'memory_observations':[end for _,_,end,_ in parsed],
            'log_sha256':{str(p):sha(p) for p in paths}}


def compare(base,new,dimension):
    if base['schema']!=new['schema']:raise ValueError('report schema mismatch')
    excluded={'perspective':{'mode','fixed','bound'},'lut':{'lut_storage'},'pie':{'pie'},'implementation':set()}[dimension]
    a={k:v for k,v in base['config'].items() if k not in excluded}
    b={k:v for k,v in new['config'].items() if k not in excluded}
    if a!=b or base['cases'].keys()!=new['cases'].keys() or base['rounds']!=new['rounds']:
        raise ValueError('workload/config/protocol mismatch outside selected dimension')
    for key in ('chip','revision','cpu_mhz','psram_mhz','core','idf'):
        if (base['device'] or {}).get(key)!=(new['device'] or {}).get(key):raise ValueError('device mismatch')
    if not new['device'] and base.get('host_environment')!=new.get('host_environment'):
        raise ValueError('host environment mismatch')
    result={}
    for case in new['cases']:
        old=base['cases'][case];cur=new['cases'][case]
        result[case]={'p95_speed_ratio':old['p95_us']/cur['p95_us'],
          'improvement_supported':base['measurement_accepted'] and new['measurement_accepted']
            and max(cur['times_us'])<min(old['times_us'])}
    return result


def defs(args,audit=False):
    mode,fixed,bound=VARIANTS[args.variant]
    return [f'-DRENDER_BENCH_SUITE={args.suite}',f'-DM2D_WALL_MODE={mode}',
        f'-DM2D_WALL_FIXED_PIXELS={fixed}',f'-DM2D_WALL_ERROR_TEXELS={bound}',
        f'-DM2D_WALL_AUDIT={"ON" if audit else "OFF"}']


def plan(output):
    entries=[]
    for label,(mode,fixed,bound) in VARIANTS.items():
        for audit in (True,False):
            name=label+('-audit' if audit else '-timing');build=output/name
            command=['idf.py','-C',str(EXAMPLE),'-B',str(build),'-DIDF_TARGET=esp32s31',
                     '-DRENDER_BENCH_DISPLAY=OFF','-DRENDER_BENCH_SUITE=wall',f'-DM2D_WALL_MODE={mode}',
                     f'-DM2D_WALL_FIXED_PIXELS={fixed}',f'-DM2D_WALL_ERROR_TEXELS={bound}',
                     f'-DM2D_WALL_AUDIT={"ON" if audit else "OFF"}','build']
            entries.append({'name':name,'build':command,'captures':1 if audit else 3,
                'flash_monitor':['idf.py','-C',str(EXAMPLE),'-B',str(build),'-p','PORT','flash','monitor']})
    for name,suite,options in (
        ('core-scalar','core',[]),('core-pie','core',['-DRENDER_BENCH_PIE=ON']),
        ('adaptive025-lut-internal-audit','wall',['-DM2D_WALL_AUDIT=ON','-DRENDER_BENCH_LUT_INTERNAL=ON']),
        ('adaptive025-lut-internal-timing','wall',['-DRENDER_BENCH_LUT_INTERNAL=ON'])):
        build=output/name
        entries.append({'name':name,'build':['idf.py','-C',str(EXAMPLE),'-B',str(build),
              '-DIDF_TARGET=esp32s31','-DRENDER_BENCH_DISPLAY=OFF',f'-DRENDER_BENCH_SUITE={suite}',*options,'build'],
              'captures':1 if name.endswith('-audit') else 3,
              'flash_monitor':['idf.py','-C',str(EXAMPLE),'-B',str(build),'-p','PORT','flash','monitor']})
    return {'schema':'render-device-plan/v1','entries':entries,
      'previews':[{'name':'wall-display','build':['idf.py','-C',str(EXAMPLE),'-B',str(output/'wall-display'),
          '-DIDF_TARGET=esp32s31','-DRENDER_BENCH_DISPLAY=ON','build'],
          'scored':False,'log_prefix':'RENDERPREVIEW','controls':'touch: previous, next, pause, split/full, auto'}],
      'order':'Audit each variant once; timing round 0 forward, round 1 reverse, round 2 forward.',
      'stop_rule':'quality pass -> repeatable device times -> compare one dimension -> replay real game'}


def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    sub.add_parser('list')
    q=sub.add_parser('plan');q.add_argument('--output',type=Path,required=True)
    q=sub.add_parser('host');q.add_argument('--suite',choices=('core','wall'),default='core')
    q.add_argument('--variant',choices=VARIANTS,default='adaptive025')
    q.add_argument('--rounds',type=int,default=3);q.add_argument('--output',type=Path,required=True)
    q=sub.add_parser('collect');q.add_argument('logs',nargs='+',type=Path)
    q.add_argument('--audit-log',type=Path);q.add_argument('--output',type=Path,required=True)
    q.add_argument('--baseline',type=Path);q.add_argument('--dimension',choices=('perspective','lut','pie','implementation'),default='implementation')
    args=p.parse_args()
    if args.action=='list':print((EXAMPLE/'config/techniques.json').read_text());return 0
    if args.action=='plan':
        args.output.mkdir(parents=True,exist_ok=False)
        (args.output/'plan.json').write_text(json.dumps(plan(args.output.resolve()),indent=2)+'\n');return 0
    if args.action=='collect':
        result=summarize(args.logs,args.audit_log)
        if args.baseline:result['comparison']=compare(json.loads(args.baseline.read_text()),result,args.dimension)
        with args.output.open('x') as stream:json.dump(result,stream,indent=2,allow_nan=False);stream.write('\n')
        return 0 if result['measurement_accepted'] else 2
    if args.rounds<3:p.error('at least 3 rounds required')
    run_lock = None
    if hasattr(os,'sched_getaffinity'):
        import fcntl
        cpu=min(os.sched_getaffinity(0));os.sched_setaffinity(0,{cpu})
        run_lock=open(Path(tempfile.gettempdir())/f'raylib-bench-{os.getuid()}-{cpu}.lock','a')
        fcntl.flock(run_lock,fcntl.LOCK_EX)
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    # Archive inputs before compilation and reject concurrent source changes.
    files=set(EXAMPLE.rglob('*'))
    files.update((ROOT/'components/mosaico_game_2d').rglob('*'))
    files.update((ROOT/'components/mosaico_game_assets/include').glob('*.h'))
    files.update((ROOT/'components/raylib_lite_platform/include').glob('*.h'))
    files.add(ROOT/'host/include/raylib.h');files.add(Path(__file__).resolve());files.add(ROOT/'tools/wall_benchmark.py')
    files={x for x in files if x.is_file() and x.suffix in ('.c','.h','.S','.cmake','.txt','.json','.py') and not any(part.startswith('build') or part in ('managed_components','.git','__pycache__') for part in x.relative_to(ROOT).parts)}
    manifest={str(f.relative_to(ROOT)):sha(f) for f in sorted(files)}
    for f in files:
        dst=out/'source'/f.relative_to(ROOT);dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(f.read_bytes())
    commands=[];audit_log=None
    for audit in ([True,False] if args.suite=='wall' else [False]):
        name='audit' if audit else 'timing';build=out/name
        command=['cmake','-S',str(EXAMPLE),'-B',str(build),'-DRENDER_BENCH_HOST=ON',*defs(args,audit)]
        commands.append(command);subprocess.run(command,check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['cmake','--build',str(build),'-j','2'],check=True,stdout=subprocess.DEVNULL)
        if audit:
            audit_log=out/'audit.log';audit_log.write_bytes(subprocess.check_output([str(build/'render_benchmark')]))
    logs=[]
    for repeat in range(args.rounds):
        log=out/f'timing-{repeat}.log';log.write_bytes(subprocess.check_output([str(out/'timing/render_benchmark')]))
        logs.append(log)
    if any(sha(ROOT/name)!=expected for name,expected in manifest.items()):raise ValueError('source changed during build/run')
    result=summarize(logs,audit_log)
    result['source_sha256']=manifest;result['commands']=commands
    result['binary_sha256']=sha(out/'timing/render_benchmark')
    result['host_environment']={'platform':platform.platform(),
        'compiler':subprocess.check_output(['cc','--version'],text=True),
        'affinity':sorted(os.sched_getaffinity(0)) if hasattr(os,'sched_getaffinity') else None}
    (out/'report.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    print(json.dumps({'report':str(out/'report.json'),'measurement_accepted':result['measurement_accepted'],
                     'cases':len(result['cases']),'device':False}))
    if run_lock is not None:run_lock.close()
    return 0 if result['measurement_accepted'] else 2


if __name__=='__main__':raise SystemExit(main())
