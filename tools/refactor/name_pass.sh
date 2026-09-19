#!/usr/bin/env bash
# Walk the naming worklist, handing each step to an agent.
#
#   ./tools/refactor/name_pass.sh [--times N] [--workers N] [--profile NAME]
#                                 [--dry-run] [--cli claude|grok|codex]
#                                 [--from ORDER] [--step ORDER]
#                                 [--list-profiles] [--clean-workers]
#
# With no --times the whole worklist is walked. --times N stops after N steps.
# --clean-workers removes the worker worktrees and exits, doing no work.
#
# Profiles are the same ones the matching vacuum uses, from
# local/vacuum_profiles: a profile names the agent arm, the model, the
# reasoning effort and optionally a wrapper command to launch it through. An
# explicit --cli, or a VACUUM_MODEL / VACUUM_MATCH_EFFORT already exported,
# still wins over the profile.
#
# A step is one line of local/worklist.tsv, or several lines sharing an order
# when a cycle means the items have to be understood together. For each step
# this builds a brief, runs the agent against a tree, verifies the build, and
# commits. A step needs the whole tree in front of it and produces no candidate
# to throw away, so there is no scratch environment of the kind the matching
# vacuum builds.
#
# The safety property is the build. An agent that leaves a tree broken, or that
# changes a checksum, has its work reverted and the pass stops rather than
# carrying a bad rename into the next step.
#
# --workers N runs N steps at a time, fork-join. Each worker owns one worktree
# for the whole run: a round assigns it one step, it works and commits there,
# and afterwards every worker's commit is replayed onto the driver's branch and
# the joined tree is verified once. A replay that conflicts, or a joined tree
# that no longer builds, is handed to a landing agent that has both sides in
# front of it - the same division of labour the overlay sweep uses. The workers
# are then reset to the landed state, so each round starts from one tree again.
#
# Only steps that do not wait on each other share a round. The worklist's
# `after` column gives the last step each one depends on, and a round is
# widened only while that stays behind the round's first step; the dependency
# order is the premise of the pass, so an item is never worked while something
# it uses is being worked beside it. How much parallelism that allows is a
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
# appended here instead, and a step is picked only if it has no row.
DONE_LEDGER="local/name_pass_done.tsv"
# The standing job description. grok takes it as a system prompt via --rules,
# where it frames the whole session; the other arms get the same text inlined at
# the head of the brief. One source either way, so the two cannot drift.
RULES=".grok/rules/name-pass.md"
CLI="${VACUUM_CLI:-claude}"
CLI_EXPLICIT=0
PROFILE="${PROFILE-${VACUUM_PROFILE:-}}"
# Zero means the whole worklist. A driver told to walk a list has no reason to
# stop after the first item, and a default of one made the common invocation
# look like it had run out of work.
TIMES=0
WORKERS=1
# Beside the repository by default, the way the matching vacuum places its
# worktrees, so a checkout is not nested inside another one.
WORKER_ROOT="${NAME_PASS_WORKTREE_ROOT:-$(dirname "$ROOT")}"
CLEAN_WORKERS=0
DRY=0
FROM=0
ONLY=""
LIST_PROFILES=0
KEEP_GOING=0
REFRESH=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --times) TIMES="$2"; shift 2 ;;
    --workers|-j) WORKERS="$2"; shift 2 ;;
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
    --dry-run) DRY=1; shift ;;
    --all) TIMES=0; KEEP_GOING=1; shift ;;
    --keep-going) KEEP_GOING=1; shift ;;
    --no-refresh) REFRESH=0; shift ;;
    --stop-on-fail) KEEP_GOING=0; shift ;;
    -h|--help) sed -n '2,45p' "$0"; exit 0 ;;
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
  grep -rqlw --include='*.c' --include='*.h' -- "$1" src include 2>/dev/null
}

# Keyed on the item name, not the step order: rebuilding the graph renumbers
# every step, so an order is only meaningful within one worklist.
ledgered() {
  [[ -f "$DONE_LEDGER" ]] || return 1
  awk -F'\t' '$4=="ok"{print $2}' "$DONE_LEDGER" | tr ' ' '\n' | grep -qx -- "$1"
}

# order, items, commit (or -), outcome.
record() {
  printf '%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" >> "$DONE_LEDGER"
}

# Items this run has already attempted without landing them. The ledger cannot
# serve here: it records the failure, but only an `ok` row retires an item, so
# without this a --keep-going run hands the same step to the next round for
# ever, against the same tree it just failed against.
SKIP_NAMES=" "

# The orders to work next, at most $WORKERS of them, one per line.
#
# The first is the lowest outstanding step, as it always was. Each further one
# is taken only if everything it depends on comes before that first step, which
# is what the `after` column says: dependencies always precede their users in
# the worklist, so `after < first` means none of a step's dependencies is in the
# round. Steps already in the round cannot depend on a later one for the same
# reason, so the set is mutually independent in both directions.
#
# A barrier - an item still in assembly - ends the scan rather than being
# skipped. The steps behind it are the ones that use it, so working them would
# reason from a name that cannot be established yet.
next_batch() {
  local order name state after first="" last="" taken=0 scanned=0
  while IFS=$'\t' read -r order _ name _ _ state _ _ after; do
    [[ "$order" == "order" ]] && continue
    (( order < FROM )) && continue
    [[ -n "$ONLY" && "$order" != "$ONLY" ]] && continue
    # A cycle is several rows sharing an order, and it is one step: the first
    # row that qualifies takes the whole order, and the rest are its siblings.
    [[ "$order" == "$last" ]] && continue
    ledgered "$name" && continue
    [[ "$SKIP_NAMES" == *" $name "* ]] && continue
    outstanding "$name" || continue
    if [[ -n "$first" ]]; then
      # Looking further costs a tree-wide grep per candidate, and the answer
      # stops improving quickly once the front narrows.
      (( ++scanned > 40 * WORKERS )) && break
      (( after < first )) || continue
    fi
    if [[ "$state" == "generated" ]]; then
      # Reported by the caller, which stops the pass: nothing behind a barrier
      # is workable, whether or not this round found work in front of it. It
      # travels on stdout because this function is read through a pipe, which
      # puts it in a subshell whose variables the caller never sees.
      printf 'barrier\t%s\t%s\n' "$order" "$name"
      break
    fi
    echo "$order"
    last="$order"
    [[ -n "$first" ]] || first="$order"
    (( ++taken >= WORKERS )) && break
  done < <(rows)
  (( taken > 0 ))
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
      line+="
references (how each use reads or writes it):
\`\`\`
$(timeout 300 venv/bin/python3 tools/refactor/find_references.py "$file/$name" -q 2>/dev/null | tail -40)
\`\`\`
"
    fi
  done < <(rows)

  cat <<EOF
$( [[ "$CLI" != "grok" && -f "$RULES" ]] && cat "$RULES" )

# Naming pass, step $order

Process ${#names[@]} item(s) together: ${names[*]}
$( ((${#names[@]} > 1)) && echo "
These items form a cycle in the dependency graph: each uses the others, so they
have to be understood as one unit rather than in sequence." )
$line

## What to do with this item

The conventions, the compiler's limits and what counts as evidence are above
(and in NAMING.md); this is what is specific to this step.

1. Derive what the item is from the references listed above and the code they
   sit in. The reference listing marks reads, writes, casts, address-taken and
   mentions in prose - the casts in particular tell you whether the declared
   type is honest.
2. If its state above is \`current\`, the name already follows the convention:
   **do not rename it.** Everything else still applies - the type, the shape of
   the declaration, the fields, the parameters and the documentation.
   Otherwise rename with the tool - always, never by hand, because the file it
   appends to is what tells the pass this item is done:
     venv/bin/python3 tools/refactor/rename_item.py <file>/<oldName> <newName> --sidecars
3. Apply the same to what the item contains: its fields, and its parameters in
   both the prototype and the definition.
4. If its visibility above is \`private\`, its declaration belongs in the \`.c\`
   that uses it rather than a header, with the \`_\` marker; add \`static\` if
   the build still matches.
5. Where the role genuinely cannot be established, leave the name and say so.
   An invented name is worse than a generated one.

## Finishing

Run \`./tools/build-and-verify.sh\`. It must end with BUILD SUCCEEDED and the
matched-function count must not drop. If your change broke the match, fixing it
is the remaining work - not grounds to revert the change. Do not commit; the
driver commits.
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

# A step is an analysis, not just a rename of its own item: it merges a
# duplicate type away, retypes a caller, renames a neighbouring field. That
# changes which items remain and what depends on what, so the plan the next
# step reads has to be rebuilt rather than carried forward. Roughly 20s against
# a step of several minutes. --no-refresh keeps the original snapshot for the
# whole run.
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
  local stream=0
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
      cmd+=(--dangerously-skip-permissions "$brief")
      ;;
    grok)
      # grok emits the same NDJSON wire format on request, so the one formatter
      # renders both arms and a grok step reports its cost and turn count the
      # way a claude step does.
      cmd=("${LAUNCH_CMD[@]}" --always-approve ${EFFORT:+--effort "$EFFORT"}
           --cwd "$dir" ${RULES:+--rules "$ROOT/$RULES"})
      if [[ "${VACUUM_STREAM:-1}" != "0" ]]; then
        cmd+=(--output-format streaming-messages-json --include-partial-messages)
        stream=1
      fi
      cmd+=(-p "$brief")
      ;;
    codex)
      cmd=("${LAUNCH_CMD[@]}" exec --dangerously-bypass-approvals-and-sandbox
           ${MODEL:+--model "$MODEL"} --cd "$dir" "$brief")
      ;;
    *) echo "unknown api: $CLI" >&2; return 2 ;;
  esac
  if (( stream )); then
    if (( term )); then
      ( cd "$dir" && "${cmd[@]}" ) \
        | python3 tools/stream_format.py ${VACUUM_STREAM_QUIET:+--quiet-text} \
        | tee -a "$log"
    else
      ( cd "$dir" && "${cmd[@]}" ) \
        | python3 tools/stream_format.py ${VACUUM_STREAM_QUIET:+--quiet-text} \
        >>"$log" 2>&1
    fi
  elif (( term )); then
    ( cd "$dir" && "${cmd[@]}" ) | tee -a "$log"
  else
    ( cd "$dir" && "${cmd[@]}" ) >>"$log" 2>&1
  fi
}

# --- one step, in one tree ----------------------------------------------------
# The agent, the build, the commit. Which tree it is does not matter here: the
# driver's own when the pass is serial, a worker's when it is not. The outcome
# goes to a file because in parallel this runs in a background subshell, whose
# variables the driver never sees.
work_step() {
  local dir="$1" order="$2" items="$3" brief="$4" log="$5" status="$6" term="$7"
  run_agent "$dir" "$brief" "$log" "$term"
  if ! ( cd "$dir" && ./tools/build-and-verify.sh ) >"$log.build" 2>&1; then
    {
      echo "step $order FAILED to build in $dir; reverting"
      tail -20 "$log.build"
    } | tee -a "$log" >&2
    git -C "$dir" checkout -- . 2>/dev/null
    git -C "$dir" clean -fd src include configs >/dev/null 2>&1
    echo failed >"$status"
    return 0
  fi
  if [[ -z "$(tree_changes "$dir")" ]]; then
    echo "step $order left the tree unchanged" | tee -a "$log" >&2
    echo unchanged >"$status"
    return 0
  fi
  git -C "$dir" add -A
  # A linked submodule or a generated tree that slipped past .gitignore would
  # otherwise be committed by `add -A` and then replayed onto the branch.
  git -C "$dir" reset -q -- "${SCAFFOLD_PATHS[@]}" 2>/dev/null
  git -C "$dir" commit -q -m "naming: $items"
  printf 'ok %s\n' "$(git -C "$dir" rev-parse HEAD)" >"$status"
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
  # The machine-local directory is a copy, not a link: the worklist and the step
  # ledger belong to the driver, and a worker regenerating them would be
  # rewriting the plan it was handed.
  mkdir -p "$wt/local"
  cp -a "$ROOT/local/." "$wt/local/" 2>/dev/null || true
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
sync_worker() {
  local i="$1" wt; wt="$(worker_dir "$i")"
  git -C "$wt" reset -q --hard HEAD
  git -C "$wt" clean -qfd src include configs 2>/dev/null
  git -C "$wt" checkout -q -B "$(worker_branch "$i")" "$(git -C "$ROOT" rev-parse HEAD)"
  # The renamer's log is an alias map the graph reads to find a ledger row filed
  # under an older spelling, so the workers get the driver's copy back.
  cp -a "$ROOT/local/renames.tsv" "$wt/local/renames.tsv" 2>/dev/null || true
  cp -a "$ROOT/local/name_pass_done.tsv" "$wt/local/name_pass_done.tsv" 2>/dev/null || true
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
4. Finish with \`./tools/build-and-verify.sh\` ending in BUILD SUCCEEDED and the
   matched-function count not dropped. The checksum is the acceptance test, not
   a fence: if reconciling changed code generation, fix the code generation.
5. Leave the work committed on this branch, with the tree clean and no replay in
   progress. One commit per step is preferred; a single commit naming every item
   is acceptable when the resolutions cannot be separated.

If a step genuinely cannot be landed, drop that one commit, say which and why,
and land the rest.
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
    if ! ./tools/build-and-verify.sh >"$LOG.build" 2>&1; then
      echo "--- the joined tree does not build; handing the round to a landing agent" | tee -a "$LOG"
      tail -20 "$LOG.build" | tee -a "$LOG"
      needs_agent="Every commit replayed cleanly, but the joined tree fails
\`./tools/build-and-verify.sh\` - the steps agree textually and disagree in
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
    if ! ./tools/build-and-verify.sh >"$LOG.build" 2>&1; then
      echo "landing agent finished but the tree does not build; rewinding to $pre" >&2
      tail -20 "$LOG.build" >&2
      git reset -q --hard "$pre"
      return 1
    fi
  fi
  return 0
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

if (( WORKERS > 1 )) && ! head -1 "$WORKLIST" | grep -q $'\tafter$'; then
  echo "this worklist has no 'after' column, so a round cannot be checked for" >&2
  echo "independence; rebuild it first:" >&2
  echo "  venv/bin/python3 tools/refactor/dep_graph.py worklist" >&2
  exit 1
fi

if (( WORKERS > 1 && DRY == 0 )); then
  for w in $(seq 1 "$WORKERS"); do
    create_worker "$w" || { echo "could not create worker $w" >&2; exit 1; }
    sync_worker "$w"
  done
fi

done_count=0
fail_count=0
barrier=
noop=0
i=0
while (( TIMES == 0 || i < TIMES )); do
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
  # --times counts steps, not rounds, so a wide round can overshoot it; trim the
  # round rather than starting one that would.
  if (( TIMES > 0 && i + ${#batch[@]} > TIMES )); then
    batch=("${batch[@]:0:$((TIMES - i))}")
  fi
  i=$((i + ${#batch[@]}))

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
    # Fork. Each worker gets one step, its own worktree and its own log; the
    # terminal would be unreadable with several agents streaming into it, so it
    # gets a line per worker and the logs hold the detail.
    pids=(); sts=(); w=0
    for order in "${batch[@]}"; do
      w=$((w + 1))
      wlog="${LOG%.log}-w$w.log"
      st="$(mktemp -t name_pass_status.XXXXXX)"
      sts[$w]="$st"
      echo "--- worker $w: step $order (${step_items[$order]}) -> $wlog" | tee -a "$LOG"
      work_step "$(worker_dir "$w")" "$order" "${step_items[$order]}" \
                "$(build_brief "$order")" "$wlog" "$st" 0 &
      pids[$w]=$!
    done
    for w in "${!pids[@]}"; do wait "${pids[$w]}" || true; done
    w=0
    for order in "${batch[@]}"; do
      w=$((w + 1))
      status_of[$order]="$(cat "${sts[$w]}" 2>/dev/null)"
      rm -f "${sts[$w]}"
      collect_renames "$(worker_dir "$w")"
      echo "--- worker $w: step $order ${status_of[$order]:-no status}" | tee -a "$LOG"
    done
  fi

  # Join. In the serial case the commit is already on the branch and there is
  # nothing to replay; the bookkeeping below is the same either way.
  commits=(); commit_order=(); round_bad=0
  for order in "${batch[@]}"; do
    read -r outcome sha <<<"${status_of[$order]:-failed}"
    case "$outcome" in
      ok) commits+=("$sha"); commit_order+=("$order") ;;
      unchanged) record "$order" "${step_items[$order]}" - unchanged
                 SKIP_NAMES+="${step_items[$order]} "
                 noop=$((noop + 1)); round_bad=1 ;;
      *) record "$order" "${step_items[$order]}" - failed
         SKIP_NAMES+="${step_items[$order]} "
         fail_count=$((fail_count + 1)); round_bad=1 ;;
    esac
  done

  if (( ${#commits[@]} > 0 )); then
    if (( WORKERS == 1 )); then
      record "${commit_order[0]}" "${step_items[${commit_order[0]}]}" \
             "$(git rev-parse --short HEAD)" ok
      done_count=$((done_count + 1))
      noop=0
      { echo "=== step ${commit_order[0]} committed: $(git rev-parse --short HEAD)"
        echo; } | tee -a "$LOG"
    elif join_round "$pre" "${commits[@]}"; then
      for order in "${commit_order[@]}"; do
        record "$order" "${step_items[$order]}" "$(git rev-parse --short HEAD)" ok
        done_count=$((done_count + 1))
      done
      noop=0
      { echo "=== round landed: $pre..$(git rev-parse --short HEAD)"; echo; } | tee -a "$LOG"
    else
      # The commits still exist on the worker branches, so the work is
      # recoverable; what must not survive is an unverified branch tip.
      for order in "${commit_order[@]}"; do
        record "$order" "${step_items[$order]}" - landing-failed
        SKIP_NAMES+="${step_items[$order]} "
        fail_count=$((fail_count + 1))
      done
      round_bad=1
      echo "round NOT landed; the branch is back at ${pre:0:9}" >&2
      echo "the steps are still committed on the name-pass/w* branches" >&2
    fi
  fi

  # A step that genuinely needs no change is rare; a run of them means the agent
  # is not working at all - an expired key or an unreachable API returns
  # instantly and touches nothing. Walking the worklist at that speed marks real
  # items as visited, so stop instead.
  if (( noop >= 3 )); then
    echo "three steps in a row changed nothing; stopping - check the agent" >&2
    exit 1
  fi
  # A step that failed its build, one that changed nothing, and a round that
  # could not be landed are all reasons to stop by default: the pass carries a
  # tree forward, so continuing past a bad outcome builds on it.
  if (( round_bad )) && ! (( KEEP_GOING )); then
    exit 1
  fi

  refresh_worklist
  if (( WORKERS > 1 )); then
    for w in $(seq 1 "$WORKERS"); do sync_worker "$w"; done
  fi
done

echo "completed $done_count step(s), $fail_count failed${barrier:+, stopped at a barrier}"
if (( WORKERS > 1 )); then
  echo "worker worktrees kept for the next run; --clean-workers removes them"
fi
