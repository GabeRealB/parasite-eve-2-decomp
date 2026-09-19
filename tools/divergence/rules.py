"""Auditable GCC 2.8.1 mechanisms and experiments, not guessed diagnoses.

Algorithms describe the inspected source. A mechanism's applicability to a
particular divergence is established separately by dump/trace observations.
"""
from __future__ import annotations

from dataclasses import dataclass, asdict
import hashlib
from pathlib import Path
import re
from report_paths import public_path

ROOT = Path(__file__).resolve().parents[2]
GCC = ROOT / "local/gcc/gcc-2.8.1-psx"


@dataclass(frozen=True)
class Edit:
    id: str
    change: str
    example: str
    precondition: str
    prediction: str
    check: str


@dataclass(frozen=True)
class Rule:
    id: str
    title: str
    passes: tuple[str, ...]
    sources: tuple[tuple[str, str], ...]
    algorithm: tuple[str, ...]
    edits: tuple[Edit, ...]
    missing: str


ORDER = Edit("operand_order", "Swap the operands of the implicated commutative expression.",
             "sum = left + right;  ->  sum = right + left;",
             "Integer commutative operation; preserve operand evaluation effects and types.",
             "Operand tying or arithmetic preference changes for this value.",
             "Compare local quantity members and suggestions, then global preferences and final homes.")
LIFETIME = Edit("lifetime", "Move the implicated definition toward its first use, or separate independent uses of a reused local.",
                "tmp = expression; ... use(tmp);  ->  ... tmp = expression; use(tmp);",
                "Moving expression must preserve reads, writes, calls, volatile accesses and evaluation count.",
                "The relevant live interval or quantity membership changes without changing computed values.",
                "Compare actual quantity intervals/global order and check reload and scheduling regressions.")
STATEMENT = Edit("statement_order", "Exchange adjacent independent computations around the divergent site.",
                 "a = left; b = right;  ->  b = right; a = left;",
                 "Establish independence including aliasing, calls and volatile accesses before applying.",
                 "The dependency graph, release order or original-order tie changes for the selected instructions.",
                 "Check block membership, dependency release, ready selection and final instruction order.")
SHARE = Edit("expression_sharing", "Compare a shared temporary with separate computations at the relevant uses.",
             "a = base + offset; use(a); ... use(a);  <->  use(base + offset); ... use(base + offset);",
             "Both forms must read the same values; memory writes/calls can invalidate recomputation.",
             "CSE equivalence, reference counts or materialisation lifetime changes.",
             "Compare cse/cse2 substitutions, loop induction, and reload rematerialisation.")
EXIT = Edit("exit_shape", "Compare shared and separate exits, preserving each path's side effects and return value.",
            "if (cond) return value; ... return value;  <->  a shared return after equivalent paths",
            "Every path must retain its predicate, stores, calls and return value.",
            "Tail merging, branch layout or eligible delay-slot candidates change.",
            "Compare jump/jump2 and dbr, then object control-flow diagnostics.")
WIDTH = Edit("access_type", "Inspect the access and promotion at this site; try a justified cast at the use before changing shared declarations.",
             "value = ptr->field;  ->  value = (u8)ptr->field;  (only if zero extension is intended)",
             "Confirm target width/signedness and intended values; a different opcode alone does not justify a semantic change.",
             "The initial RTL has the required memory mode and sign/zero extension.",
             "Check rtl then combine; confirm all consumers, not only the load opcode.")
LOOP = Edit("loop_form", "Compare equivalent index and pointer traversal, or alternate equivalent loop exit forms.",
            "use(items[i]); ++i;  <->  use(*cursor); ++cursor;",
            "Preserve iteration count, bounds, signedness, overflow behavior and values used after the loop.",
            "The basic/general induction variables, strength reduction or exit comparison change.",
            "Inspect loop and cse2, including backedges and the value after the last iteration.")

RULES = {
    r.id: r for r in (
        Rule("allocation.local", "Local quantities, suggestions and priority", ("sched", "lreg"),
             (("local-alloc.c", "combine_regs"), ("local-alloc.c", "qty_sugg_compare_1"),
              ("local-alloc.c", "qty_compare_1"), ("local-alloc.c", "find_free_reg")),
             ("Eligible pseudos are tied into quantities; per-pseudo dump statistics do not give the quantity's priority.",
              "Suggested quantities are tried first: copy suggestions precede arithmetic suggestions, fewer suggestions precede more.",
              "Remaining quantities use integer floor_log2(refs)*refs*size/(death-birth)*10000, with lower quantity number breaking ties.",
              "find_free_reg checks class, mode, calls and hard-register occupancy throughout the interval."),
             (ORDER, LIFETIME, STATEMENT), "Actual quantity membership, suggestions and intervals require a local_choice trace."),
        Rule("allocation.global", "Global allocation order, conflicts and preferences", ("lreg", "greg"),
             (("global.c", "allocno_compare"), ("global.c", "prune_preferences"), ("global.c", "find_reg")),
             ("Allocnos may group pseudos. Priority uses integer floor_log2(refs)*refs/live_length*10000*size, with allocno number breaking ties.",
              "Candidate registers must satisfy class, mode, fixed-register, call and evolving conflict constraints.",
              "The first search also avoids unused registers and preferences reserved for later conflicting allocnos; a second search relaxes those restrictions.",
              "Eligible copy/arithmetic preferences can override the first candidate. Caller-save retries and eviction are later alternatives.",
              "The .greg header predates assignments; final dispositions include reload. Neither alone reconstructs the decision-time candidate set."),
             (LIFETIME, ORDER, SHARE), "Use global_choice events for current conflicts/preferences and spill events for later changes."),
        Rule("allocation.reload", "Reload scratch reservations and late substitutions", ("greg", "sched2"),
             (("reload1.c", "spill_hard_reg"), ("reload1.c", "reload_cse_simplify_set"), ("toplev.c", "rest_of_compilation")),
             ("Reload satisfies machine operand constraints, reserving scratch registers and potentially evicting earlier assignments.",
              "After the .greg dump, reload_cse_regs can replace a load with an available hard-register value before sched2 runs.",
              "A greg-to-sched2 expression change is therefore not by itself a scheduling decision."),
             (LIFETIME, SHARE, WIDTH), "Observe spill and postreload_set events; confirm proposed substitutions survive in sched2."),
        Rule("scheduling", "Dependency release, ready ordering and hazards", ("combine", "sched", "greg", "sched2"),
             (("sched.c", "sched_analyze_1"), ("sched.c", "rank_for_schedule"),
              ("sched.c", "schedule_insn"), ("sched.c", "schedule_select")),
             ("GCC schedules backward within a basic block. Data, anti/output and memory dependencies control readiness.",
              "Ready-list comparison considers priority, dependence on the last scheduled instruction, and original RTL order.",
              "schedule_select can queue an instruction for actual hazards or select using potential hazard weights; winning a comparator is insufficient.",
              "Sched1 changes lifetimes before allocation; sched2 operates after reload and late CSE."),
             (STATEMENT, LIFETIME, SHARE), "Trace schedule_release, schedule_compare and schedule_select for the affected UIDs."),
        Rule("delay_slot", "Branch reorganisation and slot eligibility", ("jump2", "dbr"),
             (("reorg.c", "fill_simple_delay_slots"), ("reorg.c", "fill_eager_delay_slots"), ("reorg.c", "dbr_schedule")),
             ("Branch reorganisation searches for eligible instructions while checking resources and execution paths.",
              "It can take instructions from before a transfer or eligible successor threads; the final RTL sequence records the filled slot.",
              "A nop after a branch does not prove which candidate was rejected, and an assembly reorder can also be delay filling."),
             (EXIT, STATEMENT), "Inspect the dbr sequence and resource/eligibility conditions; the standard tracer does not observe every rejected candidate."),
        Rule("extension", "Access width, promotion and truncation", ("rtl", "combine"),
             (("expr.c", "convert_move"), ("expr.c", "expand_expr"), ("combine.c", "try_combine")),
             ("Expansion chooses memory modes and sign/zero extension from the typed expression.",
              "Combine may eliminate an extension proved redundant, while separate modes or uses can retain a copy.",
              "A load-width mismatch can reflect a wrong source type or an optimisation; inspect the first RTL access and its consumers."),
             (WIDTH,), "Source types and consumers are required to distinguish a semantic mismatch from a redundant extension."),
        Rule("instruction_selection", "RTL combination and machine patterns", ("rtl", "cse", "combine"),
             (("combine.c", "try_combine"), ("expr.c", "expand_expr"), ("config/mips/mips.c", "mips_move_1word")),
             ("Combine substitutes related definitions into uses, simplifies the expression and asks recognition whether the target supports the resulting pattern.",
              "Modes, use/death information and side effects constrain a legal combination; an equivalent C spelling may expand differently.",
              "The emitted -dp pattern names identify selected patterns, not why every alternative failed."),
             (ORDER, WIDTH, SHARE), "Compare expansion and combine expressions; rejected-pattern reasoning may require compiler instrumentation."),
        Rule("cse", "Expression equivalence and invalidation", ("cse", "cse2", "greg", "sched2"),
             (("cse.c", "cse_insn"), ("cse.c", "fold_rtx"), ("reload1.c", "reload_cse_simplify_set")),
             ("CSE tracks expression equivalence, folds constants and substitutes suitable equivalents.",
              "Writes and calls invalidate affected expressions; giving an expression a C name does not guarantee reuse.",
              "Late hard-register CSE is separate from the earlier pseudo-register passes."),
             (SHARE, STATEMENT, LIFETIME), "Identify the defining expression and its invalidations; a missing instruction alone cannot identify CSE."),
        Rule("loop", "Induction variables and loop transformations", ("loop", "cse2"),
             (("loop.c", "loop_optimize"), ("loop.c", "strength_reduce")),
             ("Loop optimisation identifies invariants and basic/general induction variables, then considers moving computations and strength reduction.",
              "A transformed address walk or exit test can subsequently be canonicalised by cse2.",
              "The counter's mode, updates, bounds and uses after the loop constrain equivalent forms."),
             (LOOP, SHARE, LIFETIME), "Retain loop diagnostics and compare recurrence, exit predicate and post-loop uses."),
        Rule("branch", "Jump simplification and shared tails", ("rtl", "jump", "cse", "loop", "cse2", "jump2"),
             (("jump.c", "jump_optimize"), ("toplev.c", "rest_of_compilation")),
             ("Jump optimisation removes jumps to following labels, simplifies jump chains and may share equivalent tails.",
              "Jump cleanup also runs around CSE, so dump suffixes describe intervals containing more than one transformation.",
              "Changed disassembly branch addresses can result from layout alone; compare resolved block edges and predicates."),
             (EXIT, LOOP, STATEMENT), "Use object topology and RTL label references; matching topology is not semantic equivalence."),
        Rule("stack", "Local storage, spills and frame construction", ("rtl", "lreg", "greg", "sched2"),
             (("function.c", "assign_stack_local"), ("reload1.c", "spill_hard_reg"), ("config/mips/mips.c", "compute_frame_size")),
             ("Stack slots can originate in addressable locals, reload spills, caller saves or ABI argument storage.",
              "MIPS frame construction accounts for local storage, outgoing arguments, saved registers and alignment.",
              "A changed offset is not proof of a new local: a preceding slot, save set or alignment can move many accesses."),
             (LIFETIME, Edit("local_storage", "Review unnecessary address-taking and the size/alignment of implicated locals.",
                 "use(&temporary);  ->  use(value); only where an equivalent interface already exists",
                 "Preserve object identity, lifetime, aliasing and the actual callee contract.",
                 "Addressable storage or spill pressure changes while ABI-required storage remains.",
                 "Compare initial stack MEMs, reload spills, saved registers and frame adjustment.")),
             "Track where stack MEMs first appear and observe reload; prologue output may have no original source UID."),
        Rule("address", "Address forms, constants and relocation expressions", ("rtl", "cse", "loop", "cse2", "combine", "greg"),
             (("cse.c", "fold_rtx"), ("loop.c", "strength_reduce"), ("config/mips/mips.c", "mips_move_1word")),
             ("Address expressions are folded, shared and strength-reduced before target patterns and reload constrain materialisation.",
              "The same symbol can require different high/low relocation and base-register sequences depending on expression and lifetime.",
              "Symbol spelling or a shifted label alone does not establish a changed address value."),
             (SHARE, LOOP, Edit("field_access", "Check the implicated field offset and access type against the target layout.",
                 "base->field  versus  a verified array element at the same offset",
                 "Keep structure layout, alignment and access width consistent with the real headers.",
                 "The MEM address has the required displacement or induction form.",
                 "Compare rtl/cse2 MEM addresses and object relocations.")),
             "Compare relocation identity/addends as well as normalised instruction text."),
        Rule("call", "Call expansion and calling convention", ("rtl", "greg", "dbr"),
             (("calls.c", "expand_call"), ("config/mips/mips.c", "function_arg")),
             ("Call expansion uses the declaration, argument modes and target ABI to arrange argument registers and stack arguments.",
              "Calls clobber caller-used registers and affect liveness, spilling and delay-slot candidates.",
              "An indirect/direct call or different destination can be a source/declaration error rather than an allocation question."),
             (Edit("call_contract", "Verify callee identity, prototype and argument expressions at the divergent call.",
                   "Check signed/unsigned argument promotion and direct function versus function-pointer call.",
                   "Use the actual callee contract; do not alter a prototype merely to reduce an assembly score.",
                   "Initial call RTL, argument modes and destination agree with the target requirement.",
                   "Compare call_insn, object call relocations, stack arguments and the final delay slot."), LIFETIME),
             "Call targets and ABI need source declarations and relocations, not register names alone."),
        Rule("assembler", "Assembler expansion and hardware delay handling", (),
             (("@tools/maspsx/maspsx/__init__.py", "process_line"),
              ("@tools/maspsx/maspsx/__init__.py", "_handle_nop_before_next_instruction")),
             ("maspsx expands pseudo-instructions and handles load and HI/LO hazards after cc1 emits assembly.",
              "One annotated compiler instruction can expand into several object instructions; inserted nops need not have a compiler UID.",
              "A missing link is not proof of assembler generation. Compare emitted text and assembled instructions with supported expansion rules."),
             (STATEMENT, SHARE), "Inspect emitted assembly, maspsx version/options and object instructions before changing C."),
        Rule("linkage", "Integration, relocations and data placement", (),
             (("@tools/rodata_triage.py", "owner_of"), ("@tools/claude-decomp-env/diagnose.py", "compare")),
             ("An instruction-text match does not verify relocations, ABI/header context or linked data placement.",
              "Generated jump tables must be integrated into the unit and section that owns their target data.",
              "Use the ordinary project build and checksum to verify integration."),
             (Edit("integration", "Port with the real headers and verify the owning unit's data/section layout.",
                   "Run tools/rodata_triage.py OVERLAY when a matching switch function fails integration.",
                   "Keep the matching function and unrelated definitions intact.",
                   "Relocations and generated data land at the target addresses.",
                   "Run the scoped build while iterating and the required full verification before accepting a match."),),
             "A scratch instruction stream cannot establish linked-binary equivalence."),
    )
}


class Sources:
    def __init__(self, root: Path | None = None):
        self.root = Path(root) if root is not None else GCC
        self.cache = {}

    def status(self) -> dict:
        files = sorted({file for rule in RULES.values() for file, _ in rule.sources if not file.startswith("@")})
        missing = [file for file in files if not (self.root / file).is_file()]
        return {"status": "available" if not missing else "incomplete" if self.root.is_dir() else "missing",
                "root": public_path(self.root), "missing_files": missing,
                "setup": "bash tools/fetch_gcc_source.sh",
                "guidance": "Fetch the patched source from the repository root, or use --gcc-source with an initialized GCC 2.8.1 PSX source tree."}

    def reference(self, file: str, symbol: str) -> dict:
        path = ROOT / file[1:] if file.startswith("@") else self.root / file
        key = (str(path), symbol)
        if key not in self.cache:
            result = {"path": None, "expected_path": public_path(path), "file": file.lstrip("@"),
                      "symbol": symbol, "available": False, "reason": "source_file_missing"}
            if path.is_file():
                data = path.read_bytes()
                text = data.decode(errors="replace")
                prefix = r"(?m)^[ \t]*def[ \t]+" if path.suffix == ".py" else r"(?m)^"
                match = re.search(prefix + re.escape(symbol) + r"[ \t]*\(", text)
                result.update(sha256=hashlib.sha256(data).hexdigest(), available=bool(match))
                if match:
                    result.update(path=public_path(path), reason=None)
                    result["line"] = text.count("\n", 0, match.start()) + 1
                else:
                    result["reason"] = "definition_not_found"
            self.cache[key] = result
        return self.cache[key]

    def describe(self, name: str) -> dict:
        rule = RULES[name]
        return {"id": rule.id, "title": rule.title, "passes": list(rule.passes),
                "algorithm": list(rule.algorithm), "sources": [self.reference(*s) for s in rule.sources],
                "missing_evidence": rule.missing}


def edits(names: list[str]) -> list[dict]:
    result = {}
    for name in names:
        for edit in RULES[name].edits:
            if edit.id not in result:
                result[edit.id] = {**asdict(edit), "mechanisms": [], "status": "proposed"}
            result[edit.id]["mechanisms"].append(name)
    return list(result.values())
