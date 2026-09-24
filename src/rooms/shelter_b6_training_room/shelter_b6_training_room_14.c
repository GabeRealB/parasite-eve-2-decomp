#include "common.h"

#include "main/task.h"

#include "rooms/shelter_b6_training_room.h"

/// Drives the room's tracked task: an argument in 0..1 becomes its
/// `spawnArg1`; anything else kills the task and clears the pointer. Does
/// nothing when no task is tracked.
void func_shelter_b6_training_room_8017D974(s32 arg0)
{
    Task* t = D_shelter_b6_training_room_80185C5C;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    taskKill(D_shelter_b6_training_room_80185C5C);
    D_shelter_b6_training_room_80185C5C = NULL;
}
