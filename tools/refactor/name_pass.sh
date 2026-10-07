#!/usr/bin/env bash
# Walk the naming worklist, handing each step to an agent.
#
#   ./tools/refactor/name_pass.sh [--times N] [--workers N] [--profile NAME]
#                                 [--dry-run] [--cli claude|grok|codex]
#                                 [--from ORDER] [--step ORDER]
#                                 [--kinds func,type,data,enum,macro]
#                                 [--batch N] [--batch-funcs N]
#                                 [--list-profiles] [--clean-workers]
#
# With no --times the whole worklist is walked. --times N stops after N rounds -
# a round being one fork-join cycle, which is one step per worker.
# --clean-workers removes the worker worktrees and exits, doing no work.
#
# --batch N sets how many pending types declared in one file the worklist joins
# into a step (default 8; 1 gives one type per step). Steps that declare items
# in the same file never share a round, so without it a header's types are
# worked one per round. --batch-funcs N is the same for functions (default 16),
# where the unit is a source file or all the fragments of one shared library,
# and a step may hold a function together with the callers that were waiting
# for it. Both apply when the worklist is next rebuilt.
#
# --kinds restricts the pass to steps holding an item of the listed kinds (the
# worklist's `kind` column: func, type, data, enum, macro). A cycle is one step, so it
# is worked whole when any of its items qualifies. The filter selects work, not
# dependencies: items of other kinds are left as they are, so a step can still
# reason from a neighbour that has not been named yet, and an item still in
# assembly stops the scan whatever its kind, as it always does.
#
# Profiles are the same ones the matching vacuum uses, from
# local/vacuum_profiles: a profile names the agent arm, the model, the
# reasoning effort and optionally a wrapper command to launch it through. An
# explicit --cli, or a VACUUM_MODEL / VACUUM_MATCH_EFFORT already exported,
# still wins over the profile.
#
# A step is one line of local/worklist.tsv, or several lines sharing an order
# when a cycle means the items have to be understood together. For each step
# this builds a brief, runs the agent against a tree, validates its review and
# both build modes, and commits changes. A step needs the whole tree in front
# of it and produces no candidate to throw away, so there is no scratch
# environment of the kind the matching vacuum builds.
#
# A missing review, failed build, declaration/symbol conflict or individual
# objdiff mismatch rejects the step. Unresolved analysis is preserved separately
# from accepted source changes in local/name-pass/reviews.
#
# Ctrl-C stops the pass between rounds, as it does in the matching vacuum. A
# parallel round finishes and lands first; a single-worker step is interrupted
# with the driver and recorded as not landed. The worker trees are reset either
# way, so the next run resumes from the ledger with nothing to clean up by hand.
#
# --workers N runs N steps at a time, fork-join. Each worker owns one worktree
# for the whole run: a round assigns it one step, it works and commits there,
# and afterwards every worker's commit is replayed onto the driver's branch and
# the joined tree is verified once. A replay that conflicts, or a joined tree
# that no longer builds, is handed to a landing agent that has both sides in
# front of it - the same division of labour the overlay sweep uses. The workers
# are then reset to the landed state, so each round starts from one tree again.
#
# --queue K lets a worker that finishes early take another step of the same
# round instead of waiting for the slowest. The round's steps are chosen once,
# when it starts - up to K per worker, all ready and none overlapping another -
# and since nothing lands before the join, none of them can stop being ready or
# become ready while the round runs: the queue is simply worked off. A worker
# takes one step, and on finishing asks for the next. It is given one until
# every worker has finished its first, and after that it is told to stop; the
# steps still queued stay in the worklist for the next round. A second step is
# worked in the tree that holds the worker's first commit, and the join replays
# every commit in the order the steps were started. The join, the verification
# and the worklist rebuild happen once, when all workers have stopped. K = 1 is
# one step per worker, as before; the default is 3.
#
# Only steps that do not wait on each other share a round. The worklist's
# `after` column gives the last step each one depends on, and a round is
# widened only while that stays behind the round's first step; the dependency
# order is the premise of the pass, so an item is never worked while something
# it uses is being worked beside it. Nor do steps share a round when they are
# declared in the same file or used mostly in the same files: their workers
# would review and rewrite the same code. How much parallelism that allows is a
# property of the graph, not of N: the front of this worklist is wide at first
# and narrows, so beyond a handful of workers the extra ones mostly idle. Each
# worker also owns a full copy of the generated trees and runs its own build, so
# N is bounded by disk and cores as much as by the graph.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

WORKLIST="local/worklist.tsv"
# Which steps are finished. The worklist is a plan, not a record: an item whose
# name already follows the convention keeps that name afterwards, so "is the
# name still in the tree" cannot say whether it has been done. Every outcome is
# appended here instead. `ok` completes the review; `followup` finishes this
# visit with unresolved work preserved in its JSON report. Failed steps retry.
DONE_LEDGER="local/name_pass_done.tsv"
# The standing job description. grok takes it as a system prompt via --rules,
# where it frames the whole session; the other arms get the same text inlined at
# the head of the brief. One source either way, so the two cannot drift.
RULES=".grok/rules/name-pass.md"
CLI="${VACUUM_CLI:-claude}"
CLI_EXPLICIT=0
PROFILE="${PROFILE-${VACUUM_PROFILE:-}}"
# Rounds, not steps: a round is the unit the driver actually executes, so a
# limit that cut across one would leave workers idle for no reason. Zero means
# the whole worklist - a driver told to walk a list has no reason to stop after
# the first round, and a default of one made the common invocation look like it
# had run out of work.
TIMES=0
WORKERS=1
QUEUE="${PE2_NAME_QUEUE:-3}"
# Beside the repository by default, the way the matching vacuum places its
# worktrees, so a checkout is not nested inside another one.
WORKER_ROOT="${NAME_PASS_WORKTREE_ROOT:-$(dirname "$ROOT")}"
CLEAN_WORKERS=0
DRY=0
FROM=0
ONLY=""
KINDS=""
LIST_PROFILES=0
KEEP_GOING=0
# How often a session that exits non-zero is started again before its step is
# failed and reverted (work_step).
SESSION_RETRIES="${PE2_NAME_SESSION_RETRIES:-2}"
RESUME_NOTE="NOTE: an earlier session on this step was interrupted before it finished (the provider or the
connection failed; nothing was wrong with the step). Its uncommitted edits are still in this worktree, and its
review report may be partly filled in. Start by reading \`git status\` and \`git diff\` to see what was done,
check that work instead of assuming it is right, and continue from there. The task itself is unchanged:"
REFRESH=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --times) TIMES="$2"; shift 2 ;;
    --queue) QUEUE="$2"; shift 2 ;;
    --workers|-j) WORKERS="$2"; shift 2 ;;
    --batch) export PE2_NAME_BATCH="$2"; shift 2 ;;
    --batch-funcs) export PE2_NAME_BATCH_FUNCS="$2"; shift 2 ;;
    --worktree-root) WORKER_ROOT="$2"; shift 2 ;;
    --clean-workers) CLEAN_WORKERS=1; shift ;;
    --cli)   CLI="$2"; CLI_EXPLICIT=1; shift 2 ;;
    --claude) CLI=claude; CLI_EXPLICIT=1; shift ;;
    --grok)  CLI=grok; CLI_EXPLICIT=1; shift ;;
    --codex) CLI=codex; CLI_EXPLICIT=1; shift ;;
    --profile) PROFILE="$2"; shift 2 ;;
    --profiles) export VACUUM_PROFILES_FILE="$2"; shift 2 ;;
    --list-profiles) LIST_PROFILES=1; shift ;;
    --from)  FROM="$2"; shift 2 ;;
    --step)  ONLY="$2"; shift 2 ;;
    --kinds) KINDS="$2"; shift 2 ;;
    --dry-run) DRY=1; shift ;;
    --all) TIMES=0; KEEP_GOING=1; shift ;;
    --keep-going) KEEP_GOING=1; shift ;;
    --no-refresh) REFRESH=0; shift ;;
    --stop-on-fail) KEEP_GOING=0; shift ;;
    -h|--help) awk 'NR > 1 && /^#/ { print; next } NR > 1 { exit }' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

# The same table and resolution rules the matching vacuum uses, so a profile
# means one thing across the project rather than two.
# shellcheck disable=SC1091
. "$ROOT/tools/vacuum_profile.sh"
if (( LIST_PROFILES )); then list_profiles; exit 0; fi
[[ -n "$PROFILE" ]] && apply_profile "$PROFILE"
agent_launch "$CLI" "${VACUUM_MODEL:-}"
LAUNCH_CMD=("${AGENT_CMD[@]}")
MODEL="${AGENT_MODEL:-}"
EFFORT="${VACUUM_MATCH_EFFORT:-}"

[[ -f "$WORKLIST" ]] || {
  echo "no $WORKLIST; run: venv/bin/python3 tools/refactor/dep_graph.py worklist" >&2
  exit 1
}

# Tab is whitespace as far as IFS is concerned, so `read` collapses a run of
# them and an empty column silently shifts every later field left. Feed the
# worklist through awk first so no column is ever empty.
rows() {
  awk -F'\t' -v OFS='\t' '{ for (i = 1; i <= NF; i++) if ($i == "") $i = "-"; print }' "$WORKLIST"
}

# An item is still outstanding if its current name is anywhere in the sources.
# The worklist is a snapshot, so this is checked afresh rather than trusted.
outstanding() {
  if [[ "$1" == */* ]]; then
    venv/bin/python3 tools/refactor/macro_refs.py "$1" --exists
    return
  fi
  grep -rqlw --include='*.c' --include='*.h' -- "$1" src include 2>/dev/null
}

# Keyed on the item name, not the step order: rebuilding the graph renumbers
# every step, so an order is only meaningful within one worklist.
ledgered() {
  [[ -f "$DONE_LEDGER" ]] || return 1
  awk -F'\t' '$4=="ok" || $4=="followup"{print $2}' "$DONE_LEDGER" | tr ' ' '\n' | grep -qx -- "$1"
}

# order, items, commit (or -), outcome, optional structured review path.
record() {
  printf '%s\t%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" "${5:-}" >> "$DONE_LEDGER"
}

review_file() { echo "local/name-pass/reviews/$RUN_ID-r$i-step$1.json"; }

# Items this run has already attempted without landing them. The ledger cannot
# serve here: it records failures as well as accepted visits, so
# without this a --keep-going run hands the same step to the next round for
# ever, against the same tree it just failed against.
SKIP_NAMES=" "

# The orders to work next, at most $WORKERS * $QUEUE of them, one per line: the
# first $WORKERS start the round and the rest are its queue.
#
# The first is the lowest outstanding step, as it always was. Each further one
# is a candidate only if everything it depends on comes before that first step,
# which is what the `after` column says: dependencies always precede their users
# in the worklist, so `after < first` means none of a step's dependencies is in
# the round. Steps already in the round cannot depend on a later one for the
# same reason, so the set is mutually independent in both directions.
#
# Independence is not enough for steps to be worked side by side. Two steps
# declared in one header, or used mostly in the same files, are reviewed through
# the same code, and their workers rewrite it twice over and conflict at the
# join. So the candidates go through `dep_graph.py round`, which keeps them in
# order and holds back any that overlap a step already kept. If that check
# cannot run, the round is the first step alone: slower, never unsafe.
#
# A barrier - an item still in assembly - ends the scan rather than being
# skipped. The steps behind it are the ones that use it, so working them would
# reason from a name that cannot be established yet.
next_batch() {
  local order name kind state after first="" last="" scanned=0 barrier=""
  local -a cands=()
  while IFS=$'\t' read -r order _ name kind _ state _ _ after; do
    [[ "$order" == "order" ]] && continue
    (( order < FROM )) && continue
    [[ -n "$ONLY" && "$order" != "$ONLY" ]] && continue
    # A cycle is several rows sharing an order, and it is one step: the first
    # row that qualifies takes the whole order, and the rest are its siblings.
    [[ "$order" == "$last" ]] && continue
    # The kind filter comes before the ledger and tree checks, which cost a grep
    # each, but lets an item still in assembly through: it is a barrier whatever
    # its kind, and the test below stops the scan at it.
    if [[ -n "$KINDS" && ",$KINDS," != *",$kind,"* && "$state" != "generated" ]]; then
      continue
    fi
    ledgered "$name" && continue
    [[ "$SKIP_NAMES" == *" $name "* ]] && continue
    outstanding "$name" || continue
    if [[ -n "$first" ]]; then
      # Looking further costs a tree-wide grep per candidate, and the answer
      # stops improving quickly once the front narrows.
      # Counted in steps: a batched step is several rows, and counting those
      # spent the whole window on the first few dozen steps.
      (( ++scanned > 40 * WORKERS )) && break
      (( after < first )) || { last="$order"; continue; }
    fi
    if [[ "$state" == "generated" ]]; then
      # Reported by the caller, which stops the pass: nothing behind a barrier
      # is workable, whether or not this round found work in front of it. It
      # travels on stdout because this function is read through a pipe, which
      # puts it in a subshell whose variables the caller never sees.
      barrier="$(printf 'barrier\t%s\t%s' "$order" "$name")"
      break
    fi
    cands+=("$order")
    last="$order"
    [[ -n "$first" ]] || first="$order"
    # Overlap discards many candidates, so gather several per worker.
    (( ${#cands[@]} >= 8 * WORKERS )) && break
  done < <(rows)
  if (( ${#cands[@]} > 1 && WORKERS > 1 )); then
    printf '%s\n' "${cands[@]}" \
      | venv/bin/python3 tools/refactor/dep_graph.py round --workers "$(( WORKERS * (QUEUE > 0 ? QUEUE : 1) ))" \
          --worklist "$WORKLIST" 2>>"$LOG" \
      || echo "${cands[0]}"
  elif (( ${#cands[@]} > 0 )); then
    echo "${cands[0]}"
  fi
  [[ -z "$barrier" ]] || echo "$barrier"
  (( ${#cands[@]} > 0 ))
}

build_brief() {
  local order="$1" names=() line
  while IFS=$'\t' read -r o _ name kind vis state file refs _; do
    [[ "$o" == "$order" ]] || continue
    names+=("$name")
    line+="
## $name
kind: $kind   visibility: $vis   current state: $state
declared in: $([[ "$file" == "-" ]] && echo "not defined in C (assembly or a header only)" || echo "$file")
referrers: $refs
"
    if [[ "$file" != "-" ]]; then
      # One query answers for the item and for everything it contains, so the
      # brief carries both: the per-member counts, which are short and say at a
      # glance which members are live, and the item's own sites, which are the
      # part that can run long. `refs` is already the referrer count read from
      # the row, so the capture uses names of its own.
      local fr_out fr_members fr_sites spec="$file/$name"
      [[ "$kind" == "macro" ]] && spec="$name"
      fr_out=$(timeout 600 venv/bin/python3 tools/refactor/find_references.py \
                 "$spec" -q 2>/dev/null)
      fr_members=$(printf '%s\n' "$fr_out" | sed -n '/^# [0-9][0-9]* symbol/,/^$/p')
      fr_sites=$(printf '%s\n' "$fr_out" | sed '/^# [0-9][0-9]* symbol/,/^$/d' | tail -40)
      # Retain definitions, conditional branches and ambiguous binding details.
      [[ "$kind" == "macro" ]] && { fr_members=""; fr_sites="$fr_out"; }
      line+="
references (how each use reads or writes it):
\`\`\`
${fr_members}${fr_sites}
\`\`\`
"
    fi
  done < <(rows)

  cat <<EOF
$( [[ "$CLI" != "grok" && -f "$RULES" ]] && cat "$RULES" )

# Naming pass, step $order

Process ${#names[@]} item(s) together: ${names[*]}
$( ((${#names[@]} > 1)) && echo "
These items are one step. Either they form a cycle in the dependency graph, each
using the others; or they are one embedded asset - its record and the arrays
only that record reaches; or they are types or functions of the same file, or
of one shared library's fragments, joined because steps in one file cannot run
side by side and because what you learn about the file serves all of them. A
cycle or an asset is understood together. Items that merely share a file are
not: review each on its own evidence, give each its own entry in the review,
and do not let one item's conclusion stand in for another's. Where one item of
the step uses another - a function and the function it calls - settle the one
that is used first, so the other is described in terms of its final name." )
$line

## What to do with this item

The conventions, the compiler's limits and what counts as evidence are above
(and in NAMING.md); this is what is specific to this step.

1. Derive what the item is from the references listed above and the code they
   sit in. The reference listing marks reads, writes, casts, address-taken and
   mentions in prose. Investigate what each cast represents before deciding
   whether it indicates an incorrect type.
2. Establish the owning subsystem using NAMING.md's ownership guide and the
   item's interface and consumers. Gameplay has no blanket \`gp\` prefix;
   use \`cap\`, \`inventory\`, \`actorRender\`, etc. as the evidence warrants.
   Shared implementations keep their subsystem identity; package wrappers use
   their package identity. A file can contain several subsystems.
   Macros use UPPER_SNAKE_CASE with full, unshortened subsystem prefixes where
   needed; generic helpers such as ARRAY_SIZE and PARENT_OF need no prefix.
   Established gte_* macros retain PsyQ spelling. Macro names carry no private
   underscore or global g marker. Review replacements, arguments, captures,
   conditional definitions and configuration bindings across all carriers.
   If its state above is \`current\`, only its spelling has been classified.
   Reassess ownership even for existing \`gp...\` names. Keep an established
   name unless the evidence establishes a misleading meaning or ownership.
   Review its type, declaration, fields, parameters, locals and documentation.
   Rename with the tool so its alias ledger preserves the item's history:
     venv/bin/python3 tools/refactor/rename_item.py <file>/<oldName> <newName> --sidecars
3. Apply the same to what the item contains: its fields, and its parameters in
   both the prototype and the definition. Rename those without \`--sidecars\`,
   which the tool refuses for a field or parameter (in a \`--batch\` it skips
   it for those lines). The reference listing above already
   covers them - one line per member, with the counts that say which are live -
   so read that rather than querying each one, and open the file it names when
   you need a member's individual sites.
4. Verify the suggested visibility against actual consumers, shared-source
   carriers, variants and imports. TU-local declarations belong in the source;
   overlay-shared ones in private headers; cross-overlay ones in public headers.
   Apply \`static\` where appropriate and preserve BSS declaration ordering.
   Macros have preprocessor scope, not C linkage: do not apply static to them.
5. Where the role genuinely cannot be established, leave the name and say so.
   An invented name is worse than a generated one.
6. Where another image refers to the item - a symbol-map line for its name or
   its address marked \`absolute:True\`, which \`grep -rn "<name>\\|0x<ADDR>"
   configs/USA\` finds - check that reference as part of the item, because the
   build never does: each image links against an address. Its declaration in
   the referring image must have the definition's type, and it must carry the
   definition's name. A reference into the main executable or into gameplay
   needs no annotation: only one image is ever at that address. One into a
   slot that several overlays load into carries \`owner=\`, which must be the
   image whose object the referring code really means (read the use site: a
   table's key, the resources the referrer loads, the type it expects); do not
   add \`owner=\` anywhere else. Where one is wrong, correct it - the
   annotation, the declaration, or the name on both sides - and say so in the
   review. A place several images define is \`shared=\`; a function or object
   another image refers to cannot be \`static\`. \`venv/bin/python3
   tools/check_symbols.py --image <package>\` and \`tools/refactor/check_decls.py
   --across-images\` report what disagrees.
7. Resolve meaningful literals, proven sizes and array bounds; review pointer
   and callback casts, contracts and bounds; simplify justified scaffolding.
   Add sparse coarse body comments explaining significant phases and constraints.
   Propagate findings to all consumers and remove redundant declarations/includes.

## Prior audit findings

$(venv/bin/python3 tools/refactor/name_review.py context "${names[@]}")

## Finishing

Do not add a pinned register, an \`asm\` statement or a steering macro
(\`SOFT_TOUCH_REG\`, \`SOFT_USE_REG\`, or a deleted one such as \`TOUCH_REG\`,
\`USE_REG\` or \`SOFT_BARRIER\` defined again) to keep a cleanup matching: the
worker's verification fails a step that adds one. If a change stops a function
matching, put that part back as it matched and record it as a \`rematching\`
follow-up (NAMING.md, "Naming-pass acceptance and follow-ups").

Run \`./tools/build-and-verify.sh\` until it passes: it rebuilds what you
changed, checks every image's checksum and that the symbol maps name every C
function. Do not run \`tools/refactor/verify_name_pass.py\` yourself; the
worker runs it after your session (the build again, plus the declaration and
symbol-map checks) and decides whether the step lands. Do not commit; the
driver commits.

Fill \`$(review_file "$order")\` (a template is created before your session).
Keep each entry's \`name\` as assigned; set \`current_name\`, \`meaning\`,
\`evidence\` (nonempty string list), \`changes\` (string list), and \`unresolved\`.
Use \`outcome: complete\` with an empty unresolved list, or \`outcome: followup\`
with issues of the form:
\`{ "kind": "rematching|runtime|semantics|unrelated", "location": "source:symbol", "reason": "...", "next_step": "...", "id": "existing audit ID when available" }\`.
Choose one kind per issue. Keep existing audit IDs and explain attempted fixes.
If the item should not exist - a duplicate merged into another type, a
scaffold replaced by real declarations - delete it, set \`"removed": true\` on its
entry in place of a \`current_name\`, and say in \`changes\` what replaced it;
the name must then be gone from \`src/\` and \`include/\`.
A reviewed item needing no code change still requires evidence. Outstanding work
must remain in the report even when its rename and other cleanup are successful.
$( (( WORKERS > 1 )) && echo "
Another step is being worked at the same time, in a sibling checkout of the same
repository, on an item that does not depend on yours. So stay inside the
directory you were started in - do not read or edit a sibling checkout, and do
not reach for git history or branches to see what it is doing. Its changes reach
you when the driver lands them, not before.")

Report what the item turned out to be, what you changed beyond the name, and
anything you could not make match.
EOF
}

# Work from a snapshot. A step is identified by an order number that only means
# anything within one worklist, and rebuilding the graph renumbers every one of
# them - so a regeneration partway through a run would hand the loop an order
# resolved against the old list and items read from the new one. That silently
# split a dependency cycle across two steps once; the items have to come from
# the same list the order did.
SNAPSHOT="$(mktemp -t name_pass_worklist.XXXXXX)"
SOURCE_WORKLIST="$WORKLIST"
cp "$WORKLIST" "$SNAPSHOT"
trap 'rm -f "$SNAPSHOT"' EXIT
WORKLIST="$SNAPSHOT"

# Ctrl-C is the driver's to handle: it asks the pass to stop after the round in
# flight, and nothing else may hear it. The terminal sends the interrupt to the
# whole foreground process group, though, which is every foreground child too.
# Parallel workers were safe by accident - a non-interactive shell starts an
# asynchronous child with the interrupt ignored - but the landing agent, a
# verification and a single-worker step all run in the foreground: one Ctrl-C
# during a join killed the landing agent, and the round fell back to landing
# its steps one at a time. So every long-running child is started through
# `shielded`, in a session of its own, where the terminal's signal does not
# reach it. The round finishes and lands as usual, the bookkeeping runs, and
# the pass stops; the next run resumes from the ledger.
#
# The handler stays armed, so a second Ctrl-C only repeats the message. To
# abandon a round outright, send the driver SIGTERM (or Ctrl-\) - its shielded
# children then have to be stopped by hand.
STOP_REQUESTED=0
trap 'echo ""; echo "Interrupt received; stopping after this round (kill -TERM $$ to abandon it)."; STOP_REQUESTED=1; [[ -z "${QDIR:-}" ]] || : >"$QDIR/stop"' INT
shielded() {
  if command -v setsid >/dev/null 2>&1; then
    setsid -w "$@"
  else
    ( trap '' INT; exec "$@" )
  fi
}

# A step is an analysis, not just a rename of its own item: it merges a
# duplicate type away, retypes a caller, renames a neighbouring field. That
# changes which items remain and what depends on what, so the plan the next
# step reads has to be rebuilt rather than carried forward. The graph is read
# from the reference index, which re-scans only what the round changed, so this
# is seconds against a step of several minutes. --no-refresh keeps the original
# snapshot for the whole run.
refresh_worklist() {
  (( REFRESH )) || return 0
  echo "--- rebuilding the graph and worklist" | tee -a "$LOG"
  if venv/bin/python3 tools/refactor/dep_graph.py --build >/dev/null 2>&1 \
     && venv/bin/python3 tools/refactor/dep_graph.py worklist >/dev/null 2>&1; then
    cp "$SOURCE_WORKLIST" "$SNAPSHOT"
  else
    echo "    regeneration failed; continuing on the previous worklist" | tee -a "$LOG"
  fi
}

LOG="$(vacuum_log_dir)/name_pass-$$.log"

# Tell the worklist which kinds this run works, so a pending item of another
# kind does not hold back the steps that use it (dep_graph.run_kinds).
if [[ -n "$KINDS" ]]; then
  echo "$KINDS $$" > local/name_pass_kinds
else
  rm -f local/name_pass_kinds
fi
# What this run started from, so the closing audit runs only when it landed work.
RUN_START="$(git rev-parse HEAD)"
RUN_ID="$(date -u '+%Y%m%dT%H%M%SZ')-$$"
echo "logging to $LOG"

# Paths a worktree needs but must never commit: the generated trees and the
# machine-local directory are gitignored, and a submodule that had to be linked
# rather than checked out shows up as a type change. Both the dirty-tree test
# and the commit exclude them, so scaffolding cannot ride along on a rename.
SCAFFOLD_PATHS=(venv .venv assets rom local
                tools/maspsx tools/m2c tools/asm-differ tools/decomp-permuter)
scaffold_re="^($(IFS='|'; echo "${SCAFFOLD_PATHS[*]//./\\.}"))(/|$)"

tree_changes() {
  git -C "$1" status --porcelain | sed 's/^...//' | grep -vE "$scaffold_re" || true
}

# --- the agent arm ------------------------------------------------------------
# One place that knows how to launch each CLI, because a step and a landing
# differ only in their brief and their tree. The tree is given as a directory
# rather than assumed to be the driver's, which is what lets a worker run.
run_agent() {
  local dir="$1" brief="$2" log="$3" term="$4"
  local -a cmd
  local stream=0 formatter=tools/stream_format.py rc
  # The brief never travels as an argument: Linux caps a single argument at
  # 128 KB, and a widely used item's brief passed that, so the agent could not
  # even be launched. Each CLI reads it from a file or standard input instead.
  local brief_file
  brief_file="$(mktemp -t name_pass_brief.XXXXXX)"
  printf '%s\n' "$brief" >"$brief_file"
  case "$CLI" in
    claude)
      # Plain `claude -p` prints only the final result, so a step looks frozen
      # for as long as it runs - which at high effort on a widely-used item is
      # many minutes of silence. Stream the events and format them, the way the
      # matching vacuum does. VACUUM_STREAM=0 restores the quiet form.
      cmd=("${LAUNCH_CMD[@]}" -p ${MODEL:+--model "$MODEL"} ${EFFORT:+--effort "$EFFORT"})
      if [[ "${VACUUM_STREAM:-1}" != "0" ]]; then
        cmd+=(--verbose --output-format stream-json); stream=1
      fi
      cmd+=(--dangerously-skip-permissions)
      ;;
    grok)
      # grok emits the same NDJSON wire format on request, so the one formatter
      # renders both arms and a grok step reports its cost and turn count the
      # way a claude step does.
      cmd=("${LAUNCH_CMD[@]}" --always-approve ${MODEL:+-m "$MODEL"}
           ${EFFORT:+--effort "$EFFORT"}
           --cwd "$dir" ${RULES:+--rules "$ROOT/$RULES"})
      if [[ "${VACUUM_STREAM:-1}" != "0" ]]; then
        cmd+=(--output-format streaming-messages-json --include-partial-messages)
        stream=1
      fi
      cmd+=(--prompt-file "$brief_file")
      ;;
    codex)
      # Plain `codex exec` writes the worktree's cumulative diff after every
      # event, and a step touching a long file grew one worker's log to 850 MB.
      # Its JSON events go through the formatter the matching vacuum uses.
      cmd=("${LAUNCH_CMD[@]}" exec --dangerously-bypass-approvals-and-sandbox
           ${MODEL:+--model "$MODEL"} --cd "$dir")
      if [[ "${VACUUM_STREAM:-1}" != "0" ]]; then
        cmd+=(--json); stream=1; formatter=tools/codex_format.py
      fi
      cmd+=(-)
      ;;
    *) echo "unknown api: $CLI" >&2; rm -f "$brief_file"; return 2 ;;
  esac
  # The CLI's own error output goes to the step's log too: a launch failure
  # prints nothing else, and on the terminal alone it left the log silent.
  if (( stream )); then
    if (( term )); then
      ( cd "$dir" && shielded "${cmd[@]}" ) <"$brief_file" 2> >(shielded tee -a "$log" >&2) \
        | shielded python3 "$formatter" ${VACUUM_STREAM_QUIET:+--quiet-text} \
        | shielded tee -a "$log"
    else
      ( cd "$dir" && shielded "${cmd[@]}" ) <"$brief_file" 2>>"$log" \
        | shielded python3 "$formatter" ${VACUUM_STREAM_QUIET:+--quiet-text} \
        >>"$log" 2>&1
    fi
  elif (( term )); then
    ( cd "$dir" && shielded "${cmd[@]}" ) <"$brief_file" 2> >(shielded tee -a "$log" >&2) | shielded tee -a "$log"
  else
    ( cd "$dir" && shielded "${cmd[@]}" ) <"$brief_file" >>"$log" 2>&1
  fi
  rc=$?
  rm -f "$brief_file"
  return "$rc"
}

# --- one step, in one tree ----------------------------------------------------
# The agent, the build, the commit. Which tree it is does not matter here: the
# driver's own when the pass is serial, a worker's when it is not. The outcome
# goes to a file because in parallel this runs in a background subshell, whose
# variables the driver never sees.
work_step() {
  local dir="$1" order="$2" items="$3" brief="$4" log="$5" status="$6" term="$7"
  local report outcome
  # Every build and refactor tool defaults to all CPUs. Workers run them at
  # once, and each one's splat pool alone averages ~110 MB a process, so a full
  # pool per worker exhausted memory. Divide the CPUs among the workers
  # instead; the agent and the verifier inherit this.
  export PE2_JOBS="${PE2_JOBS:-$(( $(nproc) / WORKERS > 0 ? $(nproc) / WORKERS : 1 ))}"
  local -a names
  read -ra names <<<"$items"
  report="$(review_file "$order")"
  if ! ( cd "$dir" && venv/bin/python3 tools/refactor/name_review.py init --report "$report" "${names[@]}" ); then
    echo failed >"$status"
    return 0
  fi
  # Say which stage failed. The build log is removed first so that a failure
  # before verification cannot show an earlier step's passing build as its own.
  local why="" rc
  rm -f "$log.build"
  run_agent "$dir" "$brief" "$log" "$term"; rc=$?
  # A session that exits non-zero did not finish: the provider was at capacity,
  # the connection dropped, the CLI crashed. None of that says anything about
  # the step, and each CLI reports it in its own words, so the exit status is
  # the only signal used. The session is started again on the tree as it was
  # left, since the edits made so far are most of the cost and verification
  # gates the result either way. A report that does not validate or a failed
  # verification is a finished session whose work was wrong, and is not retried.
  # Status 2 is this script refusing the CLI, 130 and 143 an interrupt.
  local attempt=0 pause
  while (( rc && rc != 2 && rc != 130 && rc != 143 && attempt < SESSION_RETRIES )); do
    attempt=$((attempt + 1))
    pause=$(( attempt == 1 ? 60 : 300 ))
    echo "step $order: the agent session exited with status $rc; retry $attempt of $SESSION_RETRIES in ${pause}s" \
      | tee -a "$log" >&2
    sleep "$pause"
    run_agent "$dir" "$RESUME_NOTE

$brief" "$log" "$term"; rc=$?
  done
  if (( rc )); then
    why="the agent session exited with status $rc"
  elif ! outcome=$(cd "$dir" && venv/bin/python3 tools/refactor/name_review.py validate \
                     --report "$report" "${names[@]}" 2>>"$log"); then
    why="its review report did not validate"
  elif ! ( cd "$dir" && shielded venv/bin/python3 tools/refactor/verify_name_pass.py ) >"$log.build" 2>&1; then
    why="verification failed"
  fi
  if [[ -n "$why" ]]; then
    {
      echo "step $order FAILED: $why ($dir); reverting"
      [[ -f "$log.build" ]] && tail -20 "$log.build"
    } | tee -a "$log" >&2
    git -C "$dir" checkout -- . 2>/dev/null
    git -C "$dir" clean -fd src include configs >/dev/null 2>&1
    echo failed >"$status"
    return 0
  fi
  if [[ -z "$(tree_changes "$dir")" ]]; then
    echo "step $order reviewed without source changes ($outcome)" | tee -a "$log"
    echo "$outcome -" >"$status"
    return 0
  fi
  git -C "$dir" add -A
  # A linked submodule or a generated tree that slipped past .gitignore would
  # otherwise be committed by `add -A` and then replayed onto the branch.
  git -C "$dir" reset -q -- "${SCAFFOLD_PATHS[@]}" 2>/dev/null
  if ! git -C "$dir" commit -q -m "naming: $items"; then
    echo failed >"$status"
    return 0
  fi
  printf '%s %s\n' "$outcome" "$(git -C "$dir" rev-parse HEAD)" >"$status"
}

# --- the round's queue ----------------------------------------------------------
# $QDIR holds the round's state, shared by the worker subshells: `queue` (orders
# not yet started, one per line), `started` (orders in the order they were
# taken, which is the order the join replays them in), `worker.<order>`,
# `status.<order>`, `first.<w>` once worker w has finished a step, and `stop`
# when the driver was interrupted. $QACTIVE is how many workers the round has.
queue_take() {
  local w="$1"
  (
    flock 9
    if [[ -e "$QDIR/first.$w" ]]; then
      [[ ! -e "$QDIR/stop" ]] || exit 0
      # Every worker has finished its first step: the round is closing.
      (( $(find "$QDIR" -maxdepth 1 -name 'first.*' | wc -l) < QACTIVE )) || exit 0
    fi
    next="$(head -n 1 "$QDIR/queue" 2>/dev/null)"
    [[ -n "$next" ]] || exit 0
    sed -i 1d "$QDIR/queue"
    echo "$w" >"$QDIR/worker.$next"
    echo "$next" >>"$QDIR/started"
    echo "$next"
  ) 9>"$QDIR/lock"
}

worker_loop() {
  local w="$1" order wlog="${LOG%.log}-w$1.log"
  while order="$(queue_take "$w")" && [[ -n "$order" ]]; do
    echo "--- worker $w: step $order (${step_items[$order]}) -> $wlog" | tee -a "$LOG"
    work_step "$(worker_dir "$w")" "$order" "${step_items[$order]}" \
              "$(build_brief "$order")" "$wlog" "$QDIR/status.$order" 0
    ( flock 9; : >"$QDIR/first.$w" ) 9>"$QDIR/lock"
  done
}

# --- worker worktrees ---------------------------------------------------------
# One worktree per worker for the whole run, so the cost of populating it is
# paid once rather than per step. The heavy generated trees are copied, not
# shared: a build re-splits asm/ and rewrites build/, so two workers pointed at
# one copy would overwrite each other's inputs. Everything read-only is linked.
worker_dir()    { echo "$WORKER_ROOT/pe2-name-w$1"; }
worker_branch() { echo "name-pass/w$1"; }

remove_worker() {
  local wt; wt="$(worker_dir "$1")"
  [[ -e "$wt" ]] || return 0
  git -C "$ROOT" worktree remove --force "$wt" >/dev/null 2>&1 || rm -rf "$wt"
  git -C "$ROOT" branch -D "$(worker_branch "$1")" >/dev/null 2>&1 || true
}

create_worker() {
  local i="$1" wt branch rel
  wt="$(worker_dir "$i")"; branch="$(worker_branch "$i")"
  if [[ -d "$wt/.git" || -f "$wt/.git" ]]; then
    echo "--- worker $i reusing $wt"
    return 0
  fi
  echo "--- worker $i creating $wt (copying the generated trees; this is once per run)"
  rm -rf "$wt"
  git -C "$ROOT" branch -D "$branch" >/dev/null 2>&1 || true
  git -C "$ROOT" worktree add -f -b "$branch" "$wt" HEAD >/dev/null || return 1
  # git worktree add does not check out submodule contents, and the build needs
  # maspsx. A checkout keeps the tree clean; a link is the fallback, and is why
  # the commit excludes these paths.
  git -C "$wt" submodule update --init --recursive >/dev/null 2>&1 || true
  for rel in tools/maspsx tools/m2c tools/asm-differ tools/decomp-permuter; do
    [[ -n "$(ls -A "$wt/$rel" 2>/dev/null)" ]] && continue
    rm -rf "$wt/$rel"; ln -sfn "$ROOT/$rel" "$wt/$rel"
  done
  for rel in assets rom venv .venv; do
    [[ -e "$ROOT/$rel" ]] || continue
    rm -rf "$wt/$rel"; ln -sfn "$ROOT/$rel" "$wt/$rel"
  done
  # The machine-local directory's files are copied, not linked: the worklist and
  # the step ledger belong to the driver, and a worker regenerating them would
  # be rewriting the plan it was handed. So is the naming pass's own directory,
  # which a worker writes its reviews and verification into; its build cache is
  # left out, since a worker fills its own. Every other directory is linked -
  # they are archives and tool state measured in gigabytes, which a copy per
  # worker multiplies.
  mkdir -p "$wt/local"
  find "$ROOT/local" -mindepth 1 -maxdepth 1 -type f -exec cp -a -t "$wt/local/" {} + 2>/dev/null || true
  for rel in "$ROOT"/local/*/ "$ROOT"/local/.[!.]*/; do
    [[ -d "$rel" ]] || continue
    rel="$(basename "$rel")"
    if [[ "$rel" == name-pass ]]; then
      mkdir -p "$wt/local/name-pass"
      find "$ROOT/local/name-pass" -mindepth 1 -maxdepth 1 ! -name objdiff-build \
        -exec cp -a -t "$wt/local/name-pass/" {} + 2>/dev/null || true
    else
      ln -sfn "$ROOT/local/$rel" "$wt/local/$rel"
    fi
  done
  for rel in asm build linkers; do
    [[ -d "$ROOT/$rel" ]] || continue
    cp -a --reflink=auto "$ROOT/$rel" "$wt/$rel" 2>/dev/null \
      || cp -a "$ROOT/$rel" "$wt/$rel"
  done
  if [[ -n "$(tree_changes "$wt")" ]]; then
    echo "worker $i is dirty right after creation:" >&2
    tree_changes "$wt" >&2
    return 1
  fi
}

# Bring a worker back to the driver's state. Called after every join, so a
# round always forks from one tree: its own commits are already on the driver's
# branch by then, and anything it left behind is not wanted.
# The reference index (tools/refactor/ref_index.py) that renames and lookups
# query. The driver keeps it current on the landed tree, and each worker gets a
# copy at sync: paths are repository-relative and keyed by content hash, so it
# is valid in any checkout of the same commit, and a worker's own edits only
# re-scan what they touch. A failure is not fatal - the tools build it on demand.
index_refresh() {
  venv/bin/python3 tools/refactor/ref_index.py refresh >>"$LOG" 2>&1 \
    || echo "    reference index refresh failed; workers will build their own" | tee -a "$LOG"
}

sync_worker() {
  local i="$1" wt; wt="$(worker_dir "$i")"
  git -C "$wt" reset -q --hard HEAD
  git -C "$wt" clean -qfd src include configs 2>/dev/null
  git -C "$wt" checkout -q -B "$(worker_branch "$i")" "$(git -C "$ROOT" rev-parse HEAD)"
  # The renamer's log is an alias map the graph reads to find a ledger row filed
  # under an older spelling, so the workers get the driver's copy back.
  cp -a "$ROOT/local/renames.tsv" "$wt/local/renames.tsv" 2>/dev/null || true
  cp -a "$ROOT/local/name_pass_done.tsv" "$wt/local/name_pass_done.tsv" 2>/dev/null || true
  mkdir -p "$wt/local/name-pass/reviews"
  cp -a "$ROOT/local/name-pass/reviews/." "$wt/local/name-pass/reviews/" 2>/dev/null || true
  if [[ -f "$ROOT/local/ref_index.sqlite" ]]; then
    rm -f "$wt/local/ref_index.sqlite-wal" "$wt/local/ref_index.sqlite-shm"
    cp "$ROOT/local/ref_index.sqlite" "$wt/local/ref_index.sqlite"
  fi
}

# Carry a worker's rename log back. The rows are appended by rename_item.py in
# whichever tree ran it, and the graph needs all of them to resolve a name.
collect_renames() {
  local wt="$1" src="$1/local/renames.tsv"
  [[ -f "$src" ]] || return 0
  if [[ ! -f "$ROOT/local/renames.tsv" ]]; then
    cp -a "$src" "$ROOT/local/renames.tsv"
    return 0
  fi
  comm -13 <(sort "$ROOT/local/renames.tsv") <(sort "$src") \
    >>"$ROOT/local/renames.tsv" 2>/dev/null || true
}

# --- the landing agent --------------------------------------------------------
# A conflict between two steps is not something a driver can decide: both sides
# are real work, and the resolution is the union of two analyses. So the tree is
# left exactly as the failed replay left it - markers in place, `git status`
# explaining itself - and an agent is given the round's commits to land.
land_round() {
  local why="$1" pre="$2"; shift 2
  local brief
  brief="$(cat <<EOF
$( [[ "$CLI" != "grok" && -f "$RULES" ]] && cat "$RULES" )

# Naming pass: land a round

$WORKERS naming steps ran in parallel, each in its own worktree, and their
commits are being replayed onto this branch. $why

Round commits, in the order they must land:
$(printf '  %s\n' "$@")

The branch was at $pre before the round, so nothing is lost: every commit above
still exists on its worker's branch.

## What to do

1. \`git status\` says where the replay stopped. Finish it. A conflict here is
   two naming analyses touching the same declaration, so the resolution is the
   union of both: keep each side's rename, its retyping and its documentation.
   Never resolve by taking one side wholesale, and never drop a step's work to
   make the tree build.
2. Replay whatever is left with \`git cherry-pick <sha>\`, in the order listed.
3. Then make the result correct rather than merely applied: two steps may have
   renamed the same thing differently, or retyped a field in incompatible ways.
   Reconcile the declarations so the tree says one thing.
4. Finish with \`venv/bin/python3 tools/refactor/verify_name_pass.py\` passing:
   the matching build and the declaration and symbol-map checks.
   Review the round's reports under \`local/name-pass/reviews/$RUN_ID-r$i-*\`;
   preserve unresolved work and update conclusions affected by reconciliation.
5. Leave the work committed on this branch, with the tree clean and no replay in
   progress. One commit per step is preferred; a single commit naming every item
   is acceptable when the resolutions cannot be separated.

If a step cannot be reconciled, leave the round unfinished and report why. The
driver must not record a dropped step as reviewed.
EOF
)"
  run_agent "$ROOT" "$brief" "$LOG" 1
}

# --- the round ----------------------------------------------------------------
# Replay the workers' commits onto the driver's branch and verify the result
# once. Everything mechanical is done here; anything that is not is handed over.
join_round() {
  local pre="$1"; shift
  local -a commits=("$@")
  local sha needs_agent=""
  for sha in "${commits[@]}"; do
    git cherry-pick "$sha" >>"$LOG" 2>&1 && continue
    echo "--- replay of $sha conflicts; handing the round to a landing agent" | tee -a "$LOG"
    needs_agent="The replay of $sha stopped on a conflict and the tree is still mid-replay."
    break
  done
  if [[ -z "$needs_agent" ]]; then
    if ! shielded venv/bin/python3 tools/refactor/verify_name_pass.py >"$LOG.build" 2>&1; then
      echo "--- the joined tree does not build; handing the round to a landing agent" | tee -a "$LOG"
      tail -20 "$LOG.build" | tee -a "$LOG"
      needs_agent="Every commit replayed cleanly, but the joined tree fails
\`tools/refactor/verify_name_pass.py\` - the steps agree textually and disagree in
substance. The build output is at $LOG.build."
    fi
  fi
  if [[ -n "$needs_agent" ]]; then
    land_round "$needs_agent" "$pre" "${commits[@]}"
    if [[ -n "$(tree_changes "$ROOT")" ]] || [[ -e "$(git rev-parse --git-dir)/CHERRY_PICK_HEAD" ]]; then
      echo "landing agent left the tree unfinished; rewinding to $pre" >&2
      git cherry-pick --abort >/dev/null 2>&1 || true
      git reset -q --hard "$pre"
      git clean -qfd src include configs >/dev/null 2>&1
      return 1
    fi
    if ! shielded venv/bin/python3 tools/refactor/verify_name_pass.py >"$LOG.build" 2>&1; then
      echo "landing agent finished but the tree does not build; rewinding to $pre" >&2
      tail -20 "$LOG.build" >&2
      git reset -q --hard "$pre"
      return 1
    fi
  fi
  return 0
}

# A round that cannot be joined is not a round of bad steps. Each step was
# verified in its own worktree, so the failure belongs to a pair of them - two
# steps that agree textually and disagree in substance - and the rest have
# nothing to do with it. So when the join and its agent both fail, the steps are
# landed one at a time, each verified on top of those already kept, and only a
# step that conflicts or breaks the tree there is left for a later round, where
# it will be worked against a tree that already holds its counterpart. Without
# this one clash discarded ten steps of finished work.
# Prints the orders that landed.
salvage_round() {
  local order sha before
  for order in "$@"; do
    sha="${sha_of[$order]:-}"
    if [[ -z "$sha" ]]; then
      echo "$order"               # a review with no commit has nothing to join
      continue
    fi
    before="$(git rev-parse HEAD)"
    if ! git cherry-pick "$sha" >>"$LOG" 2>&1; then
      git cherry-pick --abort >/dev/null 2>&1 || true
      git reset -q --hard "$before"
      echo "--- step $order conflicts with the steps already landed; left for a later round" | tee -a "$LOG" >&2
      continue
    fi
    if ! shielded venv/bin/python3 tools/refactor/verify_name_pass.py >"$LOG.build" 2>&1; then
      git reset -q --hard "$before"
      git clean -qfd src include configs >/dev/null 2>&1
      echo "--- step $order does not build on the steps already landed; left for a later round" | tee -a "$LOG" >&2
      continue
    fi
    echo "$order"
  done
}

if (( CLEAN_WORKERS )); then
  for wt in "$WORKER_ROOT"/pe2-name-w*; do
    [[ -d "$wt" ]] || continue
    remove_worker "${wt##*pe2-name-w}"
  done
  echo "worker worktrees removed"
  exit 0
fi

# A dry run only prints briefs, so it stays usable while the tree is dirty -
# which is when someone is most likely to want to read one.
if (( DRY == 0 )) && [[ -n "$(tree_changes "$ROOT")" ]]; then
  echo "tree is dirty; commit or stash before running the pass" >&2
  exit 1
fi

# The worklist on disk was built by whatever ran last: an earlier run with
# other kinds, a hand rebuild with no run alive (which reads as "all kinds" and
# makes small steps), or a tree that has moved since. Steps are sized and
# ordered for the kinds this run works, so build it once before the first
# round; every later round rebuilds it anyway. --no-refresh and --dry-run keep
# what is there.
(( DRY )) || refresh_worklist

if (( WORKERS > 1 )) && ! head -1 "$WORKLIST" | grep -q $'\tafter$'; then
  echo "this worklist has no 'after' column, so a round cannot be checked for" >&2
  echo "independence; rebuild it first:" >&2
  echo "  venv/bin/python3 tools/refactor/dep_graph.py worklist" >&2
  exit 1
fi

if (( WORKERS > 1 && DRY == 0 )); then
  for w in $(seq 1 "$WORKERS"); do
    (( w == 1 )) && index_refresh
    create_worker "$w" || { echo "could not create worker $w" >&2; exit 1; }
    sync_worker "$w"
  done
fi

done_count=0
followup_count=0
fail_count=0
barrier=
i=0
while (( TIMES == 0 || i < TIMES )); do
  (( STOP_REQUESTED )) && break
  mapfile -t picked < <(next_batch)
  batch=(); barrier_line=""
  for line in ${picked[@]+"${picked[@]}"}; do
    case "$line" in
      barrier*) barrier_line="$line" ;;
      *) batch+=("$line") ;;
    esac
  done
  if (( ${#batch[@]} == 0 )); then
    if [[ -n "$barrier_line" ]]; then
      IFS=$'\t' read -r _ barrier_order barrier_item <<<"$barrier_line"
      cat >&2 <<BARRIER
=== stopping at step $barrier_order: $barrier_item is still assembly

It has no C body, so this pass cannot establish what it is, and every later
step depends on it. Decompile it first, for example:

  ./tools/vacuum.sh --overlay <its overlay>

then rebuild the graph and resume:

  venv/bin/python3 tools/refactor/dep_graph.py --build
  venv/bin/python3 tools/refactor/dep_graph.py worklist
BARRIER
      barrier=1
    else
      echo "worklist exhausted"
    fi
    break
  fi
  i=$((i + 1))

  # Into the log as well as the terminal, with blank lines around it: the log
  # is otherwise one unbroken stream in which nothing says where a step began
  # or which item it was for.
  # Built before the banner, not inside it: the banner is a pipeline into tee,
  # which runs in a subshell, and an assignment made there is discarded.
  declare -A step_items=()
  for order in "${batch[@]}"; do
    mapfile -t its < <(awk -F'\t' -v o="$order" '$1==o{print $3}' "$WORKLIST")
    step_items[$order]="${its[*]}"
  done
  {
    echo
    echo "================================================================"
    echo "=== round of ${#batch[@]} step(s)  ($(date '+%Y-%m-%d %H:%M:%S'))"
    for order in "${batch[@]}"; do
      mapfile -t its < <(awk -F'\t' -v o="$order" '$1==o{print $3}' "$WORKLIST")
      echo "=== step $order: ${its[*]}"
      for _it in "${its[@]}"; do
        awk -F'\t' -v n="$_it" 'NR>1 && $3==n {
          printf "===   %s  (%s, %s, %s referrer(s))\n", $3, $4, $6, $8 }' "$WORKLIST"
      done
    done
    echo "================================================================"
    echo
  } | tee -a "$LOG"

  if (( DRY )); then
    for order in "${batch[@]}"; do build_brief "$order"; done
    exit 0
  fi

  pre="$(git rev-parse HEAD)"
  declare -A status_of=()
  if (( WORKERS == 1 )); then
    order="${batch[0]}"
    st="$(mktemp -t name_pass_status.XXXXXX)"
    work_step "$ROOT" "$order" "${step_items[$order]}" \
              "$(build_brief "$order")" "$LOG" "$st" 1
    status_of[$order]="$(cat "$st")"; rm -f "$st"
  else
    # Fork. Each worker gets its own worktree and its own log, and works the
    # round's queue (see queue_take); the terminal would be unreadable with
    # several agents streaming into it, so it gets a line per step and the logs
    # hold the detail.
    QDIR="$(mktemp -d -t name_pass_round.XXXXXX)"
    printf '%s\n' "${batch[@]}" >"$QDIR/queue"
    : >"$QDIR/started"
    QACTIVE=$(( ${#batch[@]} < WORKERS ? ${#batch[@]} : WORKERS ))
    (( STOP_REQUESTED )) && : >"$QDIR/stop"
    pids=()
    for w in $(seq 1 "$QACTIVE"); do
      worker_loop "$w" &
      pids[$w]=$!
    done
    for w in "${!pids[@]}"; do wait "${pids[$w]}" || true; done
    # A trapped Ctrl-C makes `wait` return early, each time it arrives, so
    # neither the loop above nor one bare wait is enough: keep waiting until
    # no worker is left running.
    while [[ -n "$(jobs -rp)" ]]; do wait 2>/dev/null || true; done
    # From here the round is the steps that were started, in that order; what
    # is still queued was never begun and stays in the worklist.
    mapfile -t unstarted <"$QDIR/queue"
    mapfile -t batch <"$QDIR/started"
    (( ${#unstarted[@]} == 0 )) || echo "--- ${#unstarted[@]} queued step(s) not started this round: ${unstarted[*]}" | tee -a "$LOG"
    declare -A collected=()
    for order in "${batch[@]}"; do
      w="$(cat "$QDIR/worker.$order")"
      status_of[$order]="$(cat "$QDIR/status.$order" 2>/dev/null)"
      [[ -n "${collected[$w]:-}" ]] || { collect_renames "$(worker_dir "$w")"; collected[$w]=1; }
      report="$(review_file "$order")"
      mkdir -p "$(dirname "$report")"
      [[ ! -f "$(worker_dir "$w")/$report" ]] || cp "$(worker_dir "$w")/$report" "$report"
      echo "--- worker $w: step $order ${status_of[$order]:-no status}" | tee -a "$LOG"
    done
    rm -rf "$QDIR"; QDIR=
  fi

  # Join. In the serial case the commit is already on the branch and there is
  # nothing to replay; the bookkeeping below is the same either way.
  commits=(); review_order=(); round_bad=0
  declare -A sha_of=()
  for order in "${batch[@]}"; do
    read -r outcome sha <<<"${status_of[$order]:-failed}"
    case "$outcome" in
      ok|followup) review_order+=("$order")
                   [[ "$sha" == "-" ]] || { commits+=("$sha"); sha_of[$order]="$sha"; } ;;
      *) record "$order" "${step_items[$order]}" - failed
         SKIP_NAMES+="${step_items[$order]} "
         fail_count=$((fail_count + 1)); round_bad=1 ;;
    esac
  done

  if (( ${#review_order[@]} > 0 )); then
    landed_order=("${review_order[@]}")
    joined=1
    # The point a failed join rewinds to is the branch as it stands now, not as
    # it stood when the round was forked: anything landed on it while the
    # workers ran - by hand, or by another tool between rounds - stays. A join
    # that fell back to one step at a time once dropped four such commits.
    (( WORKERS == 1 )) || pre="$(git rev-parse HEAD)"
    if ! { (( WORKERS == 1 || ${#commits[@]} == 0 )) || join_round "$pre" "${commits[@]}"; }; then
      joined=0
      echo "--- the round could not be joined; landing its steps one at a time" | tee -a "$LOG"
      mapfile -t landed_order < <(salvage_round "${review_order[@]}")
    fi
    if (( ${#landed_order[@]} > 0 )); then
      for order in "${landed_order[@]}"; do
        read -ra names <<<"${step_items[$order]}"
        report="$(review_file "$order")"
        outcome=$(venv/bin/python3 tools/refactor/name_review.py land --report "$report" \
                    --commit "$(git rev-parse HEAD)" "${names[@]}") || exit 1
        record "$order" "${step_items[$order]}" "$(git rev-parse --short HEAD)" "$outcome" "$report"
        done_count=$((done_count + 1))
        [[ "$outcome" != "followup" ]] || followup_count=$((followup_count + 1))
        echo "=== step $order: $outcome; review: $report" | tee -a "$LOG"
      done
      { echo "=== round landed: $pre..$(git rev-parse --short HEAD)"; echo; } | tee -a "$LOG"
    fi
    if (( ! joined )); then
      # The commits still exist on the worker branches, so the work is
      # recoverable; what must not survive is an unverified branch tip.
      for order in "${review_order[@]}"; do
        [[ " ${landed_order[*]} " == *" $order "* ]] && continue
        record "$order" "${step_items[$order]}" - landing-failed
        SKIP_NAMES+="${step_items[$order]} "
        fail_count=$((fail_count + 1))
        round_bad=1
        echo "step $order NOT landed; it is still committed on its name-pass/w* branch" >&2
      done
    fi
  fi

  if (( STOP_REQUESTED )); then
    # Reset the workers first: an interrupted step leaves its worktree dirty,
    # and the next run refuses to fork from one.
    if (( WORKERS > 1 )); then
      for w in $(seq 1 "$WORKERS"); do sync_worker "$w"; done
    fi
    echo "interrupted; stopped after the round" | tee -a "$LOG"
    break
  fi

  # Missing/incomplete reports and failed checks stop by default. A verified
  # review with no source changes is an ordinary successful visit.
  if (( round_bad )) && ! (( KEEP_GOING )); then
    exit 1
  fi

  refresh_worklist
  (( WORKERS > 1 )) && index_refresh
  if (( WORKERS > 1 )); then
    for w in $(seq 1 "$WORKERS"); do sync_worker "$w"; done
  fi
done

echo "reviewed $done_count step(s), $followup_count with follow-ups, $fail_count failed${barrier:+, stopped at a barrier}"

# Steps and joins are verified by the build, declarations and symbol maps; the
# per-function objdiff comparison is an audit, run once over everything landed.
if (( DRY == 0 )) && [[ "$(git rev-parse HEAD)" != "$RUN_START" ]]; then
  echo "--- auditing the landed tree with objdiff" | tee -a "$LOG"
  if shielded venv/bin/python3 tools/refactor/verify_name_pass.py --objdiff >"$LOG.audit" 2>&1; then
    echo "objdiff audit passed: $(grep -o 'All [0-9]* individual functions match at 100%' "$LOG.audit")" | tee -a "$LOG"
  else
    echo "OBJDIFF AUDIT FAILED over ${RUN_START:0:9}..$(git rev-parse --short HEAD); see $LOG.audit" | tee -a "$LOG" >&2
    tail -20 "$LOG.audit" >&2
  fi
fi
if (( WORKERS > 1 )); then
  echo "worker worktrees kept for the next run; --clean-workers removes them"
fi
