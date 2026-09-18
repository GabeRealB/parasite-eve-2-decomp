#!/usr/bin/env bash
# Walk the naming worklist, handing one step at a time to an agent.
#
#   ./tools/refactor/name_pass.sh [--times N] [--cli claude|grok] [--dry-run]
#                                 [--from ORDER] [--step ORDER]
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
CLI="${NAME_PASS_CLI:-claude}"
TIMES=1
DRY=0
FROM=0
ONLY=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --times) TIMES="$2"; shift 2 ;;
    --cli)   CLI="$2"; shift 2 ;;
    --from)  FROM="$2"; shift 2 ;;
    --step)  ONLY="$2"; shift 2 ;;
    --dry-run) DRY=1; shift ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

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
   uses it. **Do not trust the comments already on it** - they may be guesses,
   or about a neighbouring symbol. Derive the answer and ignore the prose.
2. If the item's state above is \`current\` its name already follows the
   convention: **do not rename it**, only document it. Otherwise rename to the
   convention with:
     venv/bin/python3 tools/refactor/rename_item.py <file>/<oldName> <newName> --sidecars
   lowerCamelCase, no separators, opening with the owning module or package. A
   leading g marks a global, a leading _ marks something private to its
   translation unit, PascalCase is reserved for types.
3. Document it. What it is and why, never how you worked it out. Functions and
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

done_count=0
for ((i = 0; i < TIMES; i++)); do
  order="$(next_step)" || { echo "worklist exhausted"; break; }
  mapfile -t items < <(awk -F'\t' -v o="$order" '$1==o{print $3}' "$WORKLIST")
  echo "=== step $order: ${items[*]}"

  brief="$(build_brief "$order")"
  if (( DRY )); then printf '%s\n' "$brief"; exit 0; fi

  if [[ -n "$(git status --porcelain)" ]]; then
    echo "tree is dirty; commit or stash before running the pass" >&2
    exit 1
  fi

  case "$CLI" in
    claude) claude -p --dangerously-skip-permissions "$brief" ;;
    grok)   grok --always-approve -p "$brief" ;;
    *) echo "unknown cli: $CLI" >&2; exit 2 ;;
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
