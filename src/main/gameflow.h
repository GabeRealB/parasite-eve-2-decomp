#ifndef MAIN_PRIVATE_GAMEFLOW_H
#define MAIN_PRIVATE_GAMEFLOW_H

#include "types.h"

#include "main/task_types.h"

extern u16 D_8005ED8A;

/// Prepares a new game, saved session or attract demo and queues session startup.
///
/// Resident bank 0, slot 3 interprets spawnArg1 as 0 for a new save (retaining
/// vibration), 2 for an attract demo, or any other value for the existing save.
/// Demo state zero queues its replay file; subsequent callbacks wait for the
/// CD queue before restoring replay state. Live-game paths complete in one
/// callback. Requires initialized resident session/save/input state; demo
/// restoration also requires its replay storage to remain valid.
/// The handoff stops the current task walk before killing the task and
/// discarding task/model lists and heap allocations, then spawns bank 0 slot 9.
void gameFlowLaunchSessionTask(Task* task);

/// Runs one stage of the title's memory-card load flow.
///
/// Resident task bank 0, slot 4 starts with `task->state` zero. The unchecked
/// state domain is 0..4: reset session progress, create the load dialog, wait
/// for its closure, wait twelve callbacks, then return to the title or restart
/// from a successfully loaded save. Requires initialized resident UI and save
/// state. State 2 borrows the UI-owned dialog through `spawnArg2`;
/// closure requests animated teardown without freeing it here. The last stage
/// kills this task and, on successful load, resets task lists and heaps.
void gameFlowLoadDialogTask(Task* task);

/// Starts a session from the live save's location through resident task bank 0, slot 9.
///
/// `task->state` must be 0..2: restore the complete saved location cell and reset
/// disk-swap presentation, wait for the required disc and queue the initial load,
/// then wait for the CD queue to drain before spawning the next loading task.
/// Each callback renews port 0's input block. The final state kills this task;
/// the state table is unchecked and the callback requires live resident state.
void gameFlowStartSessionTask(Task* task);

#endif // MAIN_PRIVATE_GAMEFLOW_H
