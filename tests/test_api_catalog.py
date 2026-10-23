# SPDX-License-Identifier: Apache-2.0
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from tools.api_catalog import ROOT, CATALOG, entrypoints, public_inventory, usage_inventory, check_catalog
from tools.audit_raylib import check_private_boundaries


class ApiCatalogTests(unittest.TestCase):
    def test_multiline_callbacks_inline_and_callable_macro(self):
        source = '''// void raylib_lite_comment(void);
const char *text="void raylib_lite_string(void);";
typedef void (*raylib_lite_callback_t)(int value);
void
raylib_lite_multiline(
  void (*callback)(int), int value);
static inline int raylib_lite_inline(int x) {
  raylib_lite_multiline(0, x); return raylib_lite_nested(x);
}
#define Entry raylib_lite_multiline
#define RAYLIB_LITE_VALUE() raylib_lite_inline(1)
#define RAYLIB_LITE_ENTRY raylib_lite_multiline
void raylib_lite_last(void); void raylib_lite_same_line(void);
'''
        entries = entrypoints(source)
        kinds = {e['name']: e['kind'] for e in entries}
        self.assertEqual(kinds, {'raylib_lite_multiline':'function', 'raylib_lite_inline':'inline',
                                'Entry':'macro', 'RAYLIB_LITE_VALUE':'macro', 'RAYLIB_LITE_ENTRY':'macro',
                                'raylib_lite_last':'function','raylib_lite_same_line':'function'})
        continued = '#define Entry ' + chr(92) + '\n  raylib_lite_multiline\n'
        self.assertEqual(entrypoints(continued)[0]['target'], 'raylib_lite_multiline')
        self.assertEqual(next(e['line'] for e in entries if e['name']=='raylib_lite_multiline'), 5)

    def test_reference_categories_aliases_and_signatures(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = {
                'tools/prepare_release.py': 'GAMES = ("sample",)\n',
                'include/raylib_lite/api.h': 'void raylib_lite_call(void);\n#define Call raylib_lite_call\n',
                'examples/sample/main/game.c': 'void raylib_lite_call(void);\nCall(); Call();\nvoid *p=raylib_lite_call;\n',
                'src/core.c': 'void raylib_lite_call(void) {}\nvoid helper(void) { raylib_lite_call(); }\n',
                'src/core_bench.c': 'raylib_lite_call();\n',
                'host/runner.c': 'raylib_lite_call();\n',
                'examples/boards/board/adapter.c': 'raylib_lite_call();\n',
                'tests/check.c': 'raylib_lite_call();\n',
                'examples/sample/tests/check.c': 'raylib_lite_call();\n',
                'examples/render_benchmark/main/bench.c': 'raylib_lite_call();\n',
                'examples/unmaintained/main/game.c': 'raylib_lite_call();\n',
                'examples/sample/build/cache.c': 'raylib_lite_call();\n',
                'examples/sample/managed_components/cache.c': 'raylib_lite_call();\n',
            }
            for name, text in files.items():
                p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
            _, uses = usage_inventory(root);call=uses['raylib_lite_call'];refs=call['references']
            self.assertEqual(refs['games']['sample'], ['examples/sample/main/game.c:3'])
            self.assertEqual(uses['Call']['references']['games']['sample'], ['examples/sample/main/game.c:2'])
            self.assertIn('src/core.c:1', call['definitions'])
            self.assertNotIn('src/core.c:1', refs['shared'])
            self.assertEqual(len(refs['shared']),3)
            self.assertEqual(len(refs['tests']),2)
            self.assertEqual(len(refs['benchmarks']),2)
            self.assertEqual(call['via_aliases']['Call']['games']['sample'], ['examples/sample/main/game.c:2'])

    def fixture_catalog(self, root):
        for tree in ['include', 'compat', 'docs']:
            shutil.copytree(ROOT/tree, root/tree)
        catalog=json.loads((root/CATALOG).read_text())
        for module in catalog['modules'].values():
            for file in module['implementation']:
                target=root/file;target.parent.mkdir(parents=True,exist_ok=True)
                if not target.exists(): target.write_text('/* staged evidence */\n')
        return catalog

    def write_catalog(self, root, catalog):
        (root/CATALOG).write_text(json.dumps(catalog,ensure_ascii=False))

    def test_catalog_and_repository_evidence(self):
        catalog=check_catalog()
        self.assertEqual(set(catalog['modules']),set(public_inventory()))
        self.assertEqual(len(catalog['compatibility']),109)
        self.assertTrue(check_private_boundaries())
        self.assertTrue(all(r['status']=='待审查' for r in catalog['retirement_review']))

    def test_unregistered_removed_header_and_entry_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);self.fixture_catalog(root)
            nested=root/'include/raylib_lite/nested/new.h';nested.parent.mkdir();nested.write_text('void raylib_lite_new(void);')
            with self.assertRaisesRegex(ValueError,'module/header'):check_catalog(root,False)
            nested.unlink()
            header=root/'include/raylib_lite/raylib_lite_scene.h';old=header.read_text()
            header.write_text(old+'\nvoid raylib_lite_scene_extra(void);\n')
            with self.assertRaisesRegex(ValueError,'entrypoint'): check_catalog(root,False)
            header.write_text(old.replace('void raylib_lite_scene_stack_init(raylib_lite_scene_stack_t *stack);',''))
            with self.assertRaisesRegex(ValueError,'entrypoint'): check_catalog(root,False)
            header.unlink()
            with self.assertRaisesRegex(ValueError,'module/header'): check_catalog(root,False)

    def test_contract_paths_retirement_fields_and_status_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);catalog=self.fixture_catalog(root)
            check_catalog(root,False)
            header = root/'include/raylib_lite/raylib_lite_scene.h'
            header.write_text(header.read_text()+'\n#define ScenePop raylib_lite_scene_pop\n')
            catalog['modules']['scene']['entries'] = sorted(catalog['modules']['scene']['entries']+['ScenePop'])
            self.write_catalog(root,catalog)
            check_catalog(root,False)  # Own macros are not frozen Raylib-name states.
            cases=[('contract',lambda c:c['modules']['scene'].pop('cleanup')),
                   ('role',lambda c:c['modules']['scene'].update(usage_role='private')),
                   ('entry_role',lambda c:c['modules']['scene'].update(entry_roles={'missing_entry':'game'})),
                   ('evidence',lambda c:c['modules']['scene'].update(implementation=['src/missing.c'])),
                   ('retirement',lambda c:c['retirement_review'][0].pop('migration')),
                   ('external',lambda c:c['retirement_review'][0].update(status='候选')),
                   ('external_evidence',lambda c:c['retirement_review'][0].update(status='候选',external_usage='已核实')),
                   ('documentation',lambda c:c['compatibility'].update(DrawText='等价'))]
            for _, change in cases:
                with self.subTest(case=_):
                    mutated=json.loads(json.dumps(catalog));change(mutated);self.write_catalog(root,mutated)
                    with self.assertRaises(ValueError):check_catalog(root,False)
            self.write_catalog(root,catalog)
            with self.assertRaisesRegex(ValueError,'Missing evidence file: tests/'):check_catalog(root,True)

    def test_private_header_and_primitive_use_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for name,text in {'tools/prepare_release.py':'GAMES=("sample",)\n',
                              'src/core/internal.h':'',
                              'examples/sample/main/game.c':'#include "internal.h"\n'}.items():
                p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
            with self.assertRaisesRegex(ValueError,'private header'):check_private_boundaries(root)
            (root/'examples/sample/main/game.c').write_text('raylib_lite_primitive_draw_triangle();\n')
            with self.assertRaisesRegex(ValueError,'private raster'):check_private_boundaries(root)


if __name__=='__main__':unittest.main()
