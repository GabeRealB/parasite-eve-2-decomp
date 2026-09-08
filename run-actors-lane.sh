#!/bin/bash
# Actors leverage lane, isolated in this worktree.
#
# Deliberately NOT --orchestrator. This checkout is the lane's own trunk, so it
# needs neither function claims nor a merge lock, and orchestrator mode would be
# actively harmful here: vacuum_orch_loop opens with prune_stale_worktrees,
# which reads the claimed list from its own orchestrator state but enumerates
# `git worktree list` - and worktrees are shared repo-wide. A lane with private
# state therefore sees the main checkout's live pe2-wt-<func> directories as
# unclaimed and rm -rf's them. Non-orchestrator mode never calls that function
# and never creates pe2-wt-* at all.
#
# CLAUDE.md's warning is about running non-orchestrator and orchestrator vacuums
# on *the same checkout*; separate checkouts are the sanctioned case.
#
# Commits land on the actors-sweep branch, to be rebased onto main periodically.
cd "$(dirname "$0")"
PROFILE="${1:-opus}"
setsid nohup python3 -c "
import signal, os
signal.signal(signal.SIGINT, signal.SIG_DFL)   # graceful stop after the current function
os.environ['VACUUM_DIFFICULT_FILE'] = os.path.join(os.getcwd(), '.actors-leverage.txt')
os.execv('/bin/bash', ['bash','./tools/vacuum.sh','--profile','$PROFILE',
                       '--difficult','--overlay','USA/actors'])
" >> "$PWD/actors-lane.log" 2>&1 &
disown
echo "actors lane started (profile $PROFILE); log: $PWD/actors-lane.log"
