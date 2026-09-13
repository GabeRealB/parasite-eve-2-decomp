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
  printf '%-18s %-7s %-26s %-7s %-14s %s\n' NAME API MODEL EFFORT LAND CLI
  # awk would split the quoted launch column on its spaces; use the same
  # shell-quoting parse the loader does.
  python3 - "$PROFILES_FILE" <<'PY'
import shlex, sys
for line in open(sys.argv[1]):
    if not line.strip() or line.lstrip().startswith("#"):
        continue
    f = (shlex.split(line) + ["-"] * 6)[:6]
    print("%-18s %-7s %-26s %-7s %-14s %s" % tuple(x or "-" for x in f))
PY
}

# One row, split with shell quoting rules so a column may contain spaces.
# shlex rather than `eval set --`: the launch column is a command, and a config
# file should not be able to run `$(...)` merely by being read.
profile_fields() {
  python3 - "$1" <<'PY'
import shlex, sys
sys.stdout.write("\0".join(shlex.split(sys.argv[1])))
PY
}

# Fill in only what the caller left unspecified: flags and environment win.
#
# Columns: name api model effort land cli
#
#   api   which agent arm runs - claude, grok or codex. It selects the flag
#         vocabulary, the output formatter and the prompt conventions.
#   cli   how that arm is launched. "-" runs the api's own binary; anything
#         else is a command that wraps it, with {model} substituted. This is
#         what lets Claude Code be driven by a model hosted elsewhere without
#         the vacuum knowing anything about the host.
#   land  another profile's name, used for the mechanical port-and-land step,
#         or "-" to land with this profile. Only that profile's match columns
#         are read - its own land column is ignored - so self-references and
#         mutual references are legal and mean what they look like.
apply_profile() {
  local name=$1 row api model effort land cli
  local -a f
  if ! row=$(profile_row "$name"); then
    echo "Error: no profile '$name' in $PROFILES_FILE"
    echo "Known profiles:"
    list_profiles
    exit 1
  fi
  mapfile -d '' -t f < <(profile_fields "$row")
  api="${f[1]:--}"; model="${f[2]:--}"; effort="${f[3]:--}"
  land="${f[4]:--}"; cli="${f[5]:--}"

  if [[ "$api" != "-" && ${CLI_EXPLICIT:-0} -eq 0 ]]; then
    CLI="$api"
  fi
  if [[ "$model" != "-" && -z "$_SET_MODEL" ]]; then
    export VACUUM_MODEL="$model"
  fi
  # Effort is a role, not a CLI setting: VACUUM_MATCH_EFFORT covers the search
  # and VACUUM_LAND_EFFORT the mechanical port, for every CLI alike.
  if [[ "$effort" != "-" && -z "$_SET_MATCH_EFFORT" ]]; then
    export VACUUM_MATCH_EFFORT="$effort"
  fi
  if [[ "$cli" != "-" ]]; then
    export VACUUM_LAUNCH="${cli//\{model\}/${model}}"
  fi

  # The land profile supplies the whole triple, not just a model, so a lane can
  # match on one api and land on another - which a land_model column could not
  # express, because the landing agent is written against one arm's flags.
  if [[ "$land" != "-" ]]; then
    local lrow; local -a lf
    if ! lrow=$(profile_row "$land"); then
      echo "Error: profile '$name' names an unknown land profile '$land'"
      exit 1
    fi
    mapfile -d '' -t lf < <(profile_fields "$lrow")
    # Its own land column is ignored, and cycles are harmless: resolution is
    # always exactly two steps - match, then land - so `A land=A` simply means
    # "land with A's settings", and a pair pointing at each other is two valid
    # profiles, not a loop. Nothing here recurses, so nothing can run away.
    [[ -z "$_SET_LAND_MODEL"  && "${lf[2]:--}" != "-" ]] && export VACUUM_LAND_MODEL="${lf[2]}"
    [[ -z "$_SET_LAND_EFFORT" && "${lf[3]:--}" != "-" ]] && export VACUUM_LAND_EFFORT="${lf[3]}"
    [[ "${lf[1]:--}" != "-" ]] && export VACUUM_LAND_API="${lf[1]}"
    if [[ "${lf[5]:--}" != "-" ]]; then
      export VACUUM_LAND_LAUNCH="${lf[5]//\{model\}/${lf[2]}}"
    fi
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
