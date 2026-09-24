#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"

/// Script command that moves the cutscene task to the given state; does
/// nothing when that task was never spawned.
void func_dryfield_dilapidated_house_8017E8A8(s32 arg0)
{
    if (D_dryfield_dilapidated_house_80189B78 != NULL) {
        D_dryfield_dilapidated_house_80189B78->state = arg0;
    }
}
