#!/usr/bin/env python3
"""Check roadside contact/transparency and the three sky frame contracts."""
import json
from pathlib import Path
import sys
import tempfile
from PIL import Image

project = Path(__file__).resolve().parents[1]
source = project / 'assets_src'
props = json.loads((source / 'rally_atlas.json').read_text())
assert [f['name'] for f in props['frames']] == [
    'roadside_beacon', 'roadside_rock', 'roadside_city', 'roadside_snow']
assert props['alpha_mode'] == 'binary'
sheet = Image.open(source / props['image']).convert('RGBA')
assert sheet.size == (256, 256)
for i, frame in enumerate(props['frames']):
    cell = sheet.crop((i % 2 * 128, i // 2 * 128,
                       i % 2 * 128 + 128, i // 2 * 128 + 128))
    alpha = cell.getchannel('A')
    bounds = alpha.getbbox()
    assert bounds and bounds[3] == 126, (frame['name'], bounds)
    assert alpha.getpixel((0, 0)) == alpha.getpixel((127, 0)) == 0
    assert alpha.getextrema() == (0, 255), frame['name']

sky = json.loads((source / 'track_background.json').read_text())
assert [f['name'] for f in sky['frames']] == [
    'track_neon_sky', 'track_sunset_sky', 'track_polar_sky']
assert sky['alpha_mode'] == 'opaque' and sky['output_cell'] == 224
image = Image.open(source / sky['image'])
assert image.size == (224, 672)
assert len({image.crop((0, i * 224, 224, (i + 1) * 224)).tobytes()
            for i in range(3)}) == 3
manifest = json.loads((source / 'game_assets.json').read_text())
for atlas in manifest['atlases']:
    config = json.loads((source / atlas['config']).read_text())
    assert atlas['source'] == config['image']
    assert (source / atlas['source']).is_file()
sys.path.insert(0, str(project.parents[1] / 'tools'))
from pack_game_assets import compile_assets
with tempfile.TemporaryDirectory(prefix='rally_art_') as directory:
    output = Path(directory)
    report = compile_assets(source, output, source / 'game_assets.json')
    assert report['total_bytes'] <= 700000, report['total_bytes']
    ids = (output / 'assets_ids.h').read_text()
for frame in props['frames'] + sky['frames']:
    assert 'RAYLIB_LITE_ASSET_ID_' + frame['name'].upper() in ids
print('roadside ground contact, transparency, sky names and manifest passed')
