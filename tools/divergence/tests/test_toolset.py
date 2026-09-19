"""Regression cases for false diagnoses and retained experiment behavior."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from align import align, Pair
from analyze import Analyzer, FAMILY_RULES, reorders
from attribute import Compile, Attribution, _link
from common import parse_asm, is_register
from compare import correspond, pseudo_correspondence, compare
from constraints import evaluate
import dumps
from evidence import digest, load_trace, SUPPORTED
from experiments import Runner, apply_edits, run_process
from families import Finding, classify
from localize import report
from minimize import changes, reconstruct
from replay import replay
from rtl import instructions, events
from rules import RULES, Sources, GCC
from scratch import Attempt
from search import select


def insn(uid, destination=80, source=81, prev=0, following=0):
    return f"(insn {uid} {prev} {following} (set (reg:SI {destination}) (plus:SI (reg:SI {source}) (const_int 1))) -1 (nil) (nil))\n"


class Workspace(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def attempt(self, target="addu a0,a1,a2\n", ours="addu v0,a1,a2\n", stem="base"):
        (self.root / "target_object_dump_normalized.s").write_text(target)
        (self.root / f"{stem}_object_dump_normalized.s").write_text(ours)
        (self.root / f"{stem}.c").write_text("int f(int a, int b) { return a + b; }\n")
        (self.root / f"{stem}.i").write_text("int f(int a, int b) { return a + b; }\n")
        (self.root / f"{stem}.score.json").write_text(json.dumps({"score": 90, "distance": 5, "penalties": {"regs": 1}}))
        return Attempt(self.root, stem)


class AssemblyTests(Workspace):
    def test_nonallocation_families_from_real_instruction_forms(self):
        cases = [
            ('lhu a0,0(v0)', 'lh a0,0(v0)', 'opcode'),
            ('lw a0,4(v0)', 'lw a0,8(v0)', 'displacement'),
            ('lui a0,%hi(first)', 'lui a0,%hi(second)', 'symbol'),
            ('jal first', 'jal second', 'call'),
            ('beq a0,a1,10', 'bne a0,a1,10', 'branch'),
            ('nop', 'li v0,1', 'hazard'),
            ('lw a0,4(v0)', 'lw a1,8(v1)', 'mixed'),
            ('addu v0,a0,a1', 'addu v0,a0,a1\nsw v0,0(a2)', 'insertion'),
            ('addu v0,a0,a1\nsw v0,0(a2)', 'addu v0,a0,a1', 'deletion'),
        ]
        for target, ours, family in cases:
            with self.subTest(family=family):
                findings = classify(align(parse_asm(target), parse_asm(ours)))
                self.assertIn(family, [f.family for f in findings])

    def test_immediates_never_become_register_substitutions(self):
        self.assertFalse(is_register("12"))
        self.assertTrue(is_register("$12"))
        self.assertFalse(is_register("$64"))
        findings = classify(align(parse_asm("sll v0,a0,2"), parse_asm("sll v0,a0,3")))
        self.assertEqual([f.family for f in findings], ["immediate"])

    def test_numeric_spelling_and_explicit_registers(self):
        a = parse_asm("lw $4,0x18($sp)")
        b = parse_asm("lw a0,24(sp)")
        self.assertFalse(align(a, b).divergences)

    def test_inline_comments_in_dp_annotation(self):
        emitted = dumps.emitted("\tli\t$2,528482304 # 0x1f800000 # 492 movsi_internal2/3\n\tlui $4,%hi(object) # high # 76 high\n")
        self.assertEqual([e.uid for e in emitted], [492, 76])
        linked = _link(parse_asm("lui v0,0x1f80\nlui a0,%hi(object)"), emitted)
        self.assertEqual([linked[i].uid for i in sorted(linked)], [492, 76])

    def test_replacement_is_not_a_compiler_link(self):
        emitted = dumps.emitted("\tlw $2,0($4) # 7 load\n")
        self.assertEqual(_link(parse_asm("sw v1,0(a0)"), emitted), {})

    def test_delay_slot_predecessor_ignores_alignment_gap(self):
        findings = classify(align(parse_asm("jr ra\nnop\n"), parse_asm("jr ra\nmove v0,a0\n")))
        self.assertTrue(any(f.family == "delay_slot" for f in findings))

    def test_frame_adjustment_is_stack_family(self):
        f = classify(align(parse_asm("addiu sp,sp,-32"), parse_asm("addiu sp,sp,-40")))
        self.assertEqual(f[0].family, "stack")

    def test_missing_artifact_discovery(self):
        (self.root / "base_object_dump_normalized.s").write_text("nop\n")
        with self.assertRaises(FileNotFoundError):
            Attempt.discover(self.root)


class RTLTests(unittest.TestCase):
    def test_nested_delay_insn_is_balanced(self):
        text = '(insn 50 0 0 (sequence [(jump_insn 7 0 8 (return) -1 (nil) (nil))\n (insn 8 7 0 (set (reg:SI 2) (const_int 0)) -1 (nil) (nil))]) -1 (nil) (nil))'
        nodes = instructions(text)
        self.assertEqual(set(nodes), {50, 7, 8})
        self.assertEqual(nodes[8].expression, "(set (reg:SI 2) (const_int 0))")
        self.assertNotIn("jump_insn", dumps.insn_rtl(text, 8))

    def test_hilo_and_virtual_registers_are_not_pseudos(self):
        self.assertEqual(dumps.pseudos_in("(reg:SI 64) (reg:SI 65) (reg:SI 75) (reg:SI 76)"), [76])

    def test_register_changes_are_not_scheduling(self):
        a = instructions(insn(1, 80, 81) + insn(2, 82, 83))
        b = instructions(insn(1, 2, 3) + insn(2, 4, 5))
        self.assertEqual(reorders(a, b, 1), [])
        c = instructions(insn(2, 4, 5) + insn(1, 2, 3))
        self.assertEqual(reorders(a, c, 1), [2])

    def test_deleted_note_is_not_an_active_insn(self):
        snapshots = {"rtl": instructions(insn(1)), "cse": instructions('(note 1 0 0 "" NOTE_INSN_DELETED)')}
        self.assertEqual(events(snapshots, 1)[-1]["event"], "removed")

    def test_renumbering_maps_values_by_expression(self):
        pairs = correspond(instructions(insn(1, 80, 81)), instructions(insn(42, 120, 121)))
        self.assertEqual(pseudo_correspondence(pairs), {80: 120, 81: 121})

    def test_duplicate_expressions_are_not_all_forced_into_pairs(self):
        a = instructions("".join(insn(i) for i in range(1, 6)))
        b = instructions("".join(insn(i, 120, 121) for i in range(10, 15)))
        self.assertLess(len(correspond(a, b)), 5)


class DiagnosisTests(Workspace):
    def test_all_families_have_algorithms_and_edits(self):
        attempt = self.attempt()
        c = Compile.load(attempt)
        for family in FAMILY_RULES:
            with self.subTest(family=family):
                finding = Finding(family, Pair("replace", parse_asm("lw a0,0(a1)")[0], parse_asm("lh v0,0(a1)")[0]))
                result = Analyzer(c).one(Attribution(finding))
                self.assertTrue(result["hypotheses"])
                self.assertTrue(result["proposals"])
                self.assertTrue(result["unknowns"])
                for edit in result["proposals"]:
                    self.assertTrue(edit["precondition"])
                    self.assertTrue(edit["prediction"])

    def test_initial_conflict_absence_is_not_availability(self):
        attempt = self.attempt()
        attempt.cc1_assembly.write_text("\taddu $2,$5,$6 # 1 add\n")
        attempt.dump("lreg").write_text("Register 80 used 3 times across 7 insns.\n" + insn(1))
        attempt.dump("greg").write_text(";; 1 regs to allocate: 80\n;; 80 conflicts: 2 3\n;; Register dispositions:\n80 in 2\n" + insn(1, 2, 5))
        data = report(attempt)
        analysis = data["divergences"][0]["analysis"]
        self.assertTrue(any("does not establish availability" in u for u in analysis["unknowns"]))
        self.assertNotIn("was available", " ".join(data["divergences"][0]["explanation"]))

    def test_postreload_load_replacement_is_not_labelled_scheduler(self):
        attempt = self.attempt("move a0,a1", "move v0,a1")
        attempt.cc1_assembly.write_text("\tmove $2,$5 # 7 mov\n")
        attempt.dump("greg").write_text('(insn 7 0 0 (set (reg:SI 2) (mem:SI (reg:SI 4))) -1 (nil) (nil))')
        attempt.dump("sched2").write_text('(insn 7 0 0 (set (reg:SI 2) (reg:SI 5)) -1 (nil) (nil))')
        site = report(attempt)["divergences"][0]
        self.assertIn("allocation.reload", [r["id"] for r in site["analysis"]["hypotheses"]])
        self.assertNotEqual(site["decided_by"], "sched2")

    def test_missing_dumps_do_not_claim_a_pass(self):
        data = report(self.attempt())
        self.assertIsNone(data["divergences"][0]["decided_by"])
        self.assertEqual(data["available_dumps"], [])

    def test_multi_function_uid_namespace_requires_selection(self):
        attempt = self.attempt()
        attempt.cc1_assembly.write_text('.ent f\n addu $2,$5,$6 # 1 add\n.end f\n.ent g\n addu $3,$5,$6 # 1 add\n.end g\n')
        with self.assertRaisesRegex(ValueError, "multiple functions"):
            Compile.load(attempt)

    def test_source_citations_resolve_definitions(self):
        if not GCC.is_dir():
            self.skipTest("GCC source checkout unavailable")
        sources = Sources()
        for key in RULES:
            for ref in sources.describe(key)["sources"]:
                self.assertTrue(ref["available"], ref)
                line = (GCC.parents[2] / ref["path"]).read_text().split("\n")[ref["line"] - 1].strip()
                self.assertTrue(line.startswith(ref["symbol"] + " ") or line.startswith(ref["symbol"] + "(") or line.startswith("def " + ref["symbol"]), ref)

    def test_stale_trace_is_rejected(self):
        attempt = self.attempt()
        trace = self.root / "trace"
        trace.mkdir()
        (trace / "manifest.json").write_text(json.dumps({"trace_complete": True, "assembly_identical": True,
                                                        "compiler_sha256": SUPPORTED, "input_sha256": "wrong"}))
        (trace / "events.jsonl").write_text('{"event":"function","name":"f"}\n')
        rows, status = load_trace(trace, attempt)
        self.assertFalse(rows)
        self.assertEqual(status["status"], "rejected")

    def test_constraints_use_fixed_target_sites(self):
        attempt = self.attempt()
        result = evaluate(attempt, report(attempt), [{"kind": "instruction", "target_index": 0, "pattern": "^addu v0,"},
                                                   {"kind": "rtl", "target_index": 0, "pass": "lreg", "pattern": "set"}])
        self.assertEqual([r["status"] for r in result], ["met", "unknown"])

    def test_compare_requires_same_target(self):
        first = self.attempt()
        other = self.root / "other"
        other.mkdir()
        (other / "target_object_dump_normalized.s").write_text("nop")
        with self.assertRaisesRegex(ValueError, "different targets"):
            compare(first, Attempt(other, "base"))

    def test_replay_checks_observations_not_only_report_generation(self):
        self.attempt()
        manifest = self.root / "cases.json"
        manifest.write_text(json.dumps({"cases": [{"scratch": ".", "stem": "base", "families": ["register"], "observations": ["local_choice"]}]}))
        self.assertEqual(replay(manifest)["failed"], 1)


class ExperimentTests(Workspace):
    def test_ambiguous_edit_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "exactly once"):
            apply_edits("x + y; x + y;", [{"before": "x + y", "after": "y + x"}])

    def test_reduction_preserves_context_and_all_edits(self):
        a = "header\none\ntwo\nfooter\n"
        b = "header\nchanged\ntwo\nadded\nfooter\n"
        lines, edits = changes(a, b)
        self.assertEqual(reconstruct(lines, edits, range(len(edits))), b)
        self.assertEqual(reconstruct(lines, edits, []), a)

    def test_beam_keeps_output_and_constraint_progress(self):
        def row(name, distance, status):
            return {"status": "complete", "source_sha256": name, "identity": {"assembly_sha256": "same"},
                    "score": {"distance": distance}, "constraints": [{"status": status}]}
        rows = [row("a", 10, "unmet"), row("b", 20, "met"), row("c", 30, "unknown")]
        self.assertEqual({r["source_sha256"] for r in select(rows, 2)}, {"a", "b"})

    def test_failed_build_uses_budget_and_keeps_source(self):
        attempt = self.attempt()
        (self.root / "build.sh").write_text("#!/bin/sh\nexit 3\n")
        runner = Runner(attempt, self.root / "evidence", budget=1)
        row = runner.build("invalid C\n", "bad")
        self.assertEqual(row["status"], "failed")
        self.assertTrue(Path(row["source"]).is_file())
        with self.assertRaisesRegex(RuntimeError, "budget"):
            runner.build("different invalid C", "bad2")
        self.assertEqual(json.loads((runner.output / "manifest.json").read_text())["used"], 1)

    def test_zero_exit_without_artifacts_is_failure(self):
        attempt = self.attempt()
        (self.root / "build.sh").write_text("#!/bin/sh\nexit 0\n")
        runner = Runner(attempt, self.root / "evidence", budget=1)
        self.assertEqual(runner.build("int f(void) {return 1;}", "missing")["status"], "missing_artifacts")

    def test_timeout_terminates_build(self):
        result = run_process([sys.executable, "-c", "import time; time.sleep(3)"], self.root, self.root / "log", .05)
        self.assertEqual(result["status"], "timeout")


if __name__ == "__main__":
    unittest.main()
