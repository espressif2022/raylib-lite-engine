#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Lexical public entrypoint inventory and reviewed module contracts.

Not a C preprocessor or linker: conditional branches and address references
are retained. Declarations/definitions are separate from lexical uses.
"""
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CATALOG = 'compat/api/catalog.json'


def mask_source(source):
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]),
                  source, flags=re.DOTALL)


def entrypoints(source):
    """Return public named functions/macros, with signature offsets and lines.

    Callback typedefs and calls inside inline bodies are not entrypoints.
    Balanced parameters allow callbacks and multiline declarations.
    """
    text = mask_source(source)
    result = []
    for match in re.finditer(r'^[ \t]*#[ \t]*define[ \t]+(\w+)((?:[^\n]*\\\n)*[^\n]*)', text, re.MULTILINE):
        name, tail = match.groups()
        target = re.search(r'\braylib_lite_\w+\b', tail)
        if name.startswith('raylib_lite_') or tail.startswith('(') or (name.startswith('RAYLIB_LITE_') and target) or (re.match(r'[A-Z][a-z]\w*$', name) and target):
            result.append({'name': name, 'kind': 'macro',
                           'line': text.count('\n', 0, match.start(1))+1,
                           'start': match.start(1), 'end': match.end(),
                           'target': target[0] if target else None})
    # Erase directives so continuation tokens cannot masquerade as declarations.
    code = re.sub(r'^[ \t]*#(?:[^\n]*\\\n)*[^\n]*',
                  lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text, flags=re.MULTILINE)
    pattern = re.compile(r'(?:^|(?<=[;{}]))[ \t]*(?P<type>(?:(?:[A-Za-z_]\w*\b[ \t\n]+)|(?:\*[ \t\n]*))+?)\b(?P<name>raylib_lite_\w+)[ \t\n]*\(', re.MULTILINE)
    bodies = []
    for match in pattern.finditer(code):
        if any(start <= match.start() < end for start, end in bodies):
            continue
        words = re.findall(r'\w+', match['type'])
        if not words or any(w in {'return', 'typedef', 'if', 'else', 'case', 'goto'} for w in words):
            continue
        pos, depth = match.end(), 1
        while pos < len(code) and depth:
            depth += (code[pos] == '(') - (code[pos] == ')')
            pos += 1
        tail = pos
        while tail < len(code) and code[tail].isspace(): tail += 1
        if tail == len(code) or code[tail] not in ';{':
            continue
        kind = 'inline' if code[tail] == '{' and 'inline' in words else ('definition' if code[tail] == '{' else 'function')
        if code[tail] == '{':
            end, nesting = tail+1, 1
            while end < len(code) and nesting:
                nesting += (code[end] == '{') - (code[end] == '}')
                end += 1
            bodies.append((tail, end))
        result.append({'name': match['name'], 'kind': kind,
                       'line': code.count('\n', 0, match.start('name'))+1,
                       'start': match.start('name'), 'end': pos})
    return sorted(result, key=lambda x: (x['line'], x['name']))


def public_inventory(root=ROOT):
    modules = {}
    for tree in ('include/raylib_lite', 'compat/raylib/include'):
        for path in sorted((root/tree).rglob('*.h')):
            header = str(path.relative_to(root))
            parent = path.relative_to(root/tree).parent
            module = (str(parent).replace('/', '.')+'.' if str(parent) != '.' else '') + path.stem.removeprefix('raylib_lite_')
            if tree.startswith('compat/'): module = 'compat.'+module
            modules[module] = {'header': header, 'entries': entrypoints(path.read_text())}
    return modules


def usage_inventory(root=ROOT):
    # Import lazily to preserve the standalone release validator.
    try:
        from .audit_raylib import release_games
    except ImportError:
        from audit_raylib import release_games
    inventory = public_inventory(root)
    symbols = {e['name'] for m in inventory.values() for e in m['entries']}
    result = {name: {'declarations': [], 'definitions': [], 'references': {
        'games': {}, 'shared': [], 'tests': [], 'benchmarks': []}} for name in sorted(symbols)}
    game_names = set(release_games(root))
    paths = set()
    for tree in ('src', 'include/raylib_lite', 'compat/raylib/include', 'host', 'tests', 'examples'):
        for path in (root/tree).rglob('*'):
            if path.suffix in ('.c', '.h') and not any(
                part in {'managed_components', '.git'} or part.startswith('build') for part in path.relative_to(root).parts):
                paths.add(path)
    for path in sorted(paths):
        rel = path.relative_to(root);parts = rel.parts
        # Other games/examples are intentionally outside the maintained scope.
        if parts[0] == 'examples':
            if len(parts) < 2: continue
            if 'benchmark' in parts[1]: category, game = 'benchmarks', None
            elif parts[1] in game_names:
                category, game = ('tests', None) if 'tests' in parts else ('games', parts[1])
            elif parts[1] in {'boards', 'common_components'}: category, game = 'shared', None
            else: continue
        else:
            if parts[0] == 'tests': category, game = 'tests', None
            elif 'bench' in path.stem: category, game = 'benchmarks', None
            else: category, game = 'shared', None
        source = path.read_text();masked = mask_source(source)
        entries = entrypoints(source)
        signatures = [(e['start'], e['end']) for e in entries]
        for e in entries:
            if e['name'] not in result: continue
            key = 'definitions' if e['kind'] in ('inline', 'definition', 'macro') else 'declarations'
            result[e['name']][key].append(f'{rel}:{e["line"]}')
        for match in re.finditer(r'\b[A-Za-z_]\w*\b', masked):
            name = match[0]
            if name not in result or any(a <= match.start() < b for a,b in signatures): continue
            site = f'{rel}:{masked.count(chr(10), 0, match.start())+1}'
            bucket = result[name]['references'][category]
            if game is not None: bucket = bucket.setdefault(game, [])
            if site not in bucket: bucket.append(site)
    for module in inventory.values():
        for entry in module['entries']:
            target = entry.get('target')
            if target in result and entry['name'] != target:
                result[target].setdefault('via_aliases', {})[entry['name']] = result[entry['name']]['references']
    return inventory, result


def check_catalog(root=ROOT, repository_evidence=True):
    catalog = json.loads((root/CATALOG).read_text())
    if catalog.get('schema') != 'raylib-lite-api-catalog-1': raise ValueError('Unsupported API catalog schema')
    actual = public_inventory(root);modules = catalog['modules']
    checkout_only = catalog['checkout_only_evidence']
    expected_checkout = sorted({f for m in modules.values() for f in m['implementation']
                                if Path(f).parts[0] not in ('src', 'include', 'compat')})
    if checkout_only != expected_checkout: raise ValueError('Checkout evidence inventory differs')
    if set(actual) != set(modules): raise ValueError('Public module/header inventory changed')
    for name, current in actual.items():
        record = modules[name]
        entries = sorted({e['name'] for e in current['entries']})
        roles = {'game', 'integration', 'low_level', 'compatibility', 'review'}
        if record.get('usage_role') not in roles: raise ValueError(f'Missing/invalid usage role: {name}')
        overrides = record.get('entry_roles', {})
        if not isinstance(overrides, dict) or not set(overrides) <= set(entries) or any(r not in roles for r in overrides.values()):
            raise ValueError(f'Invalid entry roles: {name}')
        if record['header'] != current['header'] or record['entries'] != entries:
            raise ValueError(f'Unregistered or removed public entrypoint: {name}')
        for key in ('ownership', 'failure', 'cleanup', 'boundary', 'test_gaps'):
            if not isinstance(record.get(key), str) or not record[key].strip():
                raise ValueError(f'Missing module contract {name}: {key}')
        for key in ('implementation', 'tests'):
            if not isinstance(record.get(key), list) or not record[key]: raise ValueError(f'Missing evidence {name}: {key}')
            for file in record[key]:
                if not isinstance(file, str) or not file: raise ValueError(f'Invalid evidence path: {file}')
                if Path(file).is_absolute() or '..' in Path(file).parts: raise ValueError(f'Unsafe evidence path: {file}')
                if (repository_evidence or (key != 'tests' and file not in checkout_only)) and not (root/file).is_file():
                    raise ValueError(f'Missing evidence file: {file}')
    mapping_entries = {e['name'] for name, m in actual.items()
                       if name in ('compat.raylib', 'compat.raylib_audio')
                       for e in m['entries'] if e['kind']=='macro' and e.get('target')
                       and re.match(r'[A-Z][a-z]', e['name'])}
    if set(catalog['compatibility']) != mapping_entries: raise ValueError('Compatibility status inventory differs')
    for name, state in catalog['compatibility'].items():
        if state not in ('契约', '等价', '差异'): raise ValueError(f'Invalid compatibility state: {name}')
    document = root/'docs/raylib-compat.CN.md'
    documented = {}
    for line in document.read_text().splitlines():
        cells = line.split('|')
        if len(cells) > 3 and cells[2].strip() in ('契约', '等价', '差异'):
            for name in re.findall(r'`(\w+)`', cells[1]): documented[name] = cells[2].strip()
    if documented != catalog['compatibility']: raise ValueError('Compatibility states differ from documentation')
    all_entries = {e['name'] for m in actual.values() for e in m['entries']}
    for entry in catalog['retirement_review']:
        if entry.get('status') not in ('待审查', '候选'): raise ValueError('Invalid retirement status')
        names = entry.get('entries', [])
        if not names or not set(names) <= all_entries: raise ValueError('Unknown retirement entrypoint')
        for key in ('reason', 'replacement', 'migration', 'validation', 'external_usage'):
            if not isinstance(entry.get(key), str) or not entry[key].strip(): raise ValueError(f'Missing retirement evidence: {key}')
        if entry['status'] == '候选':
            if entry['external_usage'] != '已核实':
                raise ValueError('Unknown external consumers require review, not retirement candidacy')
            evidence = entry.get('external_evidence', [])
            if not isinstance(evidence, list) or not evidence:
                raise ValueError('Retirement candidacy requires external evidence files')
            for file in evidence:
                if not isinstance(file, str) or not file or Path(file).is_absolute() or '..' in Path(file).parts:
                    raise ValueError('Invalid external evidence path')
                if not (root/file).is_file(): raise ValueError(f'Missing external evidence: {file}')
    if repository_evidence:
        _, uses = usage_inventory(root)
        for entry in catalog['retirement_review']:
            if entry['status']=='候选' and any(has_consumers(uses[n]) for n in entry['entries']):
                raise ValueError('Retirement candidate has maintained consumers')
    return catalog


def has_consumers(symbol):
    refs = symbol['references']
    return bool(refs['games'] or refs['shared'] or any(
        a['games'] or a['shared'] for a in symbol.get('via_aliases', {}).values()))


def interface_report(root=ROOT):
    catalog = check_catalog(root)
    modules, uses = usage_inventory(root)
    for name, module in modules.items():
        entries = sorted({e['name'] for e in module['entries']})
        for symbol in entries:
            uses[symbol].setdefault('module_roles', {})[name] = catalog['modules'][name]['entry_roles'].get(symbol, catalog['modules'][name]['usage_role'])
            consumed = has_consumers(uses[symbol])
            reviewing = any(symbol in row['entries'] for row in catalog['retirement_review'])
            uses[symbol]['consumer_state'] = '有消费者' if consumed else ('待审查' if reviewing else '未发现消费者')
        module['contract'] = catalog['modules'][name]
        module['consumers'] = sorted({g for s in entries for g in uses[s]['references']['games']})
        module['shared_entry_count'] = sum(bool(uses[s]['references']['shared']) for s in entries)
        module['test_entry_count'] = sum(bool(uses[s]['references']['tests']) for s in entries)
        module['benchmark_entry_count'] = sum(bool(uses[s]['references']['benchmarks']) for s in entries)
    return {'modules': modules, 'symbols': uses, 'compatibility': catalog['compatibility'],
            'retirement_review': catalog['retirement_review'],
            'scope': 'Lexical C/H uses across maintained games, shared engine/Host/board adapters, tests and benchmark examples. Declarations and definitions excluded from uses. Conditional branches and address references retained; no preprocessing, external-consumer discovery, runtime frequency, link reachability or FPS claim.'}
