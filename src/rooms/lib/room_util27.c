#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Room script callback: poll the text block parked at `Task::idMap` until
/// `TextBlockDesc::field_2` is set, copy that result through `Task::spawnArg2`,
/// and step the caller on one state.
void Room_Util27(Task* task)
{
    s16 result;

    result = ((RoomTextBlock*)task->idMap)->desc.field_2;
    if (result != 0) {
        *(s32*)task->spawnArg2 = result;
        task->state            = task->state + 1;
    }
}
