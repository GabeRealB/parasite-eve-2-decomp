#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/sound.h"

#include "rooms/dryfield_water_tank.h"

extern Task* D_dryfield_water_tank_80188D4C;

/// 1-based index of the area record the room is showing, `Gp_FindViewIndex`'s
/// result. The view gate reads it back next to `GameSession.field_52` when it
/// commits a view switch.
extern s8 D_8007216C;

/// Message 0x7DB handler: restarts the water-tank script, clearing the two
/// work-block counters and moving the task to the state the payload carries.
void func_dryfield_water_tank_8017E174(Task* task, s32 msgId, DwtMsg7DB* msg)
{
    DwtScriptWork* work;
    s32            state;

    work                = (DwtScriptWork*)task->work;
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

    work           = (DwtScriptWork*)D_dryfield_water_tank_80188D4C->work;
    work->field_50 = request;
    work->field_52 = 0;
}

/// Second sibling entry point into the script driver, the one that ends the
/// water-tank scene: publishes view 3's area-record index, asks the view gate
/// for a switch through `GameSession.field_52`, dispatches message 0x3F3 with
/// argument 1 to the driver's `owner` task, and fires the scene's sound event.
void func_dryfield_water_tank_8017E1B4(void)
{
    DwtScriptWork* work;

    work       = (DwtScriptWork*)D_dryfield_water_tank_80188D4C->work;
    D_8007216C = Gp_FindViewIndex(3);
    /* The cast is what makes this match: as `work->owner` the load carries
     * MEM_IN_STRUCT_P, and sched1's true_dependence then disregards it against
     * D_8007216C's store (`%lo` addresses do not vary), so the store sinks into
     * Gp_DispatchMsg's delay slot. Dropping the flag keeps the edge. */
    Gp_DispatchMsg(*(Task**)((u8*)work + OFFSET_OF(DwtScriptWork, owner)), 0x3F3, 1, 0);
    gGameSession->field_52 = 1;
    SndEvt_EnqueueType7(0x52150002, 0xA);
}
