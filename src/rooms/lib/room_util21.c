#include "common.h"

#include "gameplay/3688.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// Work block parked at `Task::idMap` by the factory rooms that link this
/// body. `promptKind` is the display mode forwarded to `func_800D4E78`.
typedef struct RoomUtil21Work {
    /* 0x00 */ byte pad_0[0xE];
    /* 0x0E */ s8   promptKind;
} RoomUtil21Work;

/// Spawns the action prompt for the script's current step: clears the prompt's
/// highlight state, then re-spawns it at the coordinates the gameplay side left
/// in `D_80114D28` with the display mode this state picked.
///
/// Shared body, linked into every room overlay that uses it.
void Room_Util21(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    RoomUtil21Work*   work   = (RoomUtil21Work*)task->idMap;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}
