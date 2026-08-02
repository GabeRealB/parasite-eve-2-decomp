#!/usr/bin/env bash
#
# Vacuum: repeatedly pick the easiest unmatched function and hand it to an
# agent CLI (Claude Code or Grok) for decompilation.
#
# Usage:
#   ./tools/vacuum.sh [--cli claude|grok] [--claude] [--grok] [--times N]
#
# Environment:
#   VACUUM_CLI   Default CLI when no --cli/--claude/--grok flag is given
#                (claude | grok). Defaults to claude.

set -uo pipefail

STOP_PHRASE="Error: All functions are marked as difficult!"
MAX_TIMES=""
STOP_REQUESTED=0
DIFFICULT_FUNCTIONS="tools/difficult_functions"
CLI="${VACUUM_CLI:-claude}"
LOG_FILE="tools/vacuum.log"

usage() {
  cat <<EOF
Usage: $0 [options]

Options:
  --cli NAME   Agent CLI to run: claude or grok (default: ${VACUUM_CLI:-claude})
  --claude     Shorthand for --cli claude
  --grok       Shorthand for --cli grok
  --times N    Stop after N loop iterations
  -h, --help   Show this help

Environment:
  VACUUM_CLI   Default CLI when no flag is given (claude | grok)
EOF
}

# Trap Ctrl+C (SIGINT) for graceful shutdown after the current function.
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

if ! command -v "$CLI" &>/dev/null; then
  echo "Error: '$CLI' not found in PATH"
  exit 1
fi

# Run one agent turn against the given prompt. Both CLIs support -p for
# non-interactive single-prompt mode; Grok needs --always-approve so tool
# calls (edits, builds) are not blocked in unattended vacuum runs.
run_agent() {
  local prompt=$1
  case "$CLI" in
    claude)
      claude -p "$prompt"
      ;;
    grok)
      grok --always-approve -p "$prompt"
      ;;
  esac
}

echo "Vacuum using CLI: $CLI" | tee -a "$LOG_FILE"

count=0
consecutive_failures=0

while true; do
  if [[ $STOP_REQUESTED -eq 1 ]]; then
    echo "Stopping gracefully."
    break
  fi

  if [[ -n "$MAX_TIMES" ]] && [[ $count -ge $MAX_TIMES ]]; then
    echo "Reached maximum iterations: $MAX_TIMES"
    break
  fi

  simplest_func=$(python3 tools/score_functions.py asm/USA/main/nonmatchings/ 2>&1)
  if [[ -z "$simplest_func" ]] || echo "$simplest_func" | grep -qF "Error:"; then
    echo "$simplest_func"
    break
  fi

  echo -e "\n[$(date '+%H:%M:%S')] [$CLI] Decompiling $simplest_func...\n" | tee -a "$LOG_FILE"

  output=$(run_agent "decompile the function $simplest_func" 2>&1 | tee -a "$LOG_FILE")
  exit_code=${PIPESTATUS[0]}
  echo "$output"

  # Keep difficult_functions updates; discard any other uncommitted agent mess
  # so a bad decomp cannot pollute the next iteration's commit base.
  if ! git diff --quiet -- "$DIFFICULT_FUNCTIONS" && [ -f "$DIFFICULT_FUNCTIONS" ]; then
    echo "Detected uncommitted difficult_functions change" | tee -a "$LOG_FILE"
    git add "$DIFFICULT_FUNCTIONS"
    git commit -m "Update $DIFFICULT_FUNCTIONS"
  fi

  git reset --hard HEAD

  # Bail if the tree no longer builds/matches after the agent touched it.
  ./tools/build-and-verify.sh || break

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
