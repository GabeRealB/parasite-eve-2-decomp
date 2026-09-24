#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b2_elevator.h"

/// Sets `spawnArg1` of both elevator-car tasks to 1, so each moves forwards.
s32 func_shelter_b2_elevator_8017DAB8(void)
{
    D_shelter_b2_elevator_8017EA00[0]->spawnArg1 = 1;
    D_shelter_b2_elevator_8017EA00[1]->spawnArg1 = 1;
    return 0;
}

/// Sets `spawnArg1` of both elevator-car tasks to -1, so each moves backwards.
s32 func_shelter_b2_elevator_8017DAE0(void)
{
    D_shelter_b2_elevator_8017EA00[0]->spawnArg1 = -1;
    D_shelter_b2_elevator_8017EA00[1]->spawnArg1 = -1;
    return 0;
}
