#ifndef GAMEPLAY_OBJECT_TASK_H
#define GAMEPLAY_OBJECT_TASK_H

#include "types.h"

#include "main/task_types.h"

extern u8 D_80115598;

/// Selects the current room's task, or remains its default message receiver.
///
/// Bank 9 task 0x11 dispatches state 0 for room setup, 1 for idle and 2 for
/// task teardown. Requires a live bodyless `task` with `state` in 0..2.
/// Setup requires a live session with
/// stage 1..5 and that stage's map task-descriptor table loaded. It spawns the
/// matching room/area task when present; otherwise registers this task in
/// `GAME_TASK_SLOT_ROOM` to permit unchanged transitions and refuse key items.
/// Neither spawn argument is consumed. Keep gameplay and the selected map's
/// callbacks loaded while dispatch can use them; idle preserves the task.
/// The fallback slot borrows this task; stop sending messages before teardown,
/// since task exit does not clear the registration.
void objectTaskRoomTask(Task* task);

#endif // GAMEPLAY_OBJECT_TASK_H
