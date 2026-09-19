# Divergence tools

Diagnose a candidate against its target, explain the relevant GCC 2.8.1
algorithms with source citations, propose C experiments, and test those
experiments against assembly and intermediate compiler objectives.

```sh
python3 tools/divergence/toolset.py diagnose SCRATCH base_1
python3 tools/divergence/toolset.py propose SCRATCH base_1
python3 tools/divergence/toolset.py compare SCRATCH base_1 base_2
python3 tools/divergence/toolset.py coverage
```

`SCRATCH` is a directory produced by the existing matching harness. Commands
accept `--help`. Diagnosis and comparison accept `--json`; diagnosis's
`--limit 0` prints every site. For files containing several functions, supply
`--function NAME`. A single function is selected automatically when possible.

## Prerequisite: fetch the GCC sources

Before using source citations, run this from the repository root:

```sh
bash tools/fetch_gcc_source.sh
```

This downloads GCC 2.8.1 and applies the project's PSX patches under
`local/gcc/gcc-2.8.1-psx`. That directory is generated, ignored by Git, and absent
from a fresh checkout. The bundled `cc1` executable does not initialize it.
The fetch script needs network access, Git, curl, tar, sed and patch; it fetches
source only and does not rebuild the compiler. Do not substitute stock GCC or
a different version when reasoning about this compiler's algorithms.

For an already initialized tree elsewhere, pass `--gcc-source PATH` to
`diagnose`. Reports check the expected source files and each cited definition.
If the tree is missing or incomplete, assembly/dump diagnosis still works, but
the report shows a setup instruction and marks unavailable citations with
`available: false` and `path: null`. `expected_path` is a lookup location, not a
verified citation. `coverage` also reports source availability. The source tree
must be initialized to check GCC definition lines; those checks are skipped
when it is absent. Basic regression tests and bundled fixture replay do not
require fetching GCC.

## What the report establishes

Each divergent site includes target/candidate instructions, alignment strength,
an emitted instruction UID when supported, RTL changes across passes, observed
allocation/order/operand facts, candidate mechanisms, and source-edit proposals.
Each mechanism names the relevant GCC functions. When the sources are fetched,
citations include file hashes and definition lines. Each edit has a prerequisite, expected
compiler effect, and a check that could falsify the hypothesis.

The report keeps three things distinct:

* **Observation:** something present in assembly, a dump, or an accepted trace.
* **Mechanism:** the algorithm in the inspected compiler source that may explain
  the observation. Routing by opcode or pass does not establish causality.
* **Experiment:** a proposed source change or isolated compiler intervention.
  Improvement must be measured, and the source's intended behavior preserved.

This covers all currently classified mismatch families. It is not a universal
inverse compiler: standard dumps omit some decision inputs, target RTL is not
available, and several source programs can produce the same instructions.
Unresolved links and choices remain explicit. A linked project match still
requires the ordinary build and its required verification.

## Coverage

| Difference or mechanism | Evidence and explanation | Proposed source experiments |
|---|---|---|
| Local/global allocation | Pseudo homes, conflicts, ranks; actual quantities/preferences with a trace | Operand order, shorter live ranges, expression sharing |
| Reload/spills/late CSE | Home changes, late expression changes; traced scratch reservations and substitutions | Lifetimes, sharing, access types |
| Scheduling | Relative order of surviving instructions; traced comparator, release, readiness and hazards | Independent statement order, definitions near uses |
| Branch delay slots | Nested `sequence` RTL and surrounding transfers | Equivalent exit shapes, independent computations |
| Instruction selection/combine | Expression evolution and emitted machine pattern | Operand order, typed expressions, sharing |
| Width/sign extension/constants | Memory modes, extensions, constants and target load requirements | Justified casts/access types; preserve values |
| CSE/redundant instructions | Expression changes, UID-specific compiler messages, late substitutions | Shared versus repeated expressions |
| Loops/addressing | Induction/address expressions, offsets and loop-pass changes | Equivalent pointer/index traversal, loop tests |
| Branches/control flow | Predicates, label references, validated cached object topology | Shared/separate exits, equivalent loop conditions |
| Calls/ABI | Call target/operands, typed call expansion, object diagnostics | Correct declarations, argument expressions and lifetimes |
| Stack/frame layout | Frame adjustments, stack stores, initial/late storage and allocation facts | Address-taking, local storage and register pressure |
| Assembler/hardware hazards | Emitted/object correspondence, expansion patterns, nop context | Independent instruction placement and materialisation |
| Symbols/relocations/integration | Symbol requirements and object identity; integration guidance | Correct field/data ownership and real header context |
| Unexplained insertions/deletions/mixed changes | Conservative routing to mechanisms and missing-evidence report | Form a controlled hypothesis before changing source |

`coverage` prints the machine-readable rule catalog, including unsupported
interventions. Detailed rejection reasons for combine, loop transformations or
every delay-slot candidate are not reconstructed from generic dumps. These
cases need additional instrumentation if existing observations are insufficient.

## Inputs and provenance

Minimum: `target_object_dump_normalized.s` and
`STEM_object_dump_normalized.s`. More evidence improves the diagnosis:

* `STEM.c`, `STEM.i`, annotated `STEM.s`, `STEM.score.json`.
* `STEM.i.rtl`, `.jump`, `.cse`, `.addressof`, `.loop`, `.cse2`, `.bp`, `.flow`,
  `.combine`, `.sched`, `.lreg`, `.greg`, `.sched2`, `.jump2`, `.dbr`.
* `STEM.diagnosis.json` from the existing object diagnostic tool. Its object
  hashes must match `target.o` and `STEM.o` before it is used.
* An optional `tools/trace_gcc.py` trace and `attempts.jsonl` build journal.

Exported report citations and manifest paths inside the repository are relative
to the repository root. Other paths inside the current home directory use `~/`
notation. Execution paths stay internal. Raw compiler logs, input files and
evidence archives are retained verbatim and may contain paths from the toolchain.

Generate dumps with the scratch's existing `dump.sh`; the build harness also
retains them for suitable near-misses. Fetch GCC sources as described in the
prerequisite above before following source citations. Source availability alone
does not prove how the prebuilt compiler was built.

An object instruction is linked to annotated assembly only through supported
canonical correspondences, including a small set of assembler expansions.
Other expansions and repeated instructions can be ambiguous. A missing link
is not automatically an assembler-generated instruction. Numeric literals are
not register names, and HI/LO and other hard registers below 76 are not pseudos.

UIDs are local to a function and compilation. `compare` establishes tentative
cross-build correspondences from unique expressions and neighboring context;
it does not assume equal UIDs mean equal values. It reports ambiguity.
Anonymous expression comparison is not proof of identical dataflow or semantics.

## Actual compiler decisions

The existing read-only tracer observes the pinned bundled compiler through GDB:

```sh
python3 tools/trace_gcc.py SCRATCH/base_1.i \
  --output-dir /tmp/trace-base-1 --function FUNCTION --uids 87
python3 tools/divergence/toolset.py diagnose SCRATCH base_1 \
  --trace /tmp/trace-base-1 --json
```

Use UIDs from coherent compiler artifacts. A trace is accepted only when its
input/compiler identity, no-op output check, baseline assembly hash, annotated
instruction/UID stream and selected function agree. Recorded compiler flags
are checked when a matching build journal exists; otherwise flags remain
unverified. Reports flag source/input/target mismatches against the build
journal, and experiment runners require a coherent baseline when a journal is
available. A fresh compile can have different UIDs from an old saved `.s`, even
when its object score agrees. Such a trace is rejected rather than misjoined.

Allocation observations distinguish printed initial conflicts from the
conflicts at a decision, and local pseudos from actual local quantities.
Scheduler comparator winners are distinguished from final hazard selection.
A greg-to-sched2 load replacement is not automatically called scheduling:
post-reload CSE runs in that interval too.

## Propose, probe and search

```sh
python3 tools/divergence/toolset.py propose SCRATCH base_1 --json
python3 tools/divergence/toolset.py propose SCRATCH base_1 --plan /tmp/edits.json
```

`propose` describes edits for every mechanism and maps them to source lines
when unambiguous. `--plan` additionally materializes supported simple integer
operand-order edits and adjacent constant stores to distinct fields as exact
before/after text. Store reordering requires review of volatility and field
layout, including unions and bitfields. It masks comments, strings and
directives and avoids arbitrary subexpression rewrites. Complex type, lifetime
and control-flow edits remain descriptions for a human/agent to formulate.
An empty generated plan means no supported concrete edit was found.

Review the semantic prerequisites, then supply exact mutations and objectives:

```json
{
  "constraints": [
    {"kind": "distance_le", "value": 10},
    {"kind": "instruction", "target_index": 37, "pattern": "^lhu a0,"}
  ],
  "mutations": [
    {
      "id": "commute_sum",
      "hypothesis": "Changing the operand order changes tying/preferences.",
      "edits": [{"before": "sum = left + right;", "after": "sum = right + left;"}]
    }
  ]
}
```

Every replacement must occur exactly once. Include surrounding context to
identify repeated statements. Each mutation must apply independently to the
baseline; search combines compatible mutations. Preconditions are review
requirements, not a semantic proof performed by these tools.

```sh
python3 tools/divergence/toolset.py probe SCRATCH base_1 /tmp/edits.json \
  --output /tmp/probes --budget 8 --timeout 60
python3 tools/divergence/toolset.py search SCRATCH base_1 /tmp/edits.json \
  --output /tmp/search --budget 12 --beam 4 --timeout 60
```

`probe` tests independent edits and compares their compiler effects. `search`
performs bounded beam search over combinations, keeping candidates that improve
assembly distance or satisfy intermediate constraints. Different source programs
with identical assembly remain eligible; identical source hashes are cached.
No claim of improved search speed is made without a measured corpus benchmark.

Both commands create uniquely named C candidates in the scratch and invoke its
existing `build.sh`, recording predictions through `attempt.py` when available.
They preserve its session limits. They do not overwrite the original candidate,
reset budgets, or promote a winner into the project. Failed builds and timeouts
consume the budget. The new output directory retains manifests, logs, hashes,
scores, constraints and reports. A successful process without output/score
artifacts is a failure. Inspect each experiment's `status`; completion of a
search command does not mean it found a match.

Constraints refer to **zero-based target instruction indices**, stable across
candidate trials, not candidate UIDs:

| Kind | Required fields | Check |
|---|---|---|
| `distance_le` | `value` | Independent scorer distance at most value |
| `family_count_le` | `family`, `value` | Number of diagnosed sites in family |
| `instruction` | `target_index`, `pattern` | Regex on corresponding candidate instruction |
| `rtl` | `target_index`, `pass`, `pattern` | Regex on linked candidate RTL expression |
| `allocation_before` | `before_target`, `after_target` | Global allocation order of uniquely linked destination pseudos |

Unlinked/missing state yields `unknown`, never a satisfied objective. A changing
family count can reflect alignment; use specific instruction/RTL constraints
and the independent score where possible. Local quantities cannot be constrained
by the global-order objective.

## Reduce a useful edit

```sh
python3 tools/divergence/toolset.py minimize SCRATCH base_1 base_2 \
  --output /tmp/reduced --budget 12 --constraints /tmp/constraints.json
```

The constraints file is a JSON list. The reducer first recompiles `base_2`, then
tries removing token edits while preserving its reproduced distance or better
and all requested objectives. It retains `reduced.c` and the experiment evidence.
It revisits removals after successful reductions. Budget exhaustion is explicit;
otherwise the result is minimal only against individual remaining edits under
this acceptance test, not globally minimal or semantically verified.

## Test a compiler counterfactual

`intervene` addresses the question “would this compiler decision help?” before
searching for C that reproduces it. It supports two audited hooks:

```sh
python3 tools/divergence/toolset.py intervene SCRATCH/base_2.i \
  --output /tmp/allocation-test --function FUNCTION \
  --kind global_order --before 90 --after 91 --target-object SCRATCH/target.o
python3 tools/divergence/toolset.py intervene SCRATCH/base_2.i \
  --output /tmp/schedule-test --function FUNCTION \
  --kind schedule_ready --uid 105 --pass sched1 --target-object SCRATCH/target.o
```

The first changes the global allocno permutation before preference pruning.
The second prefers a ready instruction within the leading equal-priority group;
it does not bypass dependency or hazard checks. IDs must belong to the newly
compiled input. A decision already satisfying the request is reported as
unchanged; a decision never eligible returns failure.

Every run retains an ordinary baseline, a no-op GDB control, and the modified
run. The control must produce byte-identical assembly before the modified run
is allowed. The tool pins the compiler hash, scopes writes to the named function,
keeps traces/dumps/output diffs, and optionally assembles and independently
scores both outputs. These hooks require GDB/ptrace and execution of the bundled
32-bit compiler, which some sandboxes prohibit.

This does **not** import arbitrary edited RTL or force hard-register assignments.
Those changes could violate compiler invariants. Successful experimental output
is evidence for a hypothesis, not a source match. Find ordinary C that reproduces
the improvement, then use the normal project build.

## Retention, retrieval and validation

```sh
python3 tools/divergence/toolset.py capture record --func FUNCTION --scratch SCRATCH
python3 tools/divergence/toolset.py retrieve SCRATCH --show-fixes
python3 tools/divergence/toolset.py replay tools/divergence/tests/retained_cases.json
python3 -B -m unittest discover -s tools/divergence/tests -v
python3 tools/divergence/toolset.py validate --scratch SCRATCH
python3 -B tools/divergence/tests/smoke.py --output /tmp/new-divergence-smoke
```

Capture stores unique session records and compressed source/input/assembly/
object/dump/journal bundles under `local/divergence/records`. When available it
uses the winner's recorded parent; otherwise the source comparison is labelled
as a closest-attempt comparison, not a causal pair. `matched` requires a retained
zero-distance score and matching source hash in the build journal; linked
integration remains unverified. Supplying `--matched-source` alone cannot assert
a match. Legacy give-up records remain readable. Retrieval ranks shape similarity
and optionally displays source diffs; it is not evidence that an edit generalizes.

The bundled `tests/retained_cases.json` replays hand-written inputs under
`tests/fixtures/`. `tests/compiler_checks.json` embeds the complete C sources and
mutation plan for the real-compiler smoke test. Neither JSON file references an
existing matching scratch or a previous temporary output directory. The smoke
test creates fresh outputs in the requested directory. These synthetic fixtures
are not captured evidence of successful game-function matches.

Replay of your own retained cases requires explicit assertions, for example:

```json
{"cases": [{
  "name": "retained allocation near-miss",
  "scratch": "relative/path/from/manifest",
  "stem": "base_1",
  "families": ["register"],
  "observations": ["allocation_snapshot"],
  "proposals": ["lifetime"]
}]}
```

Cases can additionally pin `identity` hashes, supply `trace` and `function`, check
`constraints`, or assert `no_divergences`. Proposed-edit coverage is separate
from proof that an edit works. Unit tests cover diagnostic false positives,
provenance rejection, correspondence, hook guardrails and experiment failures.
The opt-in smoke test builds temporary fixtures with the real compiler,
assembler and scorer and exercises probe, search and reduction. See
[VALIDATION.md](VALIDATION.md) for the measured checks and their limits.

The older `localize.py`, `explain.py`, `levers.py`, `progress.py`, `capture.py`,
`retrieve.py` and `validate.py` remain usable directly. `progress.py` uses the new
cross-build comparison; JSON consumers should check report schema versions.
