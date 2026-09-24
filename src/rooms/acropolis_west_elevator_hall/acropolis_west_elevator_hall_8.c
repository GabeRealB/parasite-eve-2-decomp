#include "common.h"

#include "main/task.h"

#include "rooms/acropolis_west_elevator_hall.h"

/// Sets both of the hall's elevator-car tasks moving forwards, by storing 1 in
/// each task's `spawnArg1` (the per-frame step direction the car task reads).
s32 func_acropolis_west_elevator_hall_8017F470(void)
{
    D_acropolis_west_elevator_hall_80186AE4[0]->spawnArg1 = 1;
    D_acropolis_west_elevator_hall_80186AE4[1]->spawnArg1 = 1;
    return 0;
}

/// Sets both elevator-car tasks moving backwards, by storing -1 in each task's
/// `spawnArg1`.
s32 func_acropolis_west_elevator_hall_8017F498(void)
{
    D_acropolis_west_elevator_hall_80186AE4[0]->spawnArg1 = -1;
    D_acropolis_west_elevator_hall_80186AE4[1]->spawnArg1 = -1;
    return 0;
}
