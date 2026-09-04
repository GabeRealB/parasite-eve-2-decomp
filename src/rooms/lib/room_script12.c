#include "common.h"

#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s8 D_8007216C;

/// Work block parked at `Task::idMap` by the factory rooms that link this
/// body. `field_A` is the one-shot trigger `Room_Util34` raises; this script
/// consumes it, parks `field_8` at 0xA, and steps the caller to state 2.
typedef struct RoomScript12Work {
    /* 0x0 */ byte pad_0[8];
    /* 0x8 */ s16  field_8;
    /* 0xA */ s16  field_A;
} RoomScript12Work;

void Room_Script12(Task* task)
{
    RoomActionPrompt* prompt;
    RoomScript12Work* work;

    prompt           = &D_80114D28;
    work             = (RoomScript12Work*)task->idMap;
    prompt->targetId = 0;
    prompt->mode     = 0;
    if (work->field_A != 0) {
        if (GameFlag_GetNibble(0x48) == 0) {
            D_8007216C = 0xC;
        } else {
            D_8007216C = 5;
        }
        work->field_8 = 0xA;
        work->field_A = 0;
        task->state   = 2;
    }
}
