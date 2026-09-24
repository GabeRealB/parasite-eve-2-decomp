#include "common.h"

#include "gameplay/3688.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The room script task's work block as this state reads it: `promptKind` is
/// the display mode forwarded to `func_800D4E78`, read signed.
typedef struct RoomUtil21Work {
    /* 0x00 */ byte pad_0[0xE];
    /* 0x0E */ s8   promptKind;
} RoomUtil21Work;

/// Spawns the action prompt for the script's current step: clears the prompt's
/// highlight state, then re-spawns it at the coordinates the gameplay side left
/// in `D_80114D28` with the display mode this state picked.
void func_dryfield_factory_8018196C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    RoomUtil21Work*   work   = (RoomUtil21Work*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}
