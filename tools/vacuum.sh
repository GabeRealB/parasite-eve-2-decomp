#!/usr/bin/env bash
#
# Vacuum: pick the easiest unmatched function across every overlay, bootstrap a
# scratch env, pack a brief, and hand it to an agent CLI.
#
# Usage:
#   ./tools/vacuum.sh [--cli claude|grok] [--claude] [--grok] [--times N]
#                     [--dry-run] [--keep-scratch] [--no-permute]
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
LOG_FILE="tools/vacuum.log"
OVERLAY_PY="tools/decomp_overlay.py"

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
  -h, --help        Show this help

Environment:
  VACUUM_CLI               Default CLI when no flag is given (claude | grok)
  VACUUM_PERMUTE_TIMEOUT   Permuter cap in seconds (default 360)
  VACUUM_PERMUTE_JOBS      Permuter threads (default: min(nproc, 8))
EOF
}

trap 'echo ""; echo "Interrupt received, will stop after current function..."; STOP_REQUESTED=1' INT

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

include_asm_present() {
  local func=$1
  grep -rE "INCLUDE_ASM[[:space:]]*\\([[:space:]]*\"[^\"]+\"[[:space:]]*,[[:space:]]*${func}[[:space:]]*\\)" \
    src --include='*.c' -l >/dev/null 2>&1
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

run_agent() {
  local prompt=$1
  local extra=()
  if [[ -n "${AGENT_MAX_TURNS:-}" ]]; then
    extra+=(--max-turns "$AGENT_MAX_TURNS")
  fi
  case "$CLI" in
    claude)
      claude -p "$prompt"
      ;;
    grok)
      grok --always-approve --effort high "${extra[@]}" -p "$prompt"
      ;;
  esac
}

build_prompt() {
  local func=$1
  local scratch=$2
  local brief_file=$3
  cat <<EOF
Match \`$func\`. The scratch environment is already created at \`$scratch\`.
Do NOT run ./tools/claude or recreate the scratch directory.

Read \`$scratch/BRIEF.md\` (also pasted below), then:

1. cd into \`$scratch\` and make \`base.c\` compile with minimal edits (\`./build.sh base.c\`).
2. Iterate \`base_N.c\` until 100%.
3. If the best score is ≥ 95% and leftover diffs are registers / scheduling / stack, run the permuter from the repo root **before** adding register pins:
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
  if [[ $PERMUTER -eq 0 ]]; then
    return 1
  fi
  if [[ ! -d "$scratch" ]]; then
    echo "Scratch gone; skipping permuter post-step" | tee -a "$LOG_FILE"
    return 1
  fi
  if ! include_asm_present "$func"; then
    return 1
  fi

  echo "Running permuter post-step for $func..." | tee -a "$LOG_FILE"
  local out st
  out=$(python3 tools/vacuum_permute.py --func "$func" --scratch "$scratch" \
    --timeout "${VACUUM_PERMUTE_TIMEOUT:-360}" \
    --jobs "${VACUUM_PERMUTE_JOBS:-0}" 2>&1 | tee -a "$LOG_FILE")
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
  AGENT_MAX_TURNS=40 run_agent "$(build_permute_prompt "$func" "$scratch" "$seed" "$winner")" | tee -a "$LOG_FILE"
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

commit_difficult_if_needed() {
  local func=$1
  if git diff --quiet -- "$DIFFICULT_FUNCTIONS"; then
    return 0
  fi
  if ! include_asm_present "$func"; then
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
  if [[ $KEEP_SCRATCH -eq 1 ]]; then
    echo "Keeping scratch at $scratch"
    return
  fi
  rm -rf "$scratch"
  rm -rf "nonmatchings/${func}" "nonmatchings/${func}-"*
  rmdir nonmatchings 2>/dev/null || true
  rm -rf "permuter/${func}"
}

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

  output=$(run_agent "$prompt" 2>&1 | tee -a "$LOG_FILE")
  exit_code=${PIPESTATUS[0]}
  echo "$output"

  try_permuter_poststep "$simplest_func" "$scratch" || true

  match_status=1
  commit_match_if_needed "$simplest_func" "$scratch"
  match_status=$?
  commit_difficult_if_needed "$simplest_func" "$scratch"

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
