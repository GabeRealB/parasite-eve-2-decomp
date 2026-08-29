#!/usr/bin/env bash
#
# Vacuum: pick the easiest unmatched function across every overlay, bootstrap a
# scratch env, pack a brief, and hand it to an agent CLI.
#
# Usage:
#   ./tools/vacuum.sh [--cli claude|grok] [--claude] [--grok] [--times N]
#                     [--dry-run] [--keep-scratch] [--no-permute]
#                     [--orchestrator]
#
# Environment:
#   VACUUM_CLI   Default CLI when no --cli/--claude/--grok flag is given
#                (claude | grok). Defaults to claude.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

STOP_PHRASE="Error: All functions are marked as difficult!"
MAX_TIMES=""
STOP_REQUESTED=0
DRY_RUN=0
KEEP_SCRATCH=0
PERMUTER=1
DIFFICULT_FUNCTIONS="tools/difficult_functions"
CLI="${VACUUM_CLI:-claude}"
LOG_FILE=""
OVERLAY_PY="tools/decomp_overlay.py"
ORCH=0
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
trap 'orch_cleanup' EXIT

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

MAIN_LOG_FILE="$ROOT/tools/vacuum.log"
SESSION="vacuum-${CLI}-$$"
WORKTREE_PARENT="${VACUUM_WORKTREE_PARENT:-$(dirname "$ROOT")}"
LOG_FLUSH_POS=0
if [[ $ORCH -eq 1 ]]; then
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
          claude -p --verbose --output-format stream-json \
            --dangerously-skip-permissions "$prompt" \
            | python3 tools/stream_format.py ${VACUUM_STREAM_QUIET:+--quiet-text}
        else
          claude -p --dangerously-skip-permissions "$prompt"
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
dump_loop_instructions() {
  local func=${1:-${AGENT_FUNC:-}}
  match_loop_text "$func"
}

build_prompt() {
  local func=$1
  local scratch=$2
  local brief_file=$3
  cat <<EOF
Match \`$func\`. The scratch environment is already created at \`$scratch\`.
Do NOT run ./tools/claude or recreate the scratch directory.

Read \`$scratch/BRIEF.md\` (also pasted below), then:

1. cd into \`$scratch\` and make \`base.c\` compile with **minimal** edits (\`./build.sh base.c\`). If this was a give-up retry, \`base.c\` is the archived seed — do not restart from m2c or rewrite from the asm before the first score.
2. $(dump_loop_instructions "$func")
3. If the best score is ≥ 95% and leftover diffs are registers / scheduling / stack (\`branch\`=\`insert\`=\`delete\`=0), run the permuter from the repo root **before** adding register pins:
   \`./permute.sh --run --timeout 360 -j4 $func <asm path from BRIEF> $scratch/base_N.c\`
4. On 100%: replace INCLUDE_ASM in the host C file, fix headers in this overlay's include/ tree, run \`./tools/build-and-verify.sh\`, commit \`matched $func <attempts>\`.
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
  cat <<EOF
decomp-permuter found a **score 0** candidate for \`$func\` after the matching session stalled.

Scratch: \`$scratch\`
Seed C: \`$seed\`
Winning preprocessed function: \`$winner\`
Report: \`$scratch/PERMUTER.txt\`

Port the permuter's transformations into the seed (or a new \`$scratch/base_N.c\`).
Do NOT add \`register … asm("")\` pins. Do NOT re-run \`./tools/claude\` or the permuter.
Confirm \`./build.sh <file>.c\` in the scratch dir reports 100%, then replace INCLUDE_ASM
in the host C file, run \`./tools/build-and-verify.sh\`, and commit \`matched $func permute\`.
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

commit_match_if_needed() {
  # 0 = we verified + committed; 2 = agent already committed (still verify);
  # 1 = no integrated match.
  local func=$1
  local scratch=$2
  local attempts
  attempts=$(count_attempts "$scratch")

  if git log -1 --pretty=%s 2>/dev/null | grep -qE "^matched ${func}( |$)"; then
    echo "Agent already committed a match for $func" | tee -a "$LOG_FILE"
    return 2
  fi

  if include_asm_present "$func"; then
    return 1
  fi

  echo "INCLUDE_ASM for $func is gone; verifying before auto-commit..." | tee -a "$LOG_FILE"
  if ! ./tools/build-and-verify.sh; then
    echo "Verify failed after INCLUDE_ASM removal; discarding uncommitted match" | tee -a "$LOG_FILE"
    return 1
  fi

  git add -A -- src include DECOMPILATION_LEARNINGS.md STRUCT_FIELDS.md NAMING.md 2>/dev/null || true
  if git diff --cached --quiet; then
    echo "No staged match files to commit" | tee -a "$LOG_FILE"
    return 1
  fi
  git commit -m "matched $func $attempts"
  echo "Auto-committed matched $func $attempts" | tee -a "$LOG_FILE"
  return 0
}

difficult_listed() {
  local func=$1
  [[ -f "$DIFFICULT_FUNCTIONS" ]] || return 1
  awk -v f="$func" '$1 == f { found = 1 } END { exit !found }' "$DIFFICULT_FUNCTIONS"
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
  local attempts score
  attempts=$(count_attempts "$scratch")
  score=$(best_score "$func" "$scratch")
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
  if git diff --quiet -- "$DIFFICULT_FUNCTIONS"; then
    return 0
  fi
  if [[ $match_status -eq 0 || $match_status -eq 2 ]]; then
    git checkout -- "$DIFFICULT_FUNCTIONS" 2>/dev/null || true
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
Do NOT edit \`$ROOT\`. Do NOT append tools/difficult_functions (a later port agent does that on trunk).

$(stale_build_warning "$wt")

Read \`$scratch/BRIEF.md\` (also pasted below), then:

1. cd into \`$scratch\` and make \`base.c\` compile with minimal edits (\`./build.sh base.c\`). If this was a give-up retry, \`base.c\` is the archived seed — do not restart from m2c or rewrite from the asm before the first score.
2. $(dump_loop_instructions "$func")
3. If the best score is ≥ 95% and leftover diffs are registers / scheduling / stack (\`branch\`=\`insert\`=\`delete\`=0), run the permuter from the worktree root **before** adding register pins:
   \`./permute.sh --run --timeout 360 -j4 $func <asm path from BRIEF> $scratch/base_N.c\`
4. On 100%: replace INCLUDE_ASM in the worktree host C file, fix headers in this overlay's include/ tree, run \`./tools/build-and-verify.sh\` **in the worktree**, commit \`matched $func <attempts>\` **on this worktree branch only**.
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
Port the matched function onto **current** trunk:

1. Read the worktree host C / headers / learnings (and \`$scratch\` winning \`base_N.c\`) as the source of intent.
2. Read the same paths on trunk. Adapt to structs, INCLUDE_ASM sites, and nearby functions that already landed.
3. Replace INCLUDE_ASM for \`$func\` on trunk. Reconcile types rather than duplicating fields.
4. If the old body no longer matches because layout / context changed, keep adapting until \`./tools/build-and-verify.sh\` prints \`✅ BUILD SUCCEEDED\`.
5. Commit \`matched $func $attempts\` on trunk. Run \`venv/bin/python3 ninja_config.py\` if splat still lists this function under nonmatchings.
6. Optional learnings: add a note to DECOMPILATION_LEARNINGS.md only if it is still true on trunk.

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
  git -C "$wt" diff "$base" -- src include DECOMPILATION_LEARNINGS.md STRUCT_FIELDS.md NAMING.md 2>/dev/null \
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
  ( cd "$repo" && ./tools/build-and-verify.sh ) 2>&1 | tee -a "$LOG_FILE"
  st=${PIPESTATUS[0]}
  return "$st"
}

# Copy worktree src/include onto trunk when those files have not moved since
# the worktree was created. Avoids a port agent (and grok's stale-checksum
# "verify") for the common no-divergence case.
fast_port_from_worktree() {
  local func=$1
  local wt=$2
  local base=$3
  local scratch=$4
  local -a changed=()
  mapfile -t changed < <(git -C "$wt" diff --name-only --diff-filter=ACDMR "$base" -- \
    src include DECOMPILATION_LEARNINGS.md STRUCT_FIELDS.md NAMING.md)
  if [[ ${#changed[@]} -eq 0 ]]; then
    echo "Fast port skipped: worktree has no src/include diff for $func" | tee -a "$LOG_FILE"
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

reset_trunk_to() {
  local rev=$1
  git -C "$ROOT" reset --hard "$rev" >/dev/null 2>&1 || true
  git -C "$ROOT" clean -fd -- src include >/dev/null 2>&1 || true
}

vacuum_orch_loop() {
  echo "Vacuum using CLI: $CLI (orchestrator session $SESSION)" | tee -a "$LOG_FILE"
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

    claim_json=$(orch claim --session "$SESSION" --pid $$ --cli "$CLI")
    claim_code=$?
    echo "$claim_json" | tee -a "$LOG_FILE"
    if [[ $claim_code -eq 3 ]]; then
      echo "Error: All functions are marked as difficult!"
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
    echo -e "\n[$(date '+%H:%M:%S')] [$CLI] [orch] Decompiling $func...\n" | tee -a "$LOG_FILE"
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
        git -C "$wt" clean -fd -- src include >/dev/null 2>&1 || true
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
      ( cd "$wt" && git checkout -- src include DECOMPILATION_LEARNINGS.md STRUCT_FIELDS.md NAMING.md "$DIFFICULT_FUNCTIONS" 2>/dev/null ) || true
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

    local did_fast=0
    if [[ "$status" == "matched" ]]; then
      if fast_port_from_worktree "$func" "$wt" "$ORCH_BASE" "$scratch"; then
        did_fast=1
      else
        reset_trunk_to "$pre_port"
      fi
    fi

    if [[ $did_fast -eq 0 ]]; then
      if [[ "$status" == "matched" ]]; then
        echo "Starting port agent for $func ($status)..." | tee -a "$LOG_FILE"
        AGENT_FUNC="$func" AGENT_MAX_TURNS="${VACUUM_PORT_MAX_TURNS:-80}" run_agent \
          "$(build_port_prompt "$func" "$status" "$wt" "$scratch" "$attempts" "$score" "$hint")" \
          "$ROOT" | tee -a "$LOG_FILE"
        if include_asm_present "$func" "$ROOT"; then
          :
        else
          ( cd "$ROOT" && commit_match_if_needed "$func" "$scratch" ) || true
        fi
      else
        echo "Recording give-up for $func on trunk (no port agent)" | tee -a "$LOG_FILE"
        ( cd "$ROOT" && record_difficult_if_needed "$func" "$scratch"
          commit_difficult_if_needed "$func" 1 ) || true
      fi
    fi

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
    if echo "$output" | grep -qF "$STOP_PHRASE"; then
      echo "$STOP_PHRASE"
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

echo "Vacuum using CLI: $CLI" | tee -a "$LOG_FILE"

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

  mapfile -t NONMATCHING_DIRS < <(python3 "$OVERLAY_PY" list-nonmatchings)
  if [[ ${#NONMATCHING_DIRS[@]} -eq 0 ]]; then
    echo "Error: No nonmatchings directories found under asm/"
    break
  fi

  simplest_func=$(python3 tools/score_functions.py "${NONMATCHING_DIRS[@]}" 2>&1)
  if [[ -z "$simplest_func" ]] || echo "$simplest_func" | grep -qF "Error:"; then
    echo "$simplest_func"
    break
  fi

  echo -e "\n[$(date '+%H:%M:%S')] [$CLI] Decompiling $simplest_func...\n" | tee -a "$LOG_FILE"

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
    git clean -fd -- src include >/dev/null 2>&1 || true
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
  git clean -fd -- src include >/dev/null 2>&1 || true
  cleanup_scratch "$simplest_func" "$scratch"

  # 0 already ran verify before auto-commit; 1/2 still need a tree check.
  if [[ $match_status -ne 0 ]]; then
    ./tools/build-and-verify.sh || break
  fi

  if [[ $STOP_REQUESTED -eq 1 ]]; then
    echo "Stopping gracefully."
    break
  fi

  if echo "$output" | grep -qF "$STOP_PHRASE"; then
    echo "$STOP_PHRASE"
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
