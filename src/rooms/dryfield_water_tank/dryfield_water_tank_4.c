#include "common.h"

#include "rooms/dryfield_water_tank.h"

extern Task* D_dryfield_water_tank_80188D4C;

/// Message 0x7DB handler: restarts the water-tank script, clearing the two
/// work-block counters and moving the task to the state the payload carries.
void func_dryfield_water_tank_8017E174(Task* task, s32 msgId, DwtMsg7DB* msg)
{
    DwtScriptWork* work;
    s32            state;

    work                = (DwtScriptWork*)task->idMap;
    work->field_4C      = 0;
    work->field_54      = 0;
    state               = msg->field_2;
    task->killCountdown = 0;
    task->state         = state;
}

/// Sibling entry point into the script driver: reaches the driver's work block
/// through the task parked in `D_dryfield_water_tank_80188D4C`, raises the
/// request halfword `field_50` — the value the driver's per-frame switch reads,
/// branches on and clears — and clears `field_52` beside it.
void func_dryfield_water_tank_8017E194(s16 request)
{
    DwtScriptWork* work;

    work           = (DwtScriptWork*)D_dryfield_water_tank_80188D4C->idMap;
    work->field_50 = request;
    work->field_52 = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017E1B4);
