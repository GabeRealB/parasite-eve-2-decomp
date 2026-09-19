"""Provenance, counterfactual guardrails and concrete edit regressions."""
import json
from pathlib import Path
import sys
import tarfile
from unittest.mock import patch
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_toolset import Workspace
from capture import record
from constraints import validate
from evidence import digest, load_trace, SUPPORTED, artifact_provenance
from experiments import run_process
from source_edits import candidates


class EditTests(unittest.TestCase):
    def test_adjacent_field_store_order(self):
        text = '    work->first = 1;\n    work->second = 2;\n'
        edits = candidates(text, {'statement_order'})
        self.assertEqual(len(edits), 1)
        self.assertEqual(edits[0]['edits'][0]['after'], '    work->second = 2;\n    work->first = 1;')

    def test_field_store_order_does_not_cross_dependency_or_effect(self):
        for text in ['p->x = 1;\np->x = 2;', 'p->x = 1;\nq->y = 2;',
                     'p->x = 1;\ncall();\np->y = 2;', 'p->x = 1;\np->y = f();']:
            self.assertEqual(candidates(text, {'statement_order'}), [])

    def test_whole_simple_statement(self):
        text = 'int f(int a, int b)\n{\n    return a + b;\n}\n'
        result = candidates(text, {'operand_order'})
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]['edits'][0]['after'], '    return b + a;')

    def test_subexpressions_and_effectful_operands_are_not_materialized(self):
        text = '    return a + b * c;\n    x = foo() + b;\n    x = a++ + b;\n    x = *a + b;\n'
        self.assertEqual(candidates(text, {'operand_order'}), [])

    def test_comments_strings_directives_are_masked(self):
        text = '/*\n    return a + b;\n*/\n#define OP a + b\nchar *s = "return a + b;";\n'
        self.assertEqual(candidates(text, {'operand_order'}), [])

    def test_repeated_statement_is_not_a_unique_literal_edit(self):
        text = '    x = a + b;\n    x = a + b;\n'
        self.assertEqual(candidates(text, {'operand_order'}), [])

    def test_continued_directive_is_masked(self):
        text = '#define ADD \\\n    x = a + b;\n'
        self.assertEqual(candidates(text, {'operand_order'}), [])

    def test_constraints_validate_input(self):
        for constraint in [{'kind': 'instruction', 'target_index': -1, 'pattern': 'x'},
                           {'kind': 'instruction', 'target_index': 0, 'pattern': '['},
                           {'kind': 'distance_le', 'value': '0'}]:
            with self.assertRaises(ValueError):
                validate([constraint])


class EvidenceTests(Workspace):
    def trace(self):
        attempt = self.attempt()
        assembly = '\taddu $2,$5,$6 # 1 add\n'
        attempt.cc1_assembly.write_text(assembly)
        trace = self.root / 'trace'
        trace.mkdir()
        (trace / 'baseline.s').write_text(assembly)
        (trace / 'manifest.json').write_text(json.dumps({'trace_complete': True, 'assembly_identical': True,
            'compiler_sha256': SUPPORTED, 'input_sha256': digest(attempt.preprocessed),
            'baseline_assembly_sha256': digest(trace / 'baseline.s'), 'flags': ['-O2', '-mcpu=3000', '-G0', '-da']}))
        (trace / 'events.jsonl').write_text('\n'.join(json.dumps(e) for e in [
            {'event': 'function', 'name': 'f'}, {'event': 'custom', 'uid': 1},
            {'event': 'function', 'name': 'other'}, {'event': 'custom', 'uid': 1}]) + '\n')
        return attempt, trace

    def test_trace_is_scoped_to_selected_function(self):
        attempt, trace = self.trace()
        rows, status = load_trace(trace, attempt, 'f')
        self.assertEqual(status['status'], 'verified_input')
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[-1]['event_index'], 2)

    def test_trace_with_identical_input_but_changed_uids_is_rejected(self):
        attempt, trace = self.trace()
        attempt.cc1_assembly.write_text('\taddu $2,$5,$6 # 42 add\n')
        rows, status = load_trace(trace, attempt, 'f')
        self.assertEqual(status['status'], 'rejected')
        self.assertTrue(any('UID' in reason for reason in status['reasons']))

    def test_full_build_flags_extract_cc1_segment(self):
        attempt, trace = self.trace()
        (self.root / 'attempts.jsonl').write_text(json.dumps({'source': attempt.source.name,
            'source_sha256': digest(attempt.source), 'flags': '-I include -P | -O2 -mcpu=3000 | --expand-div | -G0'}) + '\n{"truncated":')
        rows, status = load_trace(trace, attempt, 'f')
        self.assertEqual(status['flags_status'], 'verified')

    def test_flags_mismatch_is_rejected(self):
        attempt, trace = self.trace()
        (self.root / 'attempts.jsonl').write_text(json.dumps({'source': attempt.source.name,
            'source_sha256': digest(attempt.source), 'flags': '-O1 -mcpu=3000'}))
        rows, status = load_trace(trace, attempt, 'f')
        self.assertEqual(status['status'], 'rejected')

    def test_stale_source_is_flagged(self):
        attempt = self.attempt()
        (self.root / 'attempts.jsonl').write_text(json.dumps({'source': attempt.source.name, 'source_sha256': 'old'}))
        self.assertEqual(artifact_provenance(attempt)['status'], 'mismatch')

    def test_capture_does_not_invent_a_match_or_overwrite_session(self):
        attempt = self.attempt()
        store = self.root / 'store'
        first = record('f', self.root, store, attempt.source)
        second = record('f', self.root, store, attempt.source)
        self.assertNotEqual(first, second)
        entry = json.loads(first.read_text())
        self.assertFalse(entry['matched'])
        with tarfile.open(store / entry['artifacts']['archive']) as archive:
            self.assertIn('base.c', archive.getnames())
            self.assertIn('base.i', archive.getnames())

    def test_capture_uses_journal_parent(self):
        parent = self.attempt(stem='parent')
        winner = self.attempt(stem='winner')
        winner.source.write_text('int f(void) { return 0; }\n')
        winner.score.write_text('{"distance":0,"score":100}')
        (self.root / 'attempts.jsonl').write_text(json.dumps({'source': 'winner.c', 'source_sha256': digest(winner.source), 'parent': 'parent.c', 'distance': 0}))
        entry = json.loads(record('f', self.root, self.root / 'store').read_text())
        self.assertTrue(entry['matched'])
        self.assertEqual(entry['pairing'], 'recorded_parent')
        self.assertEqual(entry['near_miss']['stem'], 'parent')

    def test_failed_process_start_is_retained(self):
        result = run_process(['/nonexistent/divergence_test_executable'], self.root, self.root / 'log', 1)
        self.assertEqual(result['status'], 'failed')
        self.assertTrue((self.root / 'log').read_text())


class InterventionTests(unittest.TestCase):
    def hooks(self, spec, control=False):
        state = {'CONFIG': {'intervention': spec, 'control': control}, 'Entry': lambda *args: None}
        exec(compile((Path(__file__).resolve().parents[1] / '_gdb_intervene.py').read_text(), '_gdb_intervene.py', 'exec'), state)
        return state

    def test_global_order_changes_only_the_permutation(self):
        state = self.hooks({'kind': 'global_order', 'before': 80, 'after': 81})
        memory = {100: 1, 104: 2, 108: 0}
        state.update(value=lambda n: {'max_regno': 100, 'global.c:max_allocno': 3, 'global.c:allocno_order': 100}[n],
                     array=lambda n, i: {80: 0, 81: 1}[i], integer=lambda a: memory[a],
                     write_int=lambda a, v: memory.__setitem__(a, v), emit=lambda *a, **kw: None)
        state['global_order']()
        self.assertEqual(list(memory.values()), [0, 1, 2])

    def test_control_does_not_write(self):
        state = self.hooks({'kind': 'global_order', 'before': 80, 'after': 81}, True)
        memory = {100: 1, 104: 0}
        state.update(value=lambda n: {'max_regno': 100, 'global.c:max_allocno': 2, 'global.c:allocno_order': 100}[n],
                     array=lambda n, i: {80: 0, 81: 1}[i], integer=lambda a: memory[a],
                     write_int=lambda *args: self.fail('no-op control wrote memory'), emit=lambda *a, **kw: None)
        state['global_order']()
        self.assertEqual(list(memory.values()), [1, 0])

    def test_scheduler_cannot_cross_priority_groups(self):
        state = self.hooks({'kind': 'schedule_ready', 'uid': 11, 'pass': 'sched1'})
        memory = {100: 200, 104: 300, 204: 10, 304: 11}
        state.update(value=lambda n: 0, arg=lambda i: [100, 2][i], integer=lambda a: memory[a],
                     array=lambda n, i: {10: 5, 11: 3}[i], write_int=lambda *args: self.fail('crossed priority group'))
        state['schedule_ready']()
        self.assertFalse(state['APPLIED'])


if __name__ == '__main__':
    unittest.main()
