# Validation record

The automated fixtures are self-contained. Their inputs are either checked in
under `tests/fixtures/` or embedded in `tests/compiler_checks.json`; none depend
on a previous matching scratch or temporary test output.

## Prerequisites

Basic regression tests and fixture replay require Python, but no downloaded GCC
source tree. To check algorithm citations against the actual source, initialize
the patched tree from the repository root:

```sh
bash tools/fetch_gcc_source.sh
```

This creates the generated, Git-ignored `local/gcc/gcc-2.8.1-psx` directory.
A fresh checkout does not contain it. Source-definition checks are skipped when
it is absent; separate tests verify missing-tree and missing-definition handling.
The real-compiler smoke test needs the bundled compiler, maspsx and cross-binutils
used by the project build. It does not need the downloaded GCC sources.

## Automated checks

* Regression suite: 60 tests covering instruction/RTL parsing, ambiguous
  correspondence, false allocation/scheduling inferences, source citations,
  trace provenance, edit boundaries, constraints, intervention guardrails,
  retention, failures, timeouts and portable report paths.
* Missing and incomplete source trees are reported explicitly. Missing files or
  definitions never produce a citation path or line number.
* Two bundled replay cases check assembly hashes, register-allocation and
  access-width observations, proposals, and applicable instruction/RTL objectives.
  They use hand-written parser inputs, not captured compiler outputs.
* A portability test copies the fixtures and manifest to a new directory and
  replays them with GCC sources absent. Every replay input travels with them.
* The full suite also passes with the default GCC source tree simulated as
  absent; only the source-definition check is skipped.

```sh
python3 -B -m unittest discover -s tools/divergence/tests -v
python3 tools/divergence/toolset.py replay tools/divergence/tests/retained_cases.json
```

## Ordinary C workflow

`tests/compiler_checks.json` contains the full target, baseline and improved C
sources plus the mutation plan. `tests/smoke.py` materializes those inputs in a
new output directory, compiles them with the real compiler, assembles with maspsx
and independently scores the objects. It checks that:

1. An independent source probe reaches distance zero and satisfies its objective.
2. Bounded beam search finds an ordinary C candidate with that result.
3. Reduction removes an irrelevant declaration while retaining the source change
   needed for distance zero.
4. The original candidate remains unchanged.

This workflow passed with the bundled GCC 2.8.1 PSX compiler, SHA256
`60d886cd75bbd7855fc7909224a15401de76bff21af8a629c2060290a073f5fd`.
The fixture changes an incorrect subtraction to the target addition; it tests
execution and scoring, not whether mutation generation can infer semantics.
Choose any new output directory when rerunning:

```sh
python3 -B tools/divergence/tests/smoke.py --output /tmp/new-divergence-smoke
```

## Historical compiler interventions

During implementation, two interventions were measured on one nontrivial
matching candidate using the same bundled compiler:

| Controlled change | Baseline distance | Intervention distance | No-op control |
|---|---:|---:|---|
| Allocate pseudo 90 before 91 | 1440 | 1420 | Byte-identical assembly |
| Prefer UID 77 in the leading equal-priority sched1 group | 1440 | 1460 | Byte-identical assembly |

The allocation intervention reduced register penalties from 24 to 20. The
scheduler intervention increased them from 24 to 28. Other reported penalty
counts stayed the same. Neither was an ordinary source match or semantic proof.

These are historical observations, not bundled regression fixtures: their
original input, target object and raw logs lived in temporary directories and
are not included here. There are deliberately no replay commands or metadata
paths pointing to those directories. `tests/compiler_checks.json` now defines
the reproducible C smoke fixture described above, rather than those old runs.
The intervention commands in README.md can be used with a user's own retained
input and target; pseudo numbers and instruction UIDs must be selected from
that compilation.

## Remaining limits

The synthetic fixtures are not a representative matching benchmark. Historical
nontrivial measurements came from one function and do not establish universal
causal coverage or a guided-search speedup. Rules for other mechanisms have
source-definition checks and synthetic regressions, not a corpus of controlled
successful source fixes. The tool does not reconstruct every rejected combine
pattern, loop decision or delay-slot candidate. Those claims need additional
instrumentation and durable known-answer cases.

Automated concrete source generation covers simple operand-order edits and
adjacent constant field-store ordering. Other edits remain explicit proposals
for an agent or human to formulate before probing.
