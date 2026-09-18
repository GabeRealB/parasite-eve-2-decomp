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
CLI="${VACUUM_CLI:-claude}"
CLI_EXPLICIT=0
PROFILE="${PROFILE-${VACUUM_PROFILE:-}}"
TIMES=1
DRY=0
FROM=0
ONLY=""
LIST_PROFILES=0

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

next_step() {
  local order name
  while IFS=$'\t' read -r order _ name _ _ _ _ _; do
    [[ "$order" == "order" ]] && continue
    (( order < FROM )) && continue
    [[ -n "$ONLY" && "$order" != "$ONLY" ]] && continue
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
# Naming pass, step $order

Process ${#names[@]} item(s) together: ${names[*]}
$( ((${#names[@]} > 1)) && echo "
These items form a cycle in the dependency graph: each uses the others, so they
have to be understood as one unit rather than in sequence." )
$line

## What to do

Everything below is specified in NAMING.md; read it if anything here is unclear.

1. Work out what each item is and why it exists, from the code that defines and
   uses it. **Trust nothing that is already written about it** - not the
   comments, not its current name, not its declared type. Each is an earlier
   reader's hypothesis.

   The name is the most dangerous of the three, because it silently supplies
   the vocabulary for your description: a field called \`idMap\` invites you to
   describe an id map even when every write stores something else. Read the
   uses first, decide what the thing is, and only then ask whether the existing
   name survives that reading. If it does not, the new name comes from the
   evidence.

   The declared type is the second. **If the uses have to cast, the type is
   wrong.** The reference listing above marks each use that sits in a cast, so
   count them: a field whose reads and writes are nearly all casts is not the
   type it claims to be, and the honest declaration is the one that makes the
   casts unnecessary - often \`void*\` for a pointer whose target is the
   caller's business. Change it, and rebuild to confirm.
2. If the item's state above is \`current\` its name already follows the
   convention: **do not rename it**, only document it. Otherwise rename to the
   convention with:
     venv/bin/python3 tools/refactor/rename_item.py <file>/<oldName> <newName> --sidecars
   lowerCamelCase, no separators, opening with the owning module or package. A
   leading g marks a global, a leading _ marks something private to its
   translation unit, PascalCase is reserved for types.
3. Document it. What it is and why, never how you worked it out.

   **As general as the subject allows, and only as specific as it needs.**
   Naming the mechanism is the usual mistake: "frames left before the body is
   released" is the field; "counted down by \`Task_CountdownCallback\`" adds a
   function name the reader can find and that will not survive a
   reorganisation. A specific earns its place when the meaning is otherwise
   unavailable - a small set of values has to be enumerated, because nothing
   else says what a 2 means. The test: would the sentence still be true and
   still be useful if a neighbouring function were renamed?

   **Name the parameters too**, in the prototype and the definition alike. A
   signature reading \`(s32 arg0, s32 arg1)\` tells a caller nothing, and
   naming them in only one of the two places leaves one function with two
   signatures. \`rename_item.py <file>/<function>::<param> <newName>\` matches
   by position and rewrites both.

   **Watch for nested types.** If this item describes bytes belonging to a
   different type at an offset into it - the giveaway is a name like
   \`XFromN\`, or fields whose comments map onto another type's - then a run
   of that other struct is really one thing, and it belongs inside it as a
   nested type. A plain nested struct where the run is only used as a group,
   anonymous if nothing takes its address; a union of an anonymous struct with
   the named aggregate where the bytes are also read field by field. Either way
   callers stop casting, and since the address does not change the checksum
   confirms it.

   **A field you can describe is a field you can name.** Leaving a field called
   \`field_14\` while writing a comment stating its role contradicts itself:
   the offset name means the role is unknown. Rename it from the same reading -
   member renames go through rename_item.py as \`<header>/<Type>::<field>\` -
   and keep \`field_XX\` only where the role really is unproven.

   Mentions of
   the old name in comments elsewhere are rewritten by rename_item.py; check
   that what they now say is still true, since a renamed mention can leave a
   sentence describing the old idea. Functions and
   types take a /// block; struct fields take aligned trailing // comments and
   no offset annotations.
4. Visibility: a private item's declaration belongs in the .c that uses it, not
   in a header, and takes the _ prefix. Add \`static\` only if the build still
   matches afterwards.
5. If the role genuinely cannot be established from the code, leave the name
   alone and say so - an invented name is worse than a generated one.

## Finishing

Run \`./tools/build-and-verify.sh\`. It must end with BUILD SUCCEEDED and the
matched-function count must not drop. Do not commit; the driver commits.
EOF
}

LOG="$(vacuum_log_dir)/name_pass-$$.log"
echo "logging to $LOG"

done_count=0
for ((i = 0; i < TIMES; i++)); do
  order="$(next_step)" || { echo "worklist exhausted"; break; }
  mapfile -t items < <(awk -F'\t' -v o="$order" '$1==o{print $3}' "$WORKLIST")
  echo "=== step $order: ${items[*]}  ($(date +%H:%M:%S))"

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
          --cwd "$ROOT" --output-format streaming-messages-json \
          --include-partial-messages -p "$brief" \
          | python3 tools/stream_format.py ${VACUUM_STREAM_QUIET:+--quiet-text} \
          | tee -a "$LOG"
      else
        "${LAUNCH_CMD[@]}" --always-approve ${EFFORT:+--effort "$EFFORT"} \
          --cwd "$ROOT" -p "$brief" | tee -a "$LOG"
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
    exit 1
  fi

  if [[ -z "$(git status --porcelain)" ]]; then
    echo "step $order left the tree unchanged; stopping so it can be looked at" >&2
    exit 1
  fi

  git add -A
  git commit -q -m "naming: ${items[*]}"
  done_count=$((done_count + 1))
  echo "=== step $order committed"
done

echo "completed $done_count step(s)"
