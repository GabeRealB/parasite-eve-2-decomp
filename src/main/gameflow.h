#ifndef MAIN_PRIVATE_GAMEFLOW_H
#define MAIN_PRIVATE_GAMEFLOW_H

#include "types.h"

#include "main/task_types.h"

extern u16 D_8005ED8A;

void GameFlow_StateByField34(Task* task);

void GameFlow_DispatchTable5(Task* task);

/// Starts a session from the live save's location through resident task bank 0, slot 9.
///
/// `task->state` must be 0..2: restore the complete saved location cell and reset
/// disk-swap presentation, wait for the required disc and queue the initial load,
/// then wait for the CD queue to drain before spawning the next loading task.
/// Each callback renews port 0's input block. The final state kills this task;
/// the state table is unchecked and the callback requires live resident state.
void gameFlowStartSessionTask(Task* task);

#endif // MAIN_PRIVATE_GAMEFLOW_H
