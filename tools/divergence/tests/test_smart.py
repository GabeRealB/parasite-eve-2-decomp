"""Structured edits preserve control flow, scope, and experiment accounting."""
import ctypes
import json
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from transforms import Program, OPERATORS, DEFAULT_OPERATORS, body_span
from smart import frontier, ordered_mutations, search
from test_toolset import Workspace


class TransformationTests(unittest.TestCase):
    def test_body_selection_preserves_other_functions_headers_and_strings(self):
        source = '#include "header.h"\n/* f(x) { fake } */\nint f(int x);\nint f(int x) {return x;}\nint g(void) {return 9;}\n'
        preprocessed = 'int f(int x); int f(int x) {return x;} int g(void) {return 9;}'
        program = Program(source, preprocessed, "f")
        start, end = body_span(source, "f")
        changed = program.apply(program.mutations(["sched_barrier"])[0])
        self.assertTrue(changed.startswith(source[:start]))
        self.assertTrue(changed.endswith(source[end:]))
        self.assertIn('int g(void) {return 9;}', changed)

    def test_if_inversion_keeps_single_condition_and_float_nan_semantics(self):
        source = 'int f(float x) {if (x < 0.0f) return 1; else return 2;}'
        p = Program(source)
        result = p.apply(p.mutations(["invert_if"])[0])
        self.assertIn('!(x < 0.0f)', result)
        self.assertNotIn('>=', result)

    def test_guard_preserves_nested_scope_and_does_not_move_labels(self):
        p = Program('int f(int x) { if(x) return 1; else {int x; x=3; return x;} }')
        changed = p.apply(p.mutations(["guard_else"])[0])
        self.assertIn('int x;', changed)
        Program(changed)
        unsafe = Program('int f(int x) { if(x) return 1; else {L: return 3;} }')
        self.assertFalse(unsafe.mutations(["guard_else"]))

    def test_new_candidate_exposes_a_new_transformation(self):
        p = Program('int f(int x) {if(x) return 1; else return 2;}')
        self.assertFalse(p.mutations(["join_returns"]))
        changed = p.apply(p.mutations(["guard_else"])[0])
        self.assertTrue(Program(changed).mutations(["join_returns"]))

    def test_type_guards_and_fresh_names(self):
        cases = [
            'volatile int x; x=a;', 'static int x; x=a;', 'int *x; x=0;',
            'int x; {short x; x=a;}', 'const int x=0;',
            'typedef volatile int V; V x; x=a;',
        ]
        for code in cases:
            p = Program('int f(int a) {' + code + ' return a;}')
            self.assertFalse(p.mutations(["temporary", "dead_store", "volatile_temp"]), code)
        p = Program('typedef unsigned short T; int f(int a) {T x; int divergence_tmp_0; x=a; return x;}')
        output = p.apply(p.mutations(["dead_store"])[0])
        self.assertIn('unsigned short divergence_tmp_1 = 0', output)
        self.assertNotIn('volatile_temp', DEFAULT_OPERATORS)

    def test_comments_directives_and_gnu_asm(self):
        p = Program('int f(int x) {__asm__ volatile("" : "+r"(x)); return x;}',
                    '# 10 "header.h"\nint f(int x) {/* hi */ __asm__ volatile("" : "+r"(x)); return x;}')
        self.assertIn('"+r"(x)', p.render())
        self.assertFalse(p.mutations(["sched_barrier"]))
        with self.assertRaisesRegex(ValueError, 'directives'):
            Program('int f(int x) {\n#if X\nreturn x;\n#endif\n}', 'int f(int x) {return x;}')

    @unittest.skipUnless(shutil.which("cc"), "native C compiler unavailable")
    def test_native_execution_preserves_returns_side_effects_and_conversions(self):
        sources = [
            'int f(int x, int y, int *count) {if ((*count)++ && x < y) {return x;} else {return y;}}',
            'int f(int x, int y, int *count) {unsigned short z; z = x + y + (*count)++; if (x) return z; return y;}',
            'int f(int x, int y, int *count) {int z; z=3; if(x) z=1; else if(y) z=2; return z;}',
        ]
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            for case, source in enumerate(sources):
                p = Program(source)
                variants = [source] + [p.apply(m) for m in p.mutations(OPERATORS)]
                text = '\n'.join(s.replace('int f(', f'int f{i}(') for i, s in enumerate(variants))
                path = directory / f'case{case}.c'
                path.write_text(text)
                library = directory / f'case{case}.so'
                subprocess.run(['cc', '-shared', '-fPIC', '-O2', str(path), '-o', str(library)], check=True, capture_output=True)
                dll = ctypes.CDLL(str(library))
                for x in (-65536, -1, 0, 1, 65535):
                    for y in (-2, 0, 3):
                        for count in (0, 1):
                            results = []
                            for i in range(len(variants)):
                                value = ctypes.c_int(count)
                                fn = getattr(dll, f'f{i}')
                                fn.argtypes = (ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int))
                                results.append((fn(x, y, ctypes.byref(value)), value.value))
                            self.assertTrue(all(r == results[0] for r in results), (x, y, count, results))


class SearchTests(unittest.TestCase):
    def test_family_routing_and_seed_are_reproducible(self):
        p = Program('int f(int a) {int x; x=a; if(x) return 1; else return 0;}')
        args = (p, {"families": {"branch": 3}}, DEFAULT_OPERATORS)
        a = list(ordered_mutations(*args, random.Random(3), {}))
        b = list(ordered_mutations(*args, random.Random(3), {}))
        self.assertEqual(a, b)
        self.assertIn(a[0].operator, ("invert_if", "guard_else"))

    def test_frontier_retains_same_assembly_sources_and_objective_progress(self):
        rows = [{"status": "complete", "source_sha256": str(i), "score": {"distance": 10 + i},
                 "identity": {"assembly_sha256": "same"}, "constraints": [{"status": "met" if i == 1 else "unknown"}]}
                for i in range(6)]
        result = frontier(rows, 3, random.Random(0))
        self.assertEqual([r['source_sha256'] for r in result[:2]], ['0', '1'])
        self.assertEqual(len({r['source_sha256'] for r in result}), 3)

    def test_search_crosses_neutral_steps_and_records_history(self):
        # Deliberately controlled scorer: only a transformation exposed by
        # the first rewrite improves. This tests search, not compiler behavior.
        from experiments import Runner
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = 'int f(int x) {if(x) return 1; else return 2;}\n'
            (root / 'base.c').write_text(source)
            (root / 'base.i').write_text(source)
            (root / 'base_object_dump_normalized.s').write_text('nop\n')
            (root / 'target_object_dump_normalized.s').write_text('nop\n')
            (root / 'build.sh').write_text('exit 0\n')
            from scratch import Attempt
            with patch('experiments.report', return_value={"identity": {}, "score": {"distance": 5}}):
                runner = Runner(Attempt(root, 'base', 'f'), root / 'output', budget=12)
            def build(text, label, parent=None, hypothesis=None):
                name = f'trial{runner.used}'
                runner.used += 1
                path = root / f'{name}.c'
                path.write_text(text)
                path.with_suffix('.i').write_text(text)
                distance = 0 if label == 'join_returns_depth_2' else 5
                row = {"source": str(path), "source_sha256": str(runner.used), "status": "complete",
                       "score": {"distance": distance}, "constraints": [], "identity": {}, "label": label}
                runner.rows.append(row)
                (runner.output / f'{name}.report.json').write_text(json.dumps({"families": {"branch": 1}}))
                return row
            with patch.object(runner, 'build', side_effect=build):
                result = search(runner, operators=('guard_else', 'join_returns'), depth=3)
            self.assertTrue(result['matched'])
            self.assertEqual([h['operator'] for h in runner.rows[-1]['history']], ['guard_else', 'join_returns'])
            self.assertEqual((root / 'base.c').read_text(), source)


class SearchFailureTests(Workspace):
    def test_failed_baseline_consumes_one_build_and_does_not_claim_a_winner(self):
        from experiments import Runner
        attempt = self.attempt()
        (self.root / 'build.sh').write_text('exit 3\n')
        runner = Runner(attempt, self.root / 'run', budget=4)
        result = search(runner)
        self.assertEqual(result['status'], 'baseline_failed')
        self.assertEqual(runner.used, 1)
        self.assertNotIn('best_source', result)

    def test_changed_baseline_score_stops_before_mutation(self):
        from experiments import Runner
        from unittest.mock import patch
        attempt = self.attempt()
        (self.root / 'build.sh').write_text('exit 0\n')
        runner = Runner(attempt, self.root / 'run', budget=4)
        row = {'source': str(attempt.source), 'score': {'distance': 0}, 'status': 'complete', 'constraints': []}
        runner.rows.append(row)
        with patch.object(runner, 'build', return_value=row):
            result = search(runner)
        self.assertEqual(result['status'], 'baseline_did_not_reproduce')
        self.assertFalse(result['matched'])
        self.assertFalse(result['improved'])

    def test_operator_families_exist(self):
        from analyze import FAMILY_RULES
        for operator in OPERATORS.values():
            self.assertFalse(set(operator.families) - FAMILY_RULES.keys())
