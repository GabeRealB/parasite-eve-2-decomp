"""Fixtures and source diagnostics must work in a fresh checkout."""
import json
from pathlib import Path
import shutil
from unittest.mock import patch

from test_toolset import Workspace
from experiments import apply_edits
from localize import report, render
from replay import replay
from rules import Sources

TESTS = Path(__file__).resolve().parent


class PortabilityTests(Workspace):
    def test_missing_gcc_tree_has_setup_guidance_and_no_citation(self):
        missing = self.root / 'uninitialized-gcc'
        sources = Sources(missing)
        self.assertEqual(sources.status()['status'], 'missing')
        data = report(self.attempt(), gcc_source=missing)
        self.assertIn('bash tools/fetch_gcc_source.sh', render(data))
        self.assertEqual(data['gcc_source']['status'], 'missing')
        for ref in sources.describe('allocation.global')['sources']:
            self.assertFalse(ref['available'])
            self.assertIsNone(ref['path'])
            self.assertNotIn('line', ref)
            self.assertEqual(ref['reason'], 'source_file_missing')

    def test_existing_file_without_definition_is_not_a_citation(self):
        (self.root / 'global.c').write_text('/* no function definition */\n')
        sources = Sources(self.root)
        self.assertEqual(sources.status()['status'], 'incomplete')
        ref = sources.reference('global.c', 'allocno_compare')
        self.assertFalse(ref['available'])
        self.assertIsNone(ref['path'])
        self.assertNotIn('line', ref)
        self.assertEqual(ref['reason'], 'definition_not_found')

    def test_custom_source_definition_is_located(self):
        (self.root / 'global.c').write_text('int\nallocno_compare (a, b)\n{ return 0; }\n')
        ref = Sources(self.root).reference('global.c', 'allocno_compare')
        self.assertTrue(ref['available'])
        self.assertEqual(ref['line'], 2)

    def test_bundled_replay_works_after_relocation_without_gcc_sources(self):
        shutil.copytree(TESTS / 'fixtures', self.root / 'fixtures')
        manifest = self.root / 'retained_cases.json'
        shutil.copyfile(TESTS / 'retained_cases.json', manifest)
        with patch('rules.GCC', self.root / 'absent-gcc'):
            result = replay(manifest)
        self.assertEqual(result['failed'], 0, result)
        self.assertEqual(result['passed'], 2)

    def test_compiler_checks_embed_their_inputs(self):
        checks = json.loads((TESTS / 'compiler_checks.json').read_text())
        seed = checks['sources']['base']
        edits = checks['plan']['mutations'][0]['edits']
        self.assertEqual(apply_edits(seed, edits), checks['sources']['target'])
        for filename in ('retained_cases.json', 'compiler_checks.json'):
            text = (TESTS / filename).read_text()
            self.assertNotIn('nonmatchings/', text)
            self.assertNotIn('/tmp/', text)
            self.assertNotIn('local/gcc/', text)
