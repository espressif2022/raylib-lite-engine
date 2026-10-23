# SPDX-License-Identifier: Apache-2.0
import json
from pathlib import Path
import tempfile
import unittest
from tests.test_font import fixture_font
from tools.pack_game_assets import compile_assets

class FontManifestTests(unittest.TestCase):
    def test_manifest_names_bytes_coverage_and_raw_native_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder); source=root/'src'; source.mkdir(); output=root/'out'
            fixture_font(source/'original.ttf')
            (source/'ui.txt').write_text('中文 A',encoding='utf-8')
            (source/'native.fnt').write_text('info face="test"')
            (source/'glyph.png').write_bytes(b'png fixture')
            manifest={'schema':'mosaico-game-assets/v1','fonts':[{
                'source':'original.ttf','output':'ui.20.font','size':20,
                'text_files':['ui.txt'],'license':'Original test fixture Apache-2.0'}],
                'files':[{'source':'native.fnt'},{'source':'glyph.png'}]}
            path=source/'game_assets.json';path.write_text(json.dumps(manifest))
            report=compile_assets(source,output,path)
            self.assertEqual({entry['file'] for entry in report['files']},
                             {'ui.20.fnt','ui.20.atlas','native.fnt','glyph.png'})
            self.assertEqual(report['total_bytes'],sum((output/e['file']).stat().st_size for e in report['files']))
            self.assertEqual(json.loads((output/'ui.20.font.json').read_text())['missing'],[])
            self.assertIn('0x', (output/'assets_ids.h').read_text())
            # Preview is not embedded as an undeclared runtime file.
            self.assertNotIn('ui.20.png',{e['file'] for e in report['files']})
            digest=(output/'assets.sha256').read_text()
            compile_assets(source,output,path)
            self.assertEqual(digest,(output/'assets.sha256').read_text())
            manifest['fonts'].append(manifest['fonts'][0]);path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,'already exists'):
                compile_assets(source,output,path)
            manifest['fonts']=[{**manifest['fonts'][0],'native_png':True}]
            path.write_text(json.dumps(manifest))
            native_report=compile_assets(source,output,path)
            self.assertIn('ui.20.png',{e['file'] for e in native_report['files']})

    def test_missing_glyph_is_a_build_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);fixture_font(root/'test.ttf')
            (root/'ui.txt').write_text('不存在',encoding='utf-8')
            manifest={'schema':'mosaico-game-assets/v1','fonts':[{'source':'test.ttf',
                'output':'ui.font','text_files':['ui.txt'],'license':'test'}]}
            path=root/'manifest.json';path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,'missing glyphs'):
                compile_assets(root,root/'out',path)

    def test_removing_font_drops_report_and_build_history(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / 'src'
            source.mkdir()
            fixture_font(source / 'original.ttf')
            manifest = {'schema': 'mosaico-game-assets/v1', 'fonts': [{
                'source': 'original.ttf', 'output': 'ui.font',
                'license': 'Original test fixture Apache-2.0'}]}
            path = source / 'manifest.json'
            path.write_text(json.dumps(manifest))
            reused = root / 'reused'
            compile_assets(source, reused, path)
            self.assertTrue((reused / 'ui.font.json').is_file())
            manifest['fonts'] = []
            path.write_text(json.dumps(manifest))
            compile_assets(source, reused, path)
            self.assertFalse((reused / 'ui.font.json').exists())
            clean = root / 'clean'
            compile_assets(source, clean, path)
            self.assertEqual((reused / 'assets.sha256').read_text(),
                             (clean / 'assets.sha256').read_text())
