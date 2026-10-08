#ifndef INCLUDE_ROOMS_MIST_R21_H
#define INCLUDE_ROOMS_MIST_R21_H

#include "main/task_types.h"

/// Runs the M.I.S.T. room's message receiver and plaza-reload shortcut.
///
/// Spawned by the Acropolis map's room descriptor for area 21. Requires live
/// save/session state and state 0..2: register the room receiver and auxiliary
/// task, poll L3-held/Cross-pressed on port 0, or kill the task. The room
/// overlay and message table remain loaded until reload tears down the task list.
void mistR21RoomTask(Task* task);

#endif // INCLUDE_ROOMS_MIST_R21_H
