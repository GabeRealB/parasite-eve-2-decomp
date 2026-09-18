#!/usr/bin/env bash
# Walk the naming worklist, handing one step at a time to an agent.
#
#   ./tools/refactor/name_pass.sh [--times N] [--profile NAME] [--dry-run]
#                                 [--cli claude|grok|codex] [--from ORDER]
#                                 [--step ORDER] [--list-profiles]
#
# Profiles are the same ones the matching vacuum uses, from
# local/vacuum_profiles: a profile names the agent arm, the model, the
# reasoning effort and optionally a wrapper command to launch it through. An
# explicit --cli, or a VACUUM_MODEL / VACUUM_MATCH_EFFORT already exported,
# still wins over the profile.
#
# A step is one line of local/worklist.tsv, or several lines sharing an order
# when a cycle means the items have to be understood together. For each step
# this builds a brief, runs the agent against the tree, verifies the build, and
# commits. Nothing is isolated: unlike the matching vacuum there is no worktree
# and no scratch environment, because a rename needs the whole tree in front of
# it and produces no candidate to throw away.
#
# The safety property is the build. An agent that leaves the tree broken, or
# that changes a checksum, has its work reverted and the pass stops rather than
# carrying a bad rename into the next step.
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
TIMES=1
DRY=0
FROM=0
ONLY=""
LIST_PROFILES=0
KEEP_GOING=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --times) TIMES="$2"; shift 2 ;;
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
    --stop-on-fail) KEEP_GOING=0; shift ;;
    -h|--help) sed -n '2,28p' "$0"; exit 0 ;;
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
  [[ -f "$DONE_LEDGER" ]] && cut -f2 "$DONE_LEDGER" | tr ' ' '\n' | grep -qx -- "$1"
}

# order, items, commit (or -), outcome.
record() {
  printf '%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" >> "$DONE_LEDGER"
}

next_step() {
  local order name
  while IFS=$'\t' read -r order _ name _ _ _ _ _; do
    [[ "$order" == "order" ]] && continue
    (( order < FROM )) && continue
    [[ -n "$ONLY" && "$order" != "$ONLY" ]] && continue
    ledgered "$name" && continue
    if outstanding "$name"; then echo "$order"; return 0; fi
  done < <(rows)
  return 1
}

build_brief() {
  local order="$1" names=() line
  while IFS=$'\t' read -r o _ name kind vis state file refs; do
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
cp "$WORKLIST" "$SNAPSHOT"
trap 'rm -f "$SNAPSHOT"' EXIT
WORKLIST="$SNAPSHOT"

LOG="$(vacuum_log_dir)/name_pass-$$.log"
echo "logging to $LOG"

done_count=0
fail_count=0
barrier=
i=0
while (( TIMES == 0 || i < TIMES )); do
  i=$((i + 1))
  order="$(next_step)" || { echo "worklist exhausted"; break; }
  mapfile -t items < <(awk -F'\t' -v o="$order" '$1==o{print $3}' "$WORKLIST")
  echo "=== step $order: ${items[*]}  ($(date +%H:%M:%S))"

  # A placeholder whose body is still assembly is a barrier, not a step. There
  # is nothing to read, so any name given to it would be a guess - and because
  # the worklist is a dependency order, everything after it is something that
  # uses it, so the guess would be the evidence the next steps reason from.
  # Stop and let the matching vacuum turn it into C first.
  mapfile -t states < <(awk -F'\t' -v o="$order" '$1==o{print $6}' "$WORKLIST")
  for idx in "${!states[@]}"; do
    if [[ "${states[$idx]}" == "generated" ]]; then
      cat >&2 <<BARRIER
=== stopping at step $order: ${items[$idx]} is still assembly

It has no C body, so this pass cannot establish what it is, and every later
step depends on it. Decompile it first, for example:

  ./tools/vacuum.sh --overlay <its overlay>

then rebuild the graph and resume:

  venv/bin/python3 tools/refactor/dep_graph.py --build
  venv/bin/python3 tools/refactor/dep_graph.py worklist
BARRIER
      barrier=1
      break 2
    fi
  done

  brief="$(build_brief "$order")"
  if (( DRY )); then printf '%s\n' "$brief"; exit 0; fi

  if [[ -n "$(git status --porcelain)" ]]; then
    echo "tree is dirty; commit or stash before running the pass" >&2
    exit 1
  fi

  case "$CLI" in
    claude)
      # Plain `claude -p` prints only the final result, so a step looks frozen
      # for as long as it runs - which at high effort on a widely-used item is
      # many minutes of silence. Stream the events and format them, the way the
      # matching vacuum does. VACUUM_STREAM=0 restores the quiet form.
      if [[ "${VACUUM_STREAM:-1}" != "0" ]]; then
        "${LAUNCH_CMD[@]}" -p ${MODEL:+--model "$MODEL"} ${EFFORT:+--effort "$EFFORT"} \
          --verbose --output-format stream-json \
          --dangerously-skip-permissions "$brief" \
          | python3 tools/stream_format.py ${VACUUM_STREAM_QUIET:+--quiet-text} \
          | tee -a "$LOG"
      else
        "${LAUNCH_CMD[@]}" -p ${MODEL:+--model "$MODEL"} ${EFFORT:+--effort "$EFFORT"} \
          --dangerously-skip-permissions "$brief" | tee -a "$LOG"
      fi
      ;;
    grok)
      # grok emits the same NDJSON wire format on request, so the one formatter
      # renders both arms and a grok step reports its cost and turn count the
      # way a claude step does.
      if [[ "${VACUUM_STREAM:-1}" != "0" ]]; then
        "${LAUNCH_CMD[@]}" --always-approve ${EFFORT:+--effort "$EFFORT"} \
          --cwd "$ROOT" ${RULES:+--rules "$RULES"} \
          --output-format streaming-messages-json \
          --include-partial-messages -p "$brief" \
          | python3 tools/stream_format.py ${VACUUM_STREAM_QUIET:+--quiet-text} \
          | tee -a "$LOG"
      else
        "${LAUNCH_CMD[@]}" --always-approve ${EFFORT:+--effort "$EFFORT"} \
          --cwd "$ROOT" ${RULES:+--rules "$RULES"} -p "$brief" | tee -a "$LOG"
      fi
      ;;
    codex)
      "${LAUNCH_CMD[@]}" exec --dangerously-bypass-approvals-and-sandbox \
        ${MODEL:+--model "$MODEL"} --cd "$ROOT" "$brief" | tee -a "$LOG"
      ;;
    *) echo "unknown api: $CLI" >&2; exit 2 ;;
  esac

  if ! ./tools/build-and-verify.sh >/tmp/name_pass_build.log 2>&1; then
    echo "step $order FAILED to build; reverting" >&2
    tail -20 /tmp/name_pass_build.log >&2
    git checkout -- . && git clean -fd src include configs >/dev/null
    record "$order" "${items[*]}" - failed
    fail_count=$((fail_count + 1))
    (( KEEP_GOING )) || exit 1
    continue
  fi

  if [[ -z "$(git status --porcelain)" ]]; then
    echo "step $order left the tree unchanged" >&2
    record "$order" "${items[*]}" - unchanged
    (( KEEP_GOING )) || exit 1
    continue
  fi

  git add -A
  git commit -q -m "naming: ${items[*]}"
  record "$order" "${items[*]}" "$(git rev-parse --short HEAD)" ok
  done_count=$((done_count + 1))
  echo "=== step $order committed"
done

echo "completed $done_count step(s), $fail_count failed${barrier:+, stopped at a barrier}"
