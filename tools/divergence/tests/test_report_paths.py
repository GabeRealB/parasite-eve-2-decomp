"""Exported metadata must not copy a checkout or home directory into reports."""
import json
from pathlib import Path
import sys
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_toolset import Workspace, insn
from experiments import Runner
from localize import report
from report_paths import ROOT, public_data, public_path
from rules import Sources


class ReportPathTests(Workspace):
    def test_repository_and_home_paths_are_portable(self):
        with patch('report_paths.ROOT', self.root / 'repo'), patch('pathlib.Path.home', return_value=self.root):
            self.assertEqual(public_path(self.root / 'repo/tools/example.py'), 'tools/example.py')
            self.assertEqual(public_path(self.root / 'notes/file.txt'), '~/notes/file.txt')
            self.assertEqual(public_path(self.root / 'repo'), '.')
            self.assertEqual(public_path(self.root), '~')

    def test_diagnostic_copy_does_not_mutate_runtime_paths(self):
        source = str(self.root / 'repo/base.c')
        original = {'source': source, 'nested': [{'error': f"cannot read '{source}'"}]}
        with patch('report_paths.ROOT', self.root / 'repo'):
            exported = public_data(original)
        self.assertEqual(exported['source'], 'base.c')
        self.assertEqual(exported['nested'][0]['error'], "cannot read 'base.c'")
        self.assertEqual(original['source'], source)
        self.assertIn(source, original['nested'][0]['error'])

    def test_sibling_prefix_is_not_the_repository(self):
        with patch('report_paths.ROOT', self.root / 'repo'), patch('pathlib.Path.home', return_value=self.root):
            self.assertEqual(public_path(self.root / 'repo-other/file.c'), '~/repo-other/file.c')

    def test_report_scrubs_paths_embedded_in_dump_messages(self):
        attempt = self.attempt()
        attempt.cc1_assembly.write_text('\taddu $2,$5,$6 # 1 add\n')
        attempt.dump('rtl').write_text(insn(1) + f';; insn 1 in {attempt.preprocessed}\n')
        with patch('report_paths.ROOT', self.root):
            exported = report(attempt)
        serialized = json.dumps(exported)
        self.assertNotIn(str(self.root), serialized)
        self.assertNotIn(str(Path.home()) + '/', serialized)
        self.assertIn('base.i.rtl', serialized)

    def test_manifest_uses_portable_paths_but_runner_keeps_executable_paths(self):
        attempt = self.attempt()
        (self.root / 'build.sh').write_text('exit 3\n')
        with patch('report_paths.ROOT', self.root):
            runner = Runner(attempt, self.root / 'output', budget=1)
            row = runner.build('int f(void) { return 1; }\n', 'probe')
        manifest = json.loads((runner.output / 'manifest.json').read_text())
        self.assertTrue(Path(row['source']).is_absolute())
        self.assertTrue(Path(row['source']).is_file())
        self.assertEqual(manifest['experiments'][0]['source'], Path(row['source']).name)
        self.assertEqual(manifest['experiments'][0]['log'], 'output/' + Path(row['log']).name)

    def test_builtin_source_citations_and_saved_checks_are_portable(self):
        references = Sources().describe('allocation.global')['sources']
        self.assertTrue(all(ref['expected_path'].startswith('local/gcc/') for ref in references))
        self.assertTrue(all(ref['path'] is None or ref['path'].startswith('local/gcc/') for ref in references))
        saved = (ROOT / 'tools/divergence/tests/compiler_checks.json').read_text()
        self.assertNotIn(str(ROOT), saved)
        self.assertNotIn(str(Path.home()) + '/', saved)
