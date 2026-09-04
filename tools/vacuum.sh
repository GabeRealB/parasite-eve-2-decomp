#!/usr/bin/env bash
#
# Vacuum: pick the easiest unmatched function across every overlay, bootstrap a
# scratch env, pack a brief, and hand it to an agent CLI.
#
# Usage:
#   ./tools/vacuum.sh [--cli claude|grok] [--claude] [--grok] [--times N]
#                     [--dry-run] [--keep-scratch] [--no-permute]
#                     [--orchestrator] [--difficult] [--overlay NAME]
#
# Environment:
#   VACUUM_CLI   Default CLI when no --cli/--claude/--grok flag is given
#                (claude | grok). Defaults to claude.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

MAX_TIMES=""
STOP_REQUESTED=0
DRY_RUN=0
KEEP_SCRATCH=0
PERMUTER=1
# Overridable so a retry pass can be aimed at a subset - the >=99% near-misses,
# say - without editing the real list. score_functions.py --only-difficult reads
# the same path via VACUUM_DIFFICULT_FILE.
DIFFICULT_FUNCTIONS="${VACUUM_DIFFICULT_FILE:-tools/difficult_functions}"
# Paths a match is allowed to land. Jump-table / splat edits live in configs/.
# Do not add tools/ or ninja_config.py; agents sometimes stage those by accident.
MATCH_LAND_PATHS=(
  src
  include
  configs
  DECOMPILATION_LEARNINGS.md
  STRUCT_FIELDS.md
  NAMING.md
)
CLI="${VACUUM_CLI:-claude}"
LOG_FILE=""
OVERLAY_PY="tools/decomp_overlay.py"
ORCH=0
ONLY_DIFFICULT=0
OVERLAY=""
SKIP_FILE=""
SESSION=""
WORKTREE_PARENT=""
ORCH_FUNC=""
ORCH_WT=""
ORCH_SCRATCH=""
ORCH_BASE=""
ORCH_MERGE=0
ORCH_FINISHED=0

orch_cleanup() { :; }

usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  --cli NAME        Agent CLI: claude or grok (default: ${VACUUM_CLI:-claude})
  --claude          Shorthand for --cli claude
  --grok            Shorthand for --cli grok
  --times N         Stop after N loop iterations
  --dry-run         Score, bootstrap, pack the prompt; do not launch the agent
  --keep-scratch    Leave nonmatchings/<func> in place after the iteration
  --no-permute      Do not run decomp-permuter after a ≥95% give-up
  --orchestrator    Claim via tools/vacuum_orch.py, match in a throwaway
                    worktree, then run a port agent on this tree under the
                    merge lock. Do not run a non-orchestrator vacuum on this
                    tree at the same time.
  --difficult       Only pick functions listed in tools/difficult_functions.
                    VACUUM_MODEL=<model> switches the agent model. This is an
                    escalation for functions that have already failed, not a
                    default: a stronger model costs more per run, and ordinary
                    sweeps match most functions in one or two attempts on the
                    default model anyway. It earns its cost here because a
                    give-up at >=95% after ~25 attempts *and* a permuter run is
                    usually blocked on an unguessed C idiom rather than on
                    search - measured on this list, a model switch matched 8 of
                    8, including one function stalled at 99.928% after 40
                    attempts that matched on its first.
                    A verified match removes that name from the list.
  --overlay NAME    Only pick functions from this overlay (gameplay, USA/main,
                    asm path suffix). Combined with --difficult: intersection.
  -h, --help        Show this help

Environment:
  VACUUM_CLI               Default CLI when no flag is given (claude | grok)
  VACUUM_PERMUTE_TIMEOUT   Permuter cap in seconds (default 360)
  VACUUM_PERMUTE_JOBS      Permuter threads (default: min(nproc, 8))
  VACUUM_STREAM            0 disables claude's streamed per-step logging
  VACUUM_STREAM_QUIET      Non-empty: log tool calls only, no commentary
  VACUUM_WORKTREE_PARENT   Directory for pe2-wt-<func> worktrees
                           (default: parent of this repo)
  VACUUM_ORCH_STATE        Override orchestrator JSON path
  VACUUM_LAND_MODEL        Model for landing work - the port agent and the
                    overlay drift landing. Splicing a verified body into the
                    real file and fixing includes is mechanical, so this can be
                    a cheaper model than VACUUM_MODEL. Defaults to VACUUM_MODEL.
  VACUUM_PORT_MAX_TURNS    Max turns for the port agent (default 80)
  VACUUM_PORT_TRIES        Failed trunk landings before marking difficult
                           (default 2). Stops the same claimed function from
                           being retried forever after a rejected port.
  VACUUM_MERGE_WAIT        Seconds to wait for the merge lock (default 3600)
  VACUUM_GROK_EFFORT       Grok --effort for match/port (default: xhigh)
  GROK_MATCH_EFFORT        Same, used by tools/claude when launching grok

Give-up seeds are stored under tools/giveups/<func>/ (gitignored).
Orchestrator sessions log to tools/vacuum-<cli>-<pid>.log and flock-append
each function onto tools/vacuum.log.
EOF
}

trap 'echo ""; echo "Interrupt received, will stop after the current function. An unfinished match is not marked difficult."; STOP_REQUESTED=1' INT
trap 'rm -f "${SKIP_FILE:-}"; orch_cleanup' EXIT

while [[ $# -gt 0 ]]; do
  case $1 in
    --times)
      if [[ $# -lt 2 ]]; then
        echo "Error: --times requires a value"
        usage
        exit 1
      fi
      MAX_TIMES="$2"
      shift 2
      ;;
    --cli)
      if [[ $# -lt 2 ]]; then
        echo "Error: --cli requires a value (claude or grok)"
        usage
        exit 1
      fi
      CLI="$2"
      shift 2
      ;;
    --claude)
      CLI="claude"
      shift
      ;;
    --grok)
      CLI="grok"
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --keep-scratch)
      KEEP_SCRATCH=1
      shift
      ;;
    --no-permute)
      PERMUTER=0
      shift
      ;;
    --orchestrator)
      ORCH=1
      shift
      ;;
    --difficult|--only-difficult)
      ONLY_DIFFICULT=1
      shift
      ;;
    --overlay)
      if [[ $# -lt 2 ]]; then
        echo "Error: --overlay requires a value (e.g. gameplay, USA/main)"
        usage
        exit 1
      fi
      OVERLAY="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
done

if [[ -n "$MAX_TIMES" ]] && ! [[ "$MAX_TIMES" =~ ^[0-9]+$ ]]; then
  echo "Error: --times argument must be a positive integer"
  exit 1
fi

case "$CLI" in
  claude|grok) ;;
  *)
    echo "Error: unsupported CLI '$CLI' (expected claude or grok)"
    usage
    exit 1
    ;;
esac

if [[ $DRY_RUN -eq 0 ]] && ! command -v "$CLI" &>/dev/null; then
  echo "Error: '$CLI' not found in PATH"
  exit 1
fi

if [[ -n "$OVERLAY" ]]; then
  if ! python3 "$OVERLAY_PY" list-nonmatchings --overlay "$OVERLAY" >/dev/null; then
    exit 1
  fi
fi

SKIP_FILE=$(mktemp "${TMPDIR:-/tmp}/vacuum-skip.XXXXXX")

vacuum_filter_desc() {
  local parts=()
  if [[ -n "$OVERLAY" ]]; then
    parts+=("overlay=$OVERLAY")
  fi
  if [[ $ONLY_DIFFICULT -eq 1 ]]; then
    parts+=("difficult only")
  fi
  if [[ ${#parts[@]} -eq 0 ]]; then
    return 0
  fi
  local IFS=', '
  echo "Filter: ${parts[*]}"
}

list_vacuum_nonmatchings() {
  local args=()
  if [[ -n "$OVERLAY" ]]; then
    args+=(--overlay "$OVERLAY")
  fi
  python3 "$OVERLAY_PY" list-nonmatchings "${args[@]}"
}

# Functions whose body is already decompiled in another overlay. Overlays in a
# family share a lot of code - of the first 158 room functions matched, only 29
# were distinct bodies, and one two-instruction stub was matched 112 times.
# Matching such a function again derives nothing: it wants promoting into the
# shared library instead (overlay_dup_index.py promote).
solved_elsewhere_file() {
  local out
  out=$(mktemp "${TMPDIR:-/tmp}/vacuum-solved.XXXXXX")
  if ! python3 tools/overlay_dup_index.py solved --rebuild >"$out" 2>/dev/null; then
    : >"$out"      # index unavailable: fall through rather than block the vacuum
  fi
  echo "$out"
}

# Copies of this body in other overlays of the same family. Written to a file so
# the caller can both count it and paste it into a prompt. Empty is the normal
# case; a non-empty list means the match must be promoted into the shared
# library rather than left as one overlay's copy.
# Overlays carrying this body that some *other* session currently holds. Step 4b
# tells the agent to promote a shared body, which rewrites those overlays'
# manifest spans and moves their copy into src/<family>/lib. Fine when nothing
# else is working them, a collision when something is: mist_r18's sweep promoted
# a body it shared with shelter_b1_sterilization_room, and had that overlay been
# leased at the time, the other session's landing would have found its files
# rewritten underneath it.
siblings_claimed_elsewhere() {
  local siblings=$1
  [[ -s "$siblings" ]] || return 1
  python3 - "$siblings" "$SESSION" <<'PYEOF' 2>/dev/null
import json, subprocess, sys
sib, mine = sys.argv[1], sys.argv[2]
overlays = {l.split("\t")[0].split("/")[-1] for l in open(sib) if l.strip()}
try:
    gc = subprocess.run(["git", "rev-parse", "--git-common-dir"],
                        capture_output=True, text=True).stdout.strip()
    claims = json.load(open(f"{gc}/vacuum-orch.json")).get("claims", {})
except Exception:
    sys.exit(1)
busy = {c.get("overlay") for c in claims.values()
        if c.get("session") != mine and c.get("overlay")}
hit = sorted(overlays & busy)
print(" ".join(hit))
sys.exit(0 if hit else 1)
PYEOF
}

shared_siblings_file() {
  local func=$1
  local out
  out=$(mktemp "${TMPDIR:-/tmp}/vacuum-siblings.XXXXXX")
  if ! python3 tools/overlay_dup_index.py siblings "$func" --rebuild >"$out" 2>/dev/null; then
    : >"$out"
  fi
  echo "$out"
}

pick_simplest_func() {
  local extra=()
  if [[ $ONLY_DIFFICULT -eq 1 ]]; then
    extra+=(--only-difficult)
  fi
  if [[ -n "$SKIP_FILE" && -f "$SKIP_FILE" ]]; then
    extra+=(--exclude-file "$SKIP_FILE")
  fi
  local solved
  solved=$(solved_elsewhere_file)
  if [[ -s "$solved" ]]; then
    echo "Skipping $(wc -l <"$solved") function(s) already matched in another overlay." >&2
    extra+=(--exclude-file "$solved")
  fi
  python3 tools/score_functions.py "${extra[@]}" "$@"
  rm -f "$solved"
}

# "function 7 of 46" when the caller knows the population size. A whole-overlay
# sweep does: tools/vacuum_overlay.sh passes VACUUM_TOTAL so the log says where
# it is rather than only what it is doing, which over an 8-hour run is the
# difference between reading progress and guessing at it.
# The agent tag written on every run line. VACUUM_MODEL is what actually
# decides cost, and the CLI name alone does not reveal it - the Fable
# escalation runs all logged as "claude", so their spend could only be told
# apart by which file it landed in.
agent_tag() {
  if [[ -n "${VACUUM_MODEL:-}" ]]; then
    echo "$CLI/$VACUUM_MODEL"
  else
    echo "$CLI"
  fi
}

progress_tag() {
  local n=$(( ${1:-0} + 1 ))
  if [[ -n "${VACUUM_TOTAL:-}" ]]; then
    echo "[$n of $VACUUM_TOTAL] "
  else
    echo "[$n] "
  fi
}

note_skip() {
  local func=$1
  [[ -n "$SKIP_FILE" ]] || return 0
  echo "$func" >>"$SKIP_FILE"
}

difficult_listed() {
  local func=$1
  [[ -f "$DIFFICULT_FUNCTIONS" ]] || return 1
  awk -v f="$func" '$1 == f { found = 1 } END { exit !found }' "$DIFFICULT_FUNCTIONS"
}

forget_difficult_entry() {
  local func=$1
  # Also clear the real list when a retry pass was aimed at a subset via
  # VACUUM_DIFFICULT_FILE. Otherwise a match removes the name from the override
  # file and leaves it in tools/difficult_functions, where it keeps advertising
  # a give-up for a function that is now matched - func_800E06AC and
  # func_800DE150 both landed from a near-miss pass and stayed listed at 99.167%
  # and 99.565%.
  if [[ "$DIFFICULT_FUNCTIONS" != "tools/difficult_functions" ]] \
     && [[ -f tools/difficult_functions ]]; then
    awk -v f="$func" 'NF && $1 == f { next } { print }' tools/difficult_functions \
      >tools/difficult_functions.tmp \
      && mv tools/difficult_functions.tmp tools/difficult_functions
  fi
  [[ -f "$DIFFICULT_FUNCTIONS" ]] || return 0
  difficult_listed "$func" || return 0
  awk -v f="$func" 'NF && $1 == f { next } { print }' "$DIFFICULT_FUNCTIONS" \
    >"${DIFFICULT_FUNCTIONS}.tmp" \
    && mv "${DIFFICULT_FUNCTIONS}.tmp" "$DIFFICULT_FUNCTIONS"
  echo "Removed $func from $DIFFICULT_FUNCTIONS" | tee -a "$LOG_FILE"
}

orch_claim_args() {
  local args=(claim --session "$SESSION" --pid $$ --cli "$CLI")
  if [[ $ONLY_DIFFICULT -eq 1 ]]; then
    args+=(--only-difficult)
  fi
  if [[ -n "$OVERLAY" ]]; then
    args+=(--overlay "$OVERLAY")
  fi
  if [[ -n "$SKIP_FILE" && -f "$SKIP_FILE" ]]; then
    args+=(--exclude-file "$SKIP_FILE")
  fi
  printf '%s\n' "${args[@]}"
}

MAIN_LOG_FILE="$ROOT/tools/vacuum.log"
SESSION="vacuum-${CLI}-$$"
WORKTREE_PARENT="${VACUUM_WORKTREE_PARENT:-$(dirname "$ROOT")}"
LOG_FLUSH_POS=0
# VACUUM_LOG_FILE lets a caller point this run's log somewhere else. A driver
# that runs the vacuum inside a worktree - tools/vacuum_overlay.sh - otherwise
# has to capture the whole run through a pipe and only sees it when the pipe
# closes, so the per-function agent output lands in one block at the end. With
# the override the vacuum's own `tee -a` streams into the caller's log live.
if [[ -n "${VACUUM_LOG_FILE:-}" ]]; then
  LOG_FILE="$VACUUM_LOG_FILE"
elif [[ $ORCH -eq 1 ]]; then
  LOG_FILE="$ROOT/tools/vacuum-${CLI}-$$.log"
else
  LOG_FILE="$MAIN_LOG_FILE"
fi

include_asm_present() {
  local func=$1
  local repo=${2:-$PWD}
  grep -rE "INCLUDE_ASM[[:space:]]*\\([[:space:]]*\"[^\"]+\"[[:space:]]*,[[:space:]]*${func}[[:space:]]*\\)" \
    "$repo/src" --include='*.c' -l >/dev/null 2>&1
}

count_attempts() {
  local scratch=$1
  if [[ -f "$scratch/match_log.txt" ]]; then
    wc -l <"$scratch/match_log.txt" | tr -d ' '
    return
  fi
  local n
  n=$(find "$scratch" -maxdepth 1 -name 'base_*.c' 2>/dev/null | wc -l | tr -d ' ')
  echo $((n + 1))
}

MATCH_LOOP_MD="$ROOT/tools/claude-decomp-env/MATCH_LOOP.md"

# Substitute $functionName in MATCH_LOOP.md. Empty func leaves the placeholder.
match_loop_text() {
  local func=${1:-}
  if [[ -f "$MATCH_LOOP_MD" ]]; then
    sed "s/\\\$functionName/${func}/g" "$MATCH_LOOP_MD"
  else
    echo "ERROR: missing $MATCH_LOOP_MD" >&2
  fi
}

run_agent() {
  local prompt=$1
  local cwd=${2:-$PWD}
  local extra=()
  # Matching and landing are different jobs. Matching is the search that needs
  # the strong model; landing is mechanical - splice the verified body into the
  # real file, fix includes, rebuild, commit - and a call site can ask for a
  # cheaper model by setting AGENT_MODEL.
  local model="${AGENT_MODEL-${VACUUM_MODEL:-}}"
  local grok_rules=""
  if [[ -n "${AGENT_MAX_TURNS:-}" ]]; then
    extra+=(--max-turns "$AGENT_MAX_TURNS")
  fi
  if [[ -n "${AGENT_FUNC:-}" ]]; then
    grok_rules=$(match_loop_text "$AGENT_FUNC")
  elif [[ -f "$MATCH_LOOP_MD" ]]; then
    grok_rules=$(match_loop_text)
  fi
  (
    cd "$cwd" || exit 1
    # Grok's tool subprocesses often get a clean PATH without the caller's
    # venv, so ninja_config.py dies on splat/spimdisasm and the agent then
    # "verifies" against leftover build/USA/out binaries.
    if [[ -d venv/bin ]]; then
      export VIRTUAL_ENV="$PWD/venv"
      export PATH="$PWD/venv/bin:$PATH"
      # shellcheck disable=SC1091
      source venv/bin/activate
    elif [[ -d .venv/bin ]]; then
      export VIRTUAL_ENV="$PWD/.venv"
      export PATH="$PWD/.venv/bin:$PATH"
      # shellcheck disable=SC1091
      source .venv/bin/activate
    fi
    case "$CLI" in
      claude)
        # Plain `claude -p` only prints the final result, so an iteration looks
        # frozen in the log until it ends. Stream the events and format them the
        # way grok's live output reads. VACUUM_STREAM=0 restores the old output.
        if [[ "${VACUUM_STREAM:-1}" != "0" ]]; then
          claude -p ${model:+--model "$model"} --verbose --output-format stream-json \
            --dangerously-skip-permissions "$prompt" \
            | python3 tools/stream_format.py ${VACUUM_STREAM_QUIET:+--quiet-text}
        else
          claude -p ${model:+--model "$model"} --dangerously-skip-permissions "$prompt"
        fi
        ;;
      grok)
        # grok -p cwd is the worktree/repo root, so scratch CLAUDE.md is not
        # auto-loaded. --rules injects MATCH_LOOP.md into the system prompt.
        # xhigh (override with VACUUM_GROK_EFFORT) is closer to claude ultrathink.
        extra+=(--effort "${VACUUM_GROK_EFFORT:-xhigh}" --cwd "$cwd")
        if [[ -n "$grok_rules" ]]; then
          extra+=(--rules "$grok_rules")
        fi
        grok --always-approve "${extra[@]}" -p "$prompt"
        ;;
    esac
  )
}

# Shared with non-orch and orch match prompts. Claude also gets this via scratch
# CLAUDE.md; grok -p does not auto-read that file, so the vacuum prompt must
# carry MATCH_LOOP.md or it only stares at asm-differ.
# Overlay basename to pass to build-and-verify.sh --only. decomp_overlay.py
# writes it into the brief, so the agent and the vacuum agree on the scope
# without re-deriving it from paths.
build_scope() {
  local bf="$1"
  [ -f "$bf" ] || return 0
  sed -n 's/^- Build scope: `\(.*\)`$/\1/p' "$bf" | head -1
}

dump_loop_instructions() {
  local func=${1:-${AGENT_FUNC:-}}
  match_loop_text "$func"
}

build_prompt() {

  # 4b promotes a shared body, rewriting every carrier overlay. Only let that
  # happen when no other session holds one of them.
  local promote_guard="" _sibs _busy
  _sibs=$(shared_siblings_file "$1")
  if _busy=$(siblings_claimed_elsewhere "$_sibs"); then
    promote_guard="4b-OVERRIDE. This body is shared, but ${_busy} is leased by another session right now. **Do not promote it** - promotion rewrites that overlay's manifest span and moves its copy while that session is mid-match. Land it in this overlay only, skip step 4b below, and name the siblings in your report.
"
  fi
  rm -f "$_sibs"
  local func=$1
  local scratch=$2
  local brief_file=$3
  local scope
  scope=$(build_scope "$brief_file")
  cat <<EOF
Match \`$func\`. The scratch environment is already created at \`$scratch\`.
Do NOT run ./tools/claude or recreate the scratch directory.

Read \`$scratch/BRIEF.md\` (also pasted below), then:

1. cd into \`$scratch\` and make \`base.c\` compile with **minimal** edits (\`./build.sh base.c\`). If this was a give-up retry, \`base.c\` is the archived seed — do not restart from m2c or rewrite from the asm before the first score.
2. $(dump_loop_instructions "$func")
3. If the best score is ≥ 95% and leftover diffs are registers / scheduling / stack (\`branch\`=\`insert\`=\`delete\`=0), run the permuter from the repo root **before** adding register pins:
   \`./permute.sh --run --timeout 360 -j4 $func <asm path from BRIEF> $scratch/base_N.c\`
4. On 100%: replace INCLUDE_ASM in the host C file, fix headers in this overlay's include/ tree, then verify twice — \`./tools/build-and-verify.sh${scope:+ --only $scope}\` for a fast check of this overlay, then the bare \`./tools/build-and-verify.sh\` before you commit, since a scoped pass says nothing about the overlays it skipped. Commit \`matched $func <attempts>\`.
${promote_guard}4b. Then check whether other overlays carry the same body: \`python3 tools/overlay_dup_index.py find $func\`. If they do, promote it so it is matched once — \`python3 tools/overlay_dup_index.py promote $func\` writes the spans and shared symbols, and tells you to move the C body into \`src/<family>/lib/<unit>.c\` and out of this overlay's own .c. Rebuild, verify unscoped, and include it in the same commit.
5. On stall: append \`tools/difficult_functions\` as \`$func <attempts> <best%>\`, revert host C, do not leave INCLUDE_ASM replaced.
6. Leave the scratch directory (including the best unpinned \`base_N.c\`). Vacuum will run the permuter after you exit, then clean up.

---
$(cat "$brief_file")
EOF
}

build_permute_prompt() {
  local func=$1
  local scratch=$2
  local seed=$3
  local winner=$4
  local scope
  scope=$(build_scope "$scratch/BRIEF.md")
  cat <<EOF
decomp-permuter found a **score 0** candidate for \`$func\` after the matching session stalled.

Scratch: \`$scratch\`
Seed C: \`$seed\`
Winning preprocessed function: \`$winner\`
Report: \`$scratch/PERMUTER.txt\`

Port the permuter's transformations into the seed (or a new \`$scratch/base_N.c\`).
Do NOT add \`register … asm("")\` pins. Do NOT re-run \`./tools/claude\` or the permuter.
Confirm \`./build.sh <file>.c\` in the scratch dir reports 100%, then replace INCLUDE_ASM
in the host C file, run \`./tools/build-and-verify.sh${scope:+ --only $scope}\` and then the
bare \`./tools/build-and-verify.sh\`, and commit \`matched $func permute\`.
Leave the scratch directory.
EOF
}

try_permuter_poststep() {
  local func=$1
  local scratch=$2
  local repo=${3:-$PWD}
  if [[ $PERMUTER -eq 0 ]]; then
    return 1
  fi
  if [[ ! -d "$scratch" ]]; then
    echo "Scratch gone; skipping permuter post-step" | tee -a "$LOG_FILE"
    return 1
  fi
  if ! include_asm_present "$func" "$repo"; then
    return 1
  fi

  echo "Running permuter post-step for $func..." | tee -a "$LOG_FILE"
  local out st
  out=$(
    cd "$repo" || exit 1
    python3 tools/vacuum_permute.py --func "$func" --scratch "$scratch" \
      --timeout "${VACUUM_PERMUTE_TIMEOUT:-360}" \
      --jobs "${VACUUM_PERMUTE_JOBS:-0}"
  2>&1 | tee -a "$LOG_FILE")
  st=${PIPESTATUS[0]}
  if [[ $st -ne 0 ]]; then
    return 1
  fi
  local winner seed
  winner=$(echo "$out" | sed -n 's/^PERMUTER_HIT=//p' | tail -1)
  seed=$(echo "$out" | sed -n 's/^PERMUTER_SEED=\([^ ]*\).*/\1/p' | tail -1)
  if [[ -z "$winner" || ! -f "$winner" ]]; then
    return 1
  fi
  echo "Permuter hit score 0 ($winner); launching a port follow-up..." | tee -a "$LOG_FILE"
  AGENT_FUNC="$func" AGENT_MAX_TURNS=40 run_agent "$(build_permute_prompt "$func" "$scratch" "$seed" "$winner")" "$repo" | tee -a "$LOG_FILE"
  return 0
}

# Commit tools/difficult_functions on its own if it has pending changes. Used
# where the match itself was committed by the agent, so there is no commit of
# ours to fold the list change into.
commit_difficult_list() {
  local why=$1
  git diff --quiet -- tools/difficult_functions 2>/dev/null && return 0
  git add -- tools/difficult_functions 2>/dev/null || return 0
  git commit -q -m "difficult_functions: $why" >/dev/null 2>&1 \
    && echo "Committed tools/difficult_functions ($why)" | tee -a "$LOG_FILE"
}

commit_match_if_needed() {
  # 0 = we verified + committed; 2 = agent already committed (still verify);
  # 1 = no integrated match.
  local func=$1
  local scratch=$2
  local attempts
  attempts=$(count_attempts "$scratch")

  # Look across every commit this iteration made, not just HEAD. Agents are
  # asked to record findings in DECOMPILATION_LEARNINGS.md, so a match is
  # routinely followed by a `learnings: ...` commit - and with only `git log -1`
  # the match underneath went unseen. The function was then treated as
  # unmatched, nothing was left to stage, and it was filed as a give-up at
  # 100.000%: func_acropolis_patio_8017DBAC landed and was marked difficult in
  # the same run.
  local range="HEAD"
  if [[ -n "${ITER_BASE_SHA:-}" ]]; then
    range="${ITER_BASE_SHA}..HEAD"
  fi
  if git log $range --pretty=%s 2>/dev/null | grep -qE "^matched ${func}( |$)"; then
    echo "Agent already committed a match for $func" | tee -a "$LOG_FILE"
    # Clear the give-up here too. Most agents commit their own match, so this is
    # the common path, and returning straight out of it left the function listed
    # as a give-up after it had been matched - which then advertises it to the
    # next retry pass as a promising candidate.
    forget_difficult_entry "$func"
    commit_difficult_list "cleared after $func matched"
    return 2
  fi

  if include_asm_present "$func"; then
    return 1
  fi

  echo "INCLUDE_ASM for $func is gone; verifying before auto-commit..." | tee -a "$LOG_FILE"
  # Deliberately unscoped. The agent's inner loop uses --only, but this is the
  # vacuum's own check before it commits, and a scoped run cannot see an
  # overlay the change broke somewhere else.
  if ! ./tools/build-and-verify.sh; then
    echo "Verify failed after INCLUDE_ASM removal; discarding uncommitted match" | tee -a "$LOG_FILE"
    return 1
  fi

  git add -A -- "${MATCH_LAND_PATHS[@]}" 2>/dev/null || true
  if git diff --cached --quiet; then
    echo "No staged match files to commit" | tee -a "$LOG_FILE"
    return 1
  fi
  forget_difficult_entry "$func"
  # Stage the real list as well as whatever DIFFICULT_FUNCTIONS points at. With
  # VACUUM_DIFFICULT_FILE set it points at a scratch file outside the repo, so
  # `git add` on it silently staged nothing and the edit to
  # tools/difficult_functions was left dirty in the working tree.
  git add -- "$DIFFICULT_FUNCTIONS" 2>/dev/null || true
  git add -- tools/difficult_functions 2>/dev/null || true
  git commit -m "matched $func $attempts"
  echo "Auto-committed matched $func $attempts" | tee -a "$LOG_FILE"
  return 0
}

best_score() {
  # Highest percentage seen in match_log.txt (scratch first, then the giveup
  # archive, which is written just before this runs).
  local func=$1
  local scratch=$2
  local log
  for log in "$scratch/match_log.txt" "tools/giveups/$func/match_log.txt"; do
    if [[ -f "$log" ]]; then
      awk 'BEGIN { best = 0 }
           { s = $2; sub(/%$/, "", s); if (s + 0 > best) best = s + 0 }
           END { printf "%.3f", best }' "$log"
      return
    fi
  done
  echo "0.000"
}

record_difficult_if_needed() {
  # Safety net: agents are told to append difficult_functions on a stall, but
  # not all of them do. Without an entry the next iteration re-picks the same
  # function, so vacuum records the give-up itself.
  local func=$1
  local scratch=$2
  if difficult_listed "$func"; then
    return 0
  fi
  # A give-up is only evidence about the function if something was actually
  # compiled. Without match_log.txt nothing was: count_attempts falls back to
  # base_*.c + 1 and best_score returns 0.000, so an agent that never ran -
  # a CLI out of quota, a crashed session, a scratch env that would not build -
  # gets recorded as "1 attempt, 0.000" and the function is parked for good.
  # That is how 523 functions were parked in a single day while one CLI was out
  # of weekly tokens; 85 of them match a plain seed outright. Leave the function
  # in the pool instead and let the next session pick it up.
  if [[ ! -f "$scratch/match_log.txt" ]] && \
     ! compgen -G "$scratch/base_*.c" >/dev/null 2>&1; then
    echo "Not recording a give-up for $func: no candidate was ever compiled" \
      | tee -a "$LOG_FILE"
    return 0
  fi
  local attempts score
  attempts=$(count_attempts "$scratch")
  score=$(best_score "$func" "$scratch")
  # A perfect score is not a give-up. If we get here at 100.000 the match was
  # real and something upstream failed to notice it, so parking the function
  # would be wrong twice over - it hides a working match and blocks a re-pick.
  if [[ "$score" == "100.000" ]]; then
    echo "Not recording a give-up for $func: best score is 100.000" \
      | tee -a "$LOG_FILE"
    return 0
  fi
  if [[ -s "$DIFFICULT_FUNCTIONS" ]] && [[ -n "$(tail -c 1 "$DIFFICULT_FUNCTIONS")" ]]; then
    echo "" >>"$DIFFICULT_FUNCTIONS"
  fi
  echo "$func $attempts $score" >>"$DIFFICULT_FUNCTIONS"
  echo "Recorded give-up: $func $attempts $score" | tee -a "$LOG_FILE"
}

commit_difficult_if_needed() {
  # $2: 0/2 = the function was matched, anything else = give-up.
  local func=$1
  local match_status=$2
  if [[ $match_status -eq 0 || $match_status -eq 2 ]]; then
    git checkout -- "$DIFFICULT_FUNCTIONS" 2>/dev/null || true
    forget_difficult_entry "$func"
  fi
  if git diff --quiet -- "$DIFFICULT_FUNCTIONS"; then
    return 0
  fi
  git add -- "$DIFFICULT_FUNCTIONS"
  git commit -m "Update $DIFFICULT_FUNCTIONS"
  echo "Committed $DIFFICULT_FUNCTIONS" | tee -a "$LOG_FILE"
}

cleanup_scratch() {
  local func=$1
  local scratch=$2
  local repo=${3:-$PWD}
  if [[ $KEEP_SCRATCH -eq 1 ]]; then
    echo "Keeping scratch at $scratch"
    return
  fi
  rm -rf "$scratch"
  rm -rf "$repo/nonmatchings/${func}" "$repo/nonmatchings/${func}-"*
  rmdir "$repo/nonmatchings" 2>/dev/null || true
  rm -rf "$repo/permuter/${func}"
}

json_get() {
  python3 -c 'import json,sys
v=json.load(sys.stdin).get(sys.argv[1])
if v is None:
    print("")
elif isinstance(v, bool):
    print(str(v).lower())
else:
    print(v)' "$1"
}

orch() {
  python3 "$ROOT/tools/vacuum_orch.py" --root "$ROOT" "$@"
}

flush_vacuum_log() {
  [[ $ORCH -eq 1 ]] || return 0
  [[ -n "${MAIN_LOG_FILE:-}" && -n "${LOG_FILE:-}" ]] || return 0
  local out new
  out=$(orch log-flush --local "$LOG_FILE" --main "$MAIN_LOG_FILE" \
    --offset "${LOG_FLUSH_POS:-0}" --session "$SESSION" --label "${ORCH_FUNC:-}" 2>/dev/null) || return 0
  new=$(echo "$out" | json_get offset)
  if [[ "$new" =~ ^[0-9]+$ ]]; then
    LOG_FLUSH_POS=$new
  fi
}

orch_cleanup() {
  [[ $ORCH -eq 1 ]] || return 0
  flush_vacuum_log
  if [[ $ORCH_MERGE -eq 1 ]]; then
    orch merge-release --session "$SESSION" >/dev/null 2>&1 || true
    ORCH_MERGE=0
  fi
  if [[ -n "$ORCH_FUNC" && $ORCH_FINISHED -eq 0 ]]; then
    orch relinquish --session "$SESSION" --func "$ORCH_FUNC" >/dev/null 2>&1 || true
  fi
}

stale_build_warning() {
  local tree=$1
  cat <<EOF
IMPORTANT — stale build artifacts:
\`build/\` in \`$tree\` already contains binaries that checksum from the INCLUDE_ASM version. \`sha256sum --check\` / an existing \`build/USA/out/SLUS_010.42: OK\` is NOT a match.
The only success is running \`./tools/build-and-verify.sh\` **in \`$tree\`** after your edits and seeing:
  ✅ BUILD SUCCEEDED. Everything matched and there were no compiler or linter errors
That script reformats, regenerates ninja, and rebuilds. If \`python3 ninja_config.py\` fails with missing splat/spimdisasm, use \`venv/bin/python3\`.
EOF
}

build_orch_match_prompt() {
  local func=$1
  local scratch=$2
  local brief_file=$3
  local wt=$4
  cat <<EOF
Match \`$func\` in this disposable worktree.

Worktree (your repo root): \`$wt\`
Scratch (already created): \`$scratch\`
Canonical trunk (do NOT write here): \`$ROOT\`

Do NOT run ./tools/claude or recreate the scratch directory.
Do NOT run git merge / rebase / cherry-pick.
Do NOT edit \`$ROOT\`. Do NOT append tools/difficult_functions (vacuum records give-ups on trunk).

$(stale_build_warning "$wt")

Read \`$scratch/BRIEF.md\` (also pasted below), then:

1. cd into \`$scratch\` and make \`base.c\` compile with minimal edits (\`./build.sh base.c\`). If this was a give-up retry, \`base.c\` is the archived seed — do not restart from m2c or rewrite from the asm before the first score.
2. $(dump_loop_instructions "$func")
3. If the best score is ≥ 95% and leftover diffs are registers / scheduling / stack (\`branch\`=\`insert\`=\`delete\`=0), run the permuter from the worktree root **before** adding register pins:
   \`./permute.sh --run --timeout 360 -j4 $func <asm path from BRIEF> $scratch/base_N.c\`
4. On 100%: replace INCLUDE_ASM in the worktree host C file, fix headers in this overlay's include/ tree, include any \`configs/\` splat or jump-table yaml, run \`./tools/build-and-verify.sh\` **in the worktree**, commit \`matched $func <attempts>\` **on this worktree branch only**.
5. On stall: revert host C in the worktree, do not leave INCLUDE_ASM replaced, leave the scratch (best unpinned \`base_N.c\` included).
6. Leave the scratch directory.

---
$(cat "$brief_file")
EOF
}

build_port_prompt() {
  local func=$1
  local status=$2
  local wt=$3
  local scratch=$4
  local attempts=$5
  local score=$6
  local hint=$7
  local siblings=${8:-}
  local promote_block=""
  if [[ -n "$siblings" && -s "$siblings" ]]; then
    promote_block=$(cat <<PROMOTE

## This body is shared - promote it, do not land it in one overlay

\`$func\` also exists in $(wc -l <"$siblings") other overlay(s) of the same family.
Landing it in its own overlay only leaves the copies to be matched again later,
so promotion is part of this port, not a follow-up:

1. \`python3 tools/overlay_dup_index.py promote $func\` — writes the span into
   every carrying overlay's entry in \`configs/USA/overlays.toml\` and the shared
   symbol into their \`configs/USA/sym/<family>/*.txt\`.
2. Put the matched body in \`src/<family>/lib/<unit>.c\` under the shared symbol
   name it printed, and delete it from this overlay's own \`.c\`.
3. Delete the now-covered \`INCLUDE_ASM\` line for this body from each of the
   other overlays' \`.c\` — their span is carved out, so a leftover declaration
   breaks the link. splat will not do this for you: it never rewrites an
   existing \`.c\`.
4. \`venv/bin/python3 ninja_config.py\` then the unscoped
   \`./tools/build-and-verify.sh\`. All targets must still match.

The copies:

$(cat "$siblings")
PROMOTE
)
  fi
  cat <<EOF
You are the **port** agent for \`$func\`. You are running in the canonical trunk:

  $ROOT

A match session already ran in a disposable worktree. Do **not** git merge, rebase, cherry-pick, or \`git apply\` the worktree onto trunk. Other agents may have landed functions in the same TU / headers since that worktree was created. Port by rewriting current trunk files.

Worktree: \`$wt\`
Scratch: \`$scratch\`
Status from match session: \`$status\`
Attempts: $attempts
Best scratch score: $score

Rules:
- Do not claim another function. Do not start vacuum. Do not create a worktree.
- Do not \`git reset --hard\` except to undo a change **you just made** that failed verify.
- New types stay in this overlay's include/ tree, not include/main/unknown_syms.h.
- No pointer arithmetic with manual offsets.

$(stale_build_warning "$ROOT")

EOF
  if [[ "$status" == "matched" ]]; then
    cat <<EOF
${promote_block}

Port the matched function onto **current** trunk:

1. Read the worktree host C / headers / configs / learnings (and \`$scratch\` winning \`base_N.c\`) as the source of intent.
2. Read the same paths on trunk. Adapt to structs, INCLUDE_ASM sites, splat yaml, and nearby functions that already landed.
3. Replace INCLUDE_ASM for \`$func\` on trunk. Copy any \`configs/\` splat/jump-table edits. Reconcile types rather than duplicating fields.
4. If the old body no longer matches because layout / context changed, keep adapting until \`./tools/build-and-verify.sh\` prints \`✅ BUILD SUCCEEDED\`.
5. Commit \`matched $func $attempts\` on trunk. Run \`venv/bin/python3 ninja_config.py\` if splat still lists this function under nonmatchings.
6. If \`$func\` is listed in \`tools/difficult_functions\`, delete that line and include the file in the match commit (or a follow-up \`Update tools/difficult_functions\`). Vacuum also drops the name after a verified land.
7. Optional learnings: add a note to DECOMPILATION_LEARNINGS.md only if it is still true on trunk.

A diff of what the match session touched in the worktree follows as a **hint**, not a patch:

$hint
EOF
  else
    cat <<EOF
The match session gave up. On **trunk** only:

1. Append \`tools/difficult_functions\` as \`$func $attempts $score\` if that name is not already listed.
2. Commit \`Update tools/difficult_functions\` if you changed the file.
3. Do not copy worktree src/include onto trunk. Do not replace INCLUDE_ASM.

Scratch C stays in the worktree for vacuum to copy to tools/giveups/.
EOF
  fi
}

worktree_path_for() {
  echo "$WORKTREE_PARENT/pe2-wt-$1"
}

remove_match_worktree() {
  local func=$1
  local wt
  wt=$(worktree_path_for "$func")
  if git -C "$ROOT" worktree list --porcelain 2>/dev/null | grep -qxF "worktree $wt"; then
    git -C "$ROOT" worktree remove --force "$wt" 2>/dev/null || true
  fi
  rm -rf "$wt"
  git -C "$ROOT" branch -D "match/${func}" >/dev/null 2>&1 || true
  git -C "$ROOT" worktree prune >/dev/null 2>&1 || true
}

prune_stale_worktrees() {
  local wt func claimed
  claimed=$(orch status 2>/dev/null | python3 -c 'import json,sys
d=json.load(sys.stdin)
print(" ".join(d.get("claims") or []))' 2>/dev/null || true)
  local current=""
  while IFS= read -r line; do
    case "$line" in
      worktree\ *)
        current=${line#worktree }
        ;;
      branch\ refs/heads/match/*)
        func=${line#branch refs/heads/match/}
        if [[ "$current" == *"/pe2-wt-$func" || "$current" == *"/pe2-wt-$func/" ]]; then
          if [[ " $claimed " != *" $func "* ]]; then
            echo "Pruning stale worktree $current ($func)" | tee -a "$LOG_FILE"
            git -C "$ROOT" worktree remove --force "$current" 2>/dev/null || rm -rf "$current"
            git -C "$ROOT" branch -D "match/${func}" >/dev/null 2>&1 || true
          fi
        fi
        current=""
        ;;
      "")
        current=""
        ;;
    esac
  done < <(git -C "$ROOT" worktree list --porcelain 2>/dev/null)
  git -C "$ROOT" worktree prune >/dev/null 2>&1 || true
}

# splat target binaries (configs/USA/{main,title,gameplay}.yaml). ninja_config.py
# wipes asm/ and re-splits from these; a worktree without them cannot configure.
SPLAT_ASSET_TARGETS=(
  assets/USA/main.exe
  assets/USA/pe2pkg/title.pe2pkg
  assets/USA/pe2pkg/gameplay.pe2pkg
)

link_worktree_dir() {
  local src=$1
  local dest=$2
  if [[ ! -e "$src" ]]; then
    return 1
  fi
  if [[ -e "$dest" && ! -L "$dest" ]]; then
    rm -rf "$dest"
  fi
  ln -sfn "$src" "$dest"
}

# git worktree add does not check out submodule contents. Matching needs
# maspsx, m2c, decomp-permuter, and asm-differ.
WORKTREE_SUBMODULES=(
  tools/maspsx
  tools/m2c
  tools/decomp-permuter
  tools/asm-differ
)

populate_worktree_submodules() {
  local wt=$1
  local path marker
  git -C "$wt" submodule update --init --recursive 2>&1 | tee -a "$LOG_FILE" || true
  for path in "${WORKTREE_SUBMODULES[@]}"; do
    case "$path" in
      tools/maspsx) marker=maspsx.py ;;
      tools/m2c) marker=m2c.py ;;
      tools/decomp-permuter) marker=permuter.py ;;
      tools/asm-differ) marker=diff.py ;;
      *) marker="" ;;
    esac
    if [[ -n "$marker" && -e "$wt/$path/$marker" ]]; then
      continue
    fi
    if [[ ! -e "$ROOT/$path" ]]; then
      echo "Error: submodule $path is empty in the worktree and missing on trunk" | tee -a "$LOG_FILE"
      return 1
    fi
    echo "Worktree $path empty after submodule update; linking trunk copy" | tee -a "$LOG_FILE"
    rm -rf "$wt/$path"
    ln -sfn "$ROOT/$path" "$wt/$path"
  done
}

create_match_worktree() {
  local func=$1
  local wt branch rel
  wt=$(worktree_path_for "$func")
  branch="match/${func}"
  remove_match_worktree "$func"
  git -C "$ROOT" worktree add -b "$branch" "$wt" HEAD || return 1
  if ! populate_worktree_submodules "$wt"; then
    remove_match_worktree "$func"
    return 1
  fi

  if ! link_worktree_dir "$ROOT/assets" "$wt/assets"; then
    echo "Error: $ROOT/assets is missing. ninja_config/splat needs overlay binaries:" | tee -a "$LOG_FILE"
    printf '  %s\n' "${SPLAT_ASSET_TARGETS[@]}" | tee -a "$LOG_FILE"
    remove_match_worktree "$func"
    return 1
  fi
  for rel in "${SPLAT_ASSET_TARGETS[@]}"; do
    if [[ ! -e "$wt/$rel" ]]; then
      echo "Error: worktree missing $rel (splat target). Extract overlays on trunk first." | tee -a "$LOG_FILE"
      remove_match_worktree "$func"
      return 1
    fi
  done
  link_worktree_dir "$ROOT/rom" "$wt/rom" || true

  if [[ -d "$ROOT/venv" ]]; then
    ln -sfn "$ROOT/venv" "$wt/venv"
  fi
  if [[ -d "$ROOT/.venv" ]]; then
    ln -sfn "$ROOT/.venv" "$wt/.venv"
  fi
  if [[ -d "$ROOT/tools/giveups/$func" ]]; then
    mkdir -p "$wt/tools/giveups"
    cp -a "$ROOT/tools/giveups/$func" "$wt/tools/giveups/"
  fi
  # Scratch matching needs the .s (and generated headers) *before* any
  # ninja_config. Configure wipes asm/ and re-splits; it runs later when the
  # match/port agent calls build-and-verify.sh.
  if [[ ! -d "$ROOT/asm" ]]; then
    echo "Error: $ROOT/asm is missing; cannot bootstrap $func" | tee -a "$LOG_FILE"
    remove_match_worktree "$func"
    return 1
  fi
  echo "Copying asm/ into $wt ..." | tee -a "$LOG_FILE"
  cp -a "$ROOT/asm" "$wt/asm"
  if [[ -d "$ROOT/build" ]]; then
    cp -a "$ROOT/build" "$wt/build"
  fi
  if [[ -d "$ROOT/linkers" ]]; then
    cp -a "$ROOT/linkers" "$wt/linkers"
  fi
  git -C "$ROOT" rev-parse HEAD >"$wt/.vacuum-base"
}

worktree_hint_diff() {
  local wt=$1
  local base=$2
  git -C "$wt" diff "$base" -- "${MATCH_LAND_PATHS[@]}" 2>/dev/null \
    | python3 -c 'import sys; t=sys.stdin.read(); print((t[:80000] + ("\n...[truncated]..." if len(t)>80000 else "")) or "(no diff)")'
}

copy_giveup_to_main() {
  local func=$1
  local scratch=$2
  python3 "$ROOT/tools/archive_giveup.py" --func "$func" --scratch "$scratch" \
    2>&1 | tee -a "$LOG_FILE" || true
}

port_succeeded() {
  local func=$1
  local status=$2
  if [[ "$status" == "matched" ]]; then
    git -C "$ROOT" log -1 --pretty=%s | grep -qE "^matched ${func}( |$)"
    return
  fi
  difficult_listed "$func"
}

verify_repo() {
  local repo=$1
  local st
  echo "Running build-and-verify in $repo ..." | tee -a "$LOG_FILE"
  # Unscoped on purpose: this is the independent verification, not the loop.
  ( cd "$repo" && ./tools/build-and-verify.sh ) 2>&1 | tee -a "$LOG_FILE"
  st=${PIPESTATUS[0]}
  return "$st"
}

# Copy worktree match files onto trunk when those paths have not moved since
# the worktree was created. Avoids a port agent (and grok's stale-checksum
# "verify") for the common no-divergence case.
fast_port_from_worktree() {
  local func=$1
  local wt=$2
  local base=$3
  local scratch=$4
  local -a changed=()
  mapfile -t changed < <(git -C "$wt" diff --name-only --diff-filter=ACDMR "$base" -- \
    "${MATCH_LAND_PATHS[@]}")
  if [[ ${#changed[@]} -eq 0 ]]; then
    echo "Fast port skipped: worktree has no landable diff for $func" | tee -a "$LOG_FILE"
    return 1
  fi
  if ! git -C "$ROOT" diff --quiet "$base" -- "${changed[@]}"; then
    echo "Fast port skipped: trunk diverged in: ${changed[*]}" | tee -a "$LOG_FILE"
    return 1
  fi
  echo "Fast-porting $func from worktree (trunk files unchanged since worktree base)" | tee -a "$LOG_FILE"
  git -C "$ROOT" checkout "match/${func}" -- "${changed[@]}"
  local st
  ( cd "$ROOT" && commit_match_if_needed "$func" "$scratch" )
  st=$?
  if [[ $st -eq 0 ]]; then
    return 0
  fi
  if [[ $st -eq 2 ]]; then
    if verify_repo "$ROOT"; then
      return 0
    fi
    return 1
  fi
  return 1
}

# Undo this session's failed landing - and nothing else.
#
# `reset --hard $pre_port` used to be unconditional, and $pre_port is captured
# once when the merge lock is taken, long before these call sites. With
# concurrent orchestrator sessions that snapshot goes stale: weapons-1 rolled
# trunk back to its own pre_port and took two commits another session had
# landed in between (`matched func_p08_8011D1D8`, plus a learnings entry) with
# it. Silently - a hard reset reports nothing.
#
# So refuse to drop any commit this session did not make. `$func` is in scope at
# every call site and every landing commit names it, so a commit whose subject
# does not mention it belongs to somebody else and the reset is abandoned;
# the working tree is still cleaned, which is the part that matters for retry.
reset_trunk_to() {
  local rev=$1
  local foreign=0 subj
  while IFS= read -r subj; do
    [[ -z "$subj" ]] && continue
    if [[ "$subj" != *"$func"* ]]; then
      echo "reset_trunk_to: refusing to discard '$subj' (not this session's)" \
        | tee -a "$LOG_FILE"
      foreign=1
    fi
  done < <(git -C "$ROOT" log --format=%s "$rev..HEAD" 2>/dev/null)

  if [[ $foreign -eq 0 ]]; then
    git -C "$ROOT" reset --hard "$rev" >/dev/null 2>&1 || true
  else
    git -C "$ROOT" reset --hard HEAD >/dev/null 2>&1 || true
  fi
  git -C "$ROOT" clean -fd -- "${MATCH_LAND_PATHS[@]}" >/dev/null 2>&1 || true
}

vacuum_orch_loop() {
  echo "Vacuum using CLI: $CLI, model ${VACUUM_MODEL:-default} (orchestrator session $SESSION)" | tee -a "$LOG_FILE"
  vacuum_filter_desc | tee -a "$LOG_FILE"
  echo "Session log: $LOG_FILE" | tee -a "$LOG_FILE"
  echo "Shared log:  $MAIN_LOG_FILE (flock-appended per function)" | tee -a "$LOG_FILE"
  prune_stale_worktrees

  local count=0
  local consecutive_failures=0
  local claim_json claim_code func wt scratch status attempts score hint
  local match_status output exit_code bootstrap_out bootstrap_status
  local port_tries=0
  local port_try_func=""
  local wt_verified_func=""

  while true; do
    if [[ $STOP_REQUESTED -eq 1 ]]; then
      echo "Stopping gracefully."
      break
    fi
    if [[ -n "$MAX_TIMES" ]] && [[ $count -ge "$MAX_TIMES" ]]; then
      echo "Reached maximum iterations: $MAX_TIMES"
      break
    fi

    flush_vacuum_log

    ORCH_FUNC=""
    ORCH_WT=""
    ORCH_SCRATCH=""
    ORCH_BASE=""
    ORCH_FINISHED=0
    ORCH_MERGE=0

    mapfile -t _claim_args < <(orch_claim_args)
    claim_json=$(orch "${_claim_args[@]}")
    claim_code=$?
    echo "$claim_json" | tee -a "$LOG_FILE"
    if [[ $claim_code -eq 3 ]]; then
      echo "Error: $(echo "$claim_json" | json_get error)"
      break
    fi
    if [[ $claim_code -ne 0 ]]; then
      echo "claim failed ($claim_code): $claim_json" | tee -a "$LOG_FILE"
      ((consecutive_failures++)) || true
      if [[ $consecutive_failures -ge 3 ]]; then
        echo ">= 3 consecutive failures detected. Sleeping for 5 minutes..."
        sleep 300
      fi
      ((count++)) || true
      continue
    fi
    func=$(echo "$claim_json" | json_get func)
    if [[ -z "$func" ]]; then
      echo "claim returned no func: $claim_json" | tee -a "$LOG_FILE"
      break
    fi
    ORCH_FUNC=$func
    note_skip "$func"
    echo -e "\n[$(date '+%H:%M:%S')] [$(agent_tag)] [orch] $(progress_tag "$count")Decompiling $func...\n" | tee -a "$LOG_FILE"
    if [[ "$func" != "$port_try_func" ]]; then
      port_tries=0
      port_try_func=$func
      wt_verified_func=""
    fi

    local reused skip_match
    reused=$(echo "$claim_json" | json_get reused)
    wt=$(worktree_path_for "$func")
    skip_match=0
    if [[ "$reused" == "true" && -e "$wt/.git" ]]; then
      echo "Reusing existing worktree $wt" | tee -a "$LOG_FILE"
      skip_match=1
      ORCH_WT=$wt
      if [[ -f "$wt/.vacuum-base" ]]; then
        ORCH_BASE=$(cat "$wt/.vacuum-base")
      else
        ORCH_BASE=$(git -C "$ROOT" rev-parse HEAD)
      fi
      scratch="$wt/nonmatchings/${func}-vacuum"
      if [[ ! -d "$scratch" ]]; then
        scratch=$(find "$wt/nonmatchings" -maxdepth 1 -type d -name "${func}*" 2>/dev/null | head -1)
      fi
      ORCH_SCRATCH=$scratch
    fi

    if [[ $skip_match -eq 0 ]]; then
      wt_verified_func=""
      if ! create_match_worktree "$func"; then
        echo "Error: failed to create worktree for $func" | tee -a "$LOG_FILE"
        orch relinquish --session "$SESSION" --func "$func" >/dev/null 2>&1 || true
        ORCH_FUNC=""
        ((consecutive_failures++)) || true
        ((count++)) || true
        continue
      fi
      wt=$(worktree_path_for "$func")
      ORCH_WT=$wt
      ORCH_BASE=$(cat "$wt/.vacuum-base" 2>/dev/null || git -C "$ROOT" rev-parse HEAD)

      rm -rf "$wt/nonmatchings/${func}" "$wt/nonmatchings/${func}-"*
      bootstrap_out=$(cd "$wt" && ./tools/claude --bootstrap-only --cli "$CLI" --id vacuum "$func" 2>&1)
      bootstrap_status=$?
      echo "$bootstrap_out" | tee -a "$LOG_FILE" >/dev/null
      scratch=$(echo "$bootstrap_out" | awk -F= '/^SCRATCH_DIR=/{print $2}' | tail -1)
      if [[ -n "$scratch" && "$scratch" != /* ]]; then
        scratch="$wt/$scratch"
      fi
      ORCH_SCRATCH=$scratch

      if [[ $bootstrap_status -ne 0 || -z "$scratch" || ! -d "$scratch" ]]; then
        echo "Error: failed to bootstrap scratch env for $func" | tee -a "$LOG_FILE"
        remove_match_worktree "$func"
        orch relinquish --session "$SESSION" --func "$func" >/dev/null 2>&1 || true
        ORCH_FUNC=""
        ORCH_WT=""
        ((consecutive_failures++)) || true
        if [[ $consecutive_failures -ge 3 ]]; then
          echo ">= 3 consecutive failures detected. Sleeping for 5 minutes..."
          sleep 300
        fi
        ((count++)) || true
        continue
      fi

      if [[ $DRY_RUN -eq 1 ]]; then
        echo "----- dry-run orch match prompt for $func -----"
        build_orch_match_prompt "$func" "$scratch" "$scratch/BRIEF.md" "$wt"
        echo "----- end prompt (worktree: $wt scratch: $scratch) -----"
        remove_match_worktree "$func"
        orch relinquish --session "$SESSION" --func "$func" >/dev/null 2>&1 || true
        ORCH_FUNC=""
        ((count++)) || true
        continue
      fi

      ITER_BASE_SHA=$(git -C "$wt" rev-parse HEAD 2>/dev/null || echo "")
      output=$(AGENT_FUNC="$func" run_agent "$(build_orch_match_prompt "$func" "$scratch" "$scratch/BRIEF.md" "$wt")" "$wt" 2>&1 | tee -a "$LOG_FILE")
      exit_code=${PIPESTATUS[0]}
      echo "$output"

      if [[ $STOP_REQUESTED -eq 0 ]]; then
        try_permuter_poststep "$func" "$scratch" "$wt" || true
      fi
    else
      output=""
      exit_code=0
    fi

    match_status=1
    ( cd "$wt" && commit_match_if_needed "$func" "$scratch" )
    match_status=$?
    if [[ $match_status -eq 0 || $match_status -eq 2 ]]; then
      # Agents (especially grok) commit "matched" after sha256-checking leftover
      # build/ artifacts. Vacuum must rebuild before treating it as matched.
      if [[ "$wt_verified_func" == "$func" ]]; then
        status=matched
      elif [[ $match_status -eq 0 ]]; then
        status=matched
        wt_verified_func=$func
      else
        echo "Independently verifying worktree match for $func..." | tee -a "$LOG_FILE"
        git -C "$wt" reset --hard HEAD >/dev/null 2>&1 || true
        git -C "$wt" clean -fd -- "${MATCH_LAND_PATHS[@]}" >/dev/null 2>&1 || true
        if verify_repo "$wt"; then
          status=matched
          wt_verified_func=$func
        else
          echo "Worktree verify failed for $func; agent commit was not a real match" | tee -a "$LOG_FILE"
          status=difficult
          copy_giveup_to_main "$func" "$scratch"
          git -C "$wt" reset --hard "$ORCH_BASE" >/dev/null 2>&1 || true
        fi
      fi
      if [[ "$status" == "matched" ]]; then
        python3 "$ROOT/tools/archive_giveup.py" --func "$func" --clear \
          2>&1 | tee -a "$LOG_FILE" || true
      fi
    elif [[ $STOP_REQUESTED -eq 1 ]]; then
      echo "Interrupted before a match for $func; leaving it unmatched (not difficult)" | tee -a "$LOG_FILE"
      copy_giveup_to_main "$func" "$scratch"
      orch relinquish --session "$SESSION" --func "$func" >/dev/null 2>&1 || true
      if [[ $KEEP_SCRATCH -eq 0 ]]; then
        remove_match_worktree "$func"
      fi
      ORCH_FUNC=""
      ORCH_WT=""
      echo "Stopping gracefully."
      break
    else
      status=difficult
      copy_giveup_to_main "$func" "$scratch"
      ( cd "$wt" && git checkout -- "${MATCH_LAND_PATHS[@]}" "$DIFFICULT_FUNCTIONS" 2>/dev/null ) || true
    fi

    attempts=$(count_attempts "$scratch")
    score=$(best_score "$func" "$scratch")
    hint=$(worktree_hint_diff "$wt" "$ORCH_BASE")

    echo "Acquiring merge lock for $func ($status)..." | tee -a "$LOG_FILE"
    local merge_json merge_code pre_port
    merge_json=$(orch merge-acquire --session "$SESSION" --pid $$ --wait "${VACUUM_MERGE_WAIT:-3600}")
    merge_code=$?
    echo "$merge_json" | tee -a "$LOG_FILE" >/dev/null
    if [[ $merge_code -ne 0 ]]; then
      echo "Error: could not acquire merge lock for $func" | tee -a "$LOG_FILE"
      ((consecutive_failures++)) || true
      ((count++)) || true
      continue
    fi
    ORCH_MERGE=1
    pre_port=$(git -C "$ROOT" rev-parse HEAD)

    # Is this body shared with other overlays? Decided here, between the match
    # and the port, because it changes which port path is legal.
    local siblings=""
    if [[ "$status" == "matched" ]]; then
      siblings=$(cd "$ROOT" && shared_siblings_file "$func")
      if [[ -s "$siblings" ]]; then
        echo "$func is shared with $(wc -l <"$siblings") other overlay(s); it must be promoted, not landed in one." | tee -a "$LOG_FILE"
      fi
    fi

    local did_fast=0
    # The fast path copies the worktree onto trunk verbatim, which lands the
    # body in its own overlay and cannot promote it. Not legal for a shared one.
    if [[ "$status" == "matched" && ! -s "$siblings" ]]; then
      if fast_port_from_worktree "$func" "$wt" "$ORCH_BASE" "$scratch"; then
        did_fast=1
      else
        reset_trunk_to "$pre_port"
      fi
    fi

    if [[ $did_fast -eq 0 ]]; then
      # Hand the merge lock to the agent rather than holding it across the
      # call. build_port_prompt tells the agent to take the lock itself, so
      # holding it here deadlocks: func_p229_8011D860's port agent spent 14
      # minutes polling merge-acquire against its own parent's lock before a
      # human released it. vacuum_overlay.sh's drift path already hands over
      # without the lock, and its agent takes and releases it correctly.
      # Re-acquired below for the driver's own commit steps.
      if [[ $ORCH_MERGE -eq 1 ]]; then
        orch merge-release --session "$SESSION" >/dev/null 2>&1 || true
        ORCH_MERGE=0
      fi
      if [[ "$status" == "matched" ]]; then
        echo "Starting port agent for $func ($status), model ${VACUUM_LAND_MODEL:-${VACUUM_MODEL:-default}}..." | tee -a "$LOG_FILE"
        AGENT_FUNC="$func" AGENT_MAX_TURNS="${VACUUM_PORT_MAX_TURNS:-80}" \
        AGENT_MODEL="${VACUUM_LAND_MODEL-${VACUUM_MODEL:-}}" run_agent \
          "$(build_port_prompt "$func" "$status" "$wt" "$scratch" "$attempts" "$score" "$hint" "$siblings")" \
          "$ROOT" | tee -a "$LOG_FILE"
        if [[ $ORCH_MERGE -eq 0 ]] \
           && orch merge-acquire --session "$SESSION" --pid $$ \
                --wait "${VACUUM_MERGE_WAIT:-3600}" >/dev/null 2>&1; then
          ORCH_MERGE=1
        fi
        if include_asm_present "$func" "$ROOT"; then
          :
        else
          ( cd "$ROOT" && commit_match_if_needed "$func" "$scratch" ) || true
        fi
      else
        if [[ $ORCH_MERGE -eq 0 ]] \
           && orch merge-acquire --session "$SESSION" --pid $$ \
                --wait "${VACUUM_MERGE_WAIT:-3600}" >/dev/null 2>&1; then
          ORCH_MERGE=1
        fi
        echo "Recording give-up for $func on trunk (no port agent)" | tee -a "$LOG_FILE"
        ( cd "$ROOT" && record_difficult_if_needed "$func" "$scratch"
          commit_difficult_if_needed "$func" 1 ) || true
      fi
    fi
    [[ -n "$siblings" ]] && rm -f "$siblings"

    local landed=0
    if port_succeeded "$func" "$status"; then
      if [[ "$status" != "matched" ]]; then
        landed=1
      elif [[ $did_fast -eq 1 ]]; then
        landed=1
      elif verify_repo "$ROOT"; then
        landed=1
      else
        echo "Verify failed after port of $func; resetting trunk to $pre_port" | tee -a "$LOG_FILE"
        reset_trunk_to "$pre_port"
      fi
    fi

    # Drop the give-up listing on trunk while we still hold the merge lock.
    if [[ $landed -eq 1 && "$status" == "matched" ]]; then
      ( cd "$ROOT" && commit_difficult_if_needed "$func" 0 ) || true
    fi

    orch merge-release --session "$SESSION" >/dev/null 2>&1 || true
    ORCH_MERGE=0

    if [[ $landed -eq 1 ]]; then
      orch finish --session "$SESSION" --func "$func" --status "$status" >/dev/null
      ORCH_FINISHED=1
      if [[ $KEEP_SCRATCH -eq 1 ]]; then
        echo "Keeping worktree $wt"
      else
        echo "Removing worktree $wt" | tee -a "$LOG_FILE"
        remove_match_worktree "$func"
      fi
      ORCH_FUNC=""
      ORCH_WT=""
      port_tries=0
      wt_verified_func=""
      consecutive_failures=0
    else
      reset_trunk_to "$pre_port"
      ((port_tries++)) || true
      local max_tries="${VACUUM_PORT_TRIES:-2}"
      if [[ $port_tries -ge $max_tries ]]; then
        echo "Giving up $func after $port_tries failed trunk landings; marking difficult" | tee -a "$LOG_FILE"
        copy_giveup_to_main "$func" "$scratch"
        ( cd "$ROOT" && record_difficult_if_needed "$func" "$scratch"
          commit_difficult_if_needed "$func" 1 ) || true
        orch finish --session "$SESSION" --func "$func" --status difficult >/dev/null
        ORCH_FINISHED=1
        if [[ $KEEP_SCRATCH -eq 1 ]]; then
          echo "Keeping worktree $wt"
        else
          echo "Removing worktree $wt" | tee -a "$LOG_FILE"
          remove_match_worktree "$func"
        fi
        ORCH_FUNC=""
        ORCH_WT=""
        port_tries=0
        wt_verified_func=""
        consecutive_failures=0
      else
        echo "Port did not land $func ($status); keeping claim for retry $port_tries/$max_tries ($wt)" | tee -a "$LOG_FILE"
        ((consecutive_failures++)) || true
        if [[ $consecutive_failures -ge 3 ]]; then
          echo ">= 3 consecutive failures detected. Sleeping for 5 minutes..."
          sleep 300
        fi
      fi
    fi

    if [[ $STOP_REQUESTED -eq 1 ]]; then
      echo "Stopping gracefully."
      break
    fi
    if [[ $exit_code -ne 0 && $ORCH_FINISHED -eq 0 ]]; then
      ((consecutive_failures++)) || true
    fi
    flush_vacuum_log
    ((count++)) || true
  done

  echo "Total iterations: $count" | tee -a "$LOG_FILE"
  flush_vacuum_log
}


if [[ $ORCH -eq 1 ]]; then
  vacuum_orch_loop
  exit $?
fi

echo "Vacuum using CLI: $CLI, model ${VACUUM_MODEL:-default}" | tee -a "$LOG_FILE"
vacuum_filter_desc | tee -a "$LOG_FILE"

count=0
consecutive_failures=0

while true; do
  if [[ $STOP_REQUESTED -eq 1 ]]; then
    echo "Stopping gracefully."
    break
  fi

  if [[ -n "$MAX_TIMES" ]] && [[ $count -ge "$MAX_TIMES" ]]; then
    echo "Reached maximum iterations: $MAX_TIMES"
    break
  fi

  mapfile -t NONMATCHING_DIRS < <(list_vacuum_nonmatchings)
  if [[ ${#NONMATCHING_DIRS[@]} -eq 0 ]]; then
    if [[ -n "$OVERLAY" ]]; then
      echo "Error: No nonmatchings directories found for overlay $OVERLAY"
    else
      echo "Error: No nonmatchings directories found under asm/"
    fi
    break
  fi

  # Capture stdout only. pick_simplest_func writes progress notes to stderr -
  # "Skipping N function(s) already matched in another overlay." - and folding
  # those into the capture made them part of the function name, so the very next
  # step failed with "failed to bootstrap scratch env for Skipping 677 ...".
  # Errors are still detected, from whichever stream they arrive on.
  _pick_err=$(mktemp "${TMPDIR:-/tmp}/vacuum-pick.XXXXXX")
  simplest_func=$(pick_simplest_func "${NONMATCHING_DIRS[@]}" 2>"$_pick_err")
  [[ -s "$_pick_err" ]] && tee -a "$LOG_FILE" <"$_pick_err"
  if [[ -z "$simplest_func" ]] || grep -qF "Error:" "$_pick_err" \
     || echo "$simplest_func" | grep -qF "Error:"; then
    echo "$simplest_func"
    rm -f "$_pick_err"
    break
  fi
  rm -f "$_pick_err"

  note_skip "$simplest_func"
  echo -e "\n[$(date '+%H:%M:%S')] [$(agent_tag)] $(progress_tag "$count")Decompiling $simplest_func...\n" | tee -a "$LOG_FILE"

  rm -rf "nonmatchings/${simplest_func}" "nonmatchings/${simplest_func}-"*
  bootstrap_out=$(./tools/claude --bootstrap-only --cli "$CLI" --id vacuum "$simplest_func" 2>&1 | tee -a "$LOG_FILE")
  bootstrap_status=${PIPESTATUS[0]}
  scratch=$(echo "$bootstrap_out" | awk -F= '/^SCRATCH_DIR=/{print $2}' | tail -1)

  if [[ $bootstrap_status -ne 0 || -z "$scratch" || ! -d "$scratch" ]]; then
    echo "Error: failed to bootstrap scratch env for $simplest_func" | tee -a "$LOG_FILE"
    ((consecutive_failures++)) || true
    if [[ $consecutive_failures -ge 3 ]]; then
      echo ">= 3 consecutive failures detected. Sleeping for 5 minutes..."
      sleep 300
    fi
    ((count++)) || true
    continue
  fi

  prompt=$(build_prompt "$simplest_func" "$scratch" "$scratch/BRIEF.md")

  if [[ $DRY_RUN -eq 1 ]]; then
    echo "----- dry-run prompt for $simplest_func -----"
    echo "$prompt"
    echo "----- end prompt (scratch: $scratch) -----"
    if [[ $PERMUTER -eq 1 ]]; then
      echo "(after a ≥95% give-up, vacuum would run tools/vacuum_permute.py then a port follow-up)"
    fi
    cleanup_scratch "$simplest_func" "$scratch"
    ((count++)) || true
    continue
  fi

  ITER_BASE_SHA=$(git rev-parse HEAD 2>/dev/null || echo "")
  output=$(AGENT_FUNC="$simplest_func" run_agent "$prompt" 2>&1 | tee -a "$LOG_FILE")
  exit_code=${PIPESTATUS[0]}
  echo "$output"

  if [[ $STOP_REQUESTED -eq 0 ]]; then
    try_permuter_poststep "$simplest_func" "$scratch" || true
  fi

  match_status=1
  commit_match_if_needed "$simplest_func" "$scratch"
  match_status=$?
  if [[ $match_status -eq 0 || $match_status -eq 2 ]]; then
    python3 tools/archive_giveup.py --func "$simplest_func" --clear \
      2>&1 | tee -a "$LOG_FILE" || true
  elif [[ $STOP_REQUESTED -eq 1 ]]; then
    echo "Interrupted before a match for $simplest_func; leaving it unmatched (not difficult)" | tee -a "$LOG_FILE"
    python3 tools/archive_giveup.py --func "$simplest_func" --scratch "$scratch" \
      2>&1 | tee -a "$LOG_FILE" || true
    git reset --hard HEAD >/dev/null
    git clean -fd -- "${MATCH_LAND_PATHS[@]}" >/dev/null 2>&1 || true
    cleanup_scratch "$simplest_func" "$scratch"
    echo "Stopping gracefully."
    break
  else
    python3 tools/archive_giveup.py --func "$simplest_func" --scratch "$scratch" \
      2>&1 | tee -a "$LOG_FILE" || true
    record_difficult_if_needed "$simplest_func" "$scratch"
  fi
  commit_difficult_if_needed "$simplest_func" "$match_status"

  git reset --hard HEAD >/dev/null
  git clean -fd -- "${MATCH_LAND_PATHS[@]}" >/dev/null 2>&1 || true
  cleanup_scratch "$simplest_func" "$scratch"

  # 0 already ran verify before auto-commit; 1/2 still need a tree check.
  if [[ $match_status -ne 0 ]]; then
    ./tools/build-and-verify.sh || break
  fi

  if [[ $STOP_REQUESTED -eq 1 ]]; then
    echo "Stopping gracefully."
    break
  fi

  if [[ $exit_code -ne 0 ]]; then
    ((consecutive_failures++)) || true
    if [[ $consecutive_failures -ge 3 ]]; then
      echo ">= 3 consecutive failures detected. Sleeping for 5 minutes..."
      sleep 300
    fi
  else
    consecutive_failures=0
  fi

  ((count++)) || true
done

echo "Total iterations: $count"
