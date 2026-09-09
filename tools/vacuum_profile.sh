#!/usr/bin/env bash
#
# Named agent configurations, shared by tools/vacuum.sh and
# tools/vacuum_overlay.sh. Source it; do not execute it.
#
#   . "$ROOT/tools/vacuum_profile.sh"      # after ROOT and CLI are set
#   apply_profile "$PROFILE"               # once flags have been parsed
#
# A profile only fills in what the caller left unspecified: an explicit
# --cli/--claude/--grok/--codex flag, or a VACUUM_MODEL / VACUUM_LAND_MODEL /
# VACUUM_MATCH_EFFORT / VACUUM_LAND_EFFORT already exported, always wins. That
# is why the caller must set CLI_EXPLICIT=1 when a CLI flag was given.
#
# The table lives outside the repo (local/ is gitignored) because which models
# resolve depends on the account each CLI is logged in as.

PROFILE="${PROFILE-${VACUUM_PROFILE:-}}"
PROFILES_FILE="${VACUUM_PROFILES_FILE:-$ROOT/local/vacuum_profiles}"
CLI_EXPLICIT="${CLI_EXPLICIT:-0}"

# Which knobs the caller set before we touch them. Recorded at source time,
# while the difference between "unset" and "set to empty" is still observable.
_SET_MODEL=${VACUUM_MODEL+1}
_SET_LAND_MODEL=${VACUUM_LAND_MODEL+1}
_SET_MATCH_EFFORT=${VACUUM_MATCH_EFFORT+1}
_SET_LAND_EFFORT=${VACUUM_LAND_EFFORT+1}

# One row of the profiles table, comments and blank lines skipped.
profile_row() {
  [[ -f "$PROFILES_FILE" ]] || return 1
  awk -v n="$1" '$0 !~ /^[[:space:]]*#/ && NF && $1 == n { print; found = 1; exit }
                 END { exit !found }' "$PROFILES_FILE"
}

list_profiles() {
  if [[ ! -f "$PROFILES_FILE" ]]; then
    echo "No profiles file at $PROFILES_FILE"
    return
  fi
  printf '%-18s %-7s %-18s %-7s %-14s %s\n' NAME CLI MODEL EFFORT LAND_MODEL LAND_EFFORT
  awk '$0 !~ /^[[:space:]]*#/ && NF { printf "%-18s %-7s %-18s %-7s %-14s %s\n", $1, $2, $3, $4, ($5 == "" ? "-" : $5), ($6 == "" ? "-" : $6) }' \
    "$PROFILES_FILE"
}

# Fill in only what the caller left unspecified: flags and environment win.
apply_profile() {
  local name=$1 row _n cli model effort land land_effort
  if ! row=$(profile_row "$name"); then
    echo "Error: no profile '$name' in $PROFILES_FILE"
    echo "Known profiles:"
    list_profiles
    exit 1
  fi
  read -r _n cli model effort land land_effort <<<"$row"
  if [[ "$cli" != "-" && ${CLI_EXPLICIT:-0} -eq 0 ]]; then
    CLI="$cli"
  fi
  if [[ "$model" != "-" && -z "$_SET_MODEL" ]]; then
    export VACUUM_MODEL="$model"
  fi
  if [[ -n "${land:-}" && "$land" != "-" && -z "$_SET_LAND_MODEL" ]]; then
    export VACUUM_LAND_MODEL="$land"
  fi
  # Effort is a role, not a CLI setting: VACUUM_MATCH_EFFORT covers the search
  # and VACUUM_LAND_EFFORT the mechanical port, for every CLI alike - the same
  # split VACUUM_MODEL / VACUUM_LAND_MODEL already makes.
  if [[ "$effort" != "-" && -z "$_SET_MATCH_EFFORT" ]]; then
    export VACUUM_MATCH_EFFORT="$effort"
  fi
  if [[ -n "${land_effort:-}" && "$land_effort" != "-" && -z "$_SET_LAND_EFFORT" ]]; then
    export VACUUM_LAND_EFFORT="$land_effort"
  fi
  PROFILE="$name"
  # Exported so a nested vacuum.sh - the one vacuum_overlay.sh runs inside a
  # worktree - reports the same profile instead of re-resolving a default.
  # The table must travel with the name: a worktree's ROOT is the worktree, and
  # local/ is gitignored, so <worktree>/local/vacuum_profiles does not exist and
  # the nested run would abort with "no profile '<name>'".
  export VACUUM_PROFILE="$name"
  export VACUUM_PROFILES_FILE="$PROFILES_FILE"
}

# Every lane writes here. local/ is gitignored, so logs never reach a commit,
# and keeping them out of tools/ stops a stale log from being mistaken for a
# tracked file.
vacuum_log_dir() {
  local d="${VACUUM_LOG_DIR:-$ROOT/local/logs}"
  mkdir -p "$d" 2>/dev/null || true
  echo "$d"
}
