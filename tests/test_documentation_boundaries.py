# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import unittest
from urllib.parse import unquote, urlsplit
from tools.check_markdown_links import LINK
from tools.prepare_release import ROOT, DEVELOPMENT_DOCS
from tools.api_catalog import check_catalog


class DocumentationBoundaryTests(unittest.TestCase):
    def test_formal_documents_do_not_link_to_unpublished_development_records(self):
        documents=[ROOT/name for name in ('README.md','README.CN.md','README.EN.md','API.md')]
        documents += [p for p in (ROOT/'docs').rglob('*.md')
                      if not p.relative_to(ROOT).as_posix().startswith(DEVELOPMENT_DOCS)
                      and 'debug' not in p.relative_to(ROOT).parts]
        boundary=(ROOT/DEVELOPMENT_DOCS).resolve()
        for document in documents:
            if not document.is_file():continue
            for match in LINK.finditer(document.read_text()):
                raw=match.group(1).strip('<>')
                if raw.startswith('#') or urlsplit(raw).scheme:continue
                target=(document.parent/unquote(raw.split('#')[0])).resolve()
                with self.subTest(document=str(document.relative_to(ROOT)),target=raw):
                    self.assertFalse(target.is_relative_to(boundary))

    def test_roles_distinguish_review_and_integration_from_game_entries(self):
        catalog=check_catalog()
        for name in ('scene','ui','mtx2'):
            self.assertEqual(catalog['modules'][name]['usage_role'],'review')
        renderer=catalog['modules']['renderer_draw']
        self.assertEqual(renderer['usage_role'],'game')
        self.assertEqual(renderer['entry_roles']['raylib_lite_renderer_set_target'],'integration')
        self.assertEqual(catalog['modules']['compat.2d']['usage_role'],'compatibility')
        self.assertIn('docs/development/**/*',(ROOT/'idf_component.yml').read_text())


if __name__=='__main__':unittest.main()
