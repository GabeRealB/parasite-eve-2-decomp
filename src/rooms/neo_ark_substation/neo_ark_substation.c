#include "common.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/neo_ark_substation.h"

/// The room's ambience table, one `(pan, vol)` entry per area.
extern RoomAmbienceEntry D_neo_ark_substation_8017E2C8[];

/// Area id the room publishes to the sound system (see
/// `func_shelter_b4_upper_sewer_8017DB94` and friends).
extern u8 D_8007216C;

/// Keeps the substation's looping ambience in step with the area the session is
/// in: `Game_Session->field_4` selects one of the room's nine `(pan, vol)`
/// entries, and state 0 starts that loop with `SndEvt_EnqueueType6`. States 1
/// through 4 then watch for the session's index to stop matching the area
/// `D_8007216C` publishes - state 1 tests the pair and 2, 3 and 4 walk the task
/// along - and state 5 retunes the playing loop to the new entry with
/// `SndEvt_EnqueueTypeA` and returns to state 1 to keep watching.
void func_neo_ark_substation_8017D608(Task* task)
{
    s32 pan;
    s32 vol;
    u8  idx;

    idx = Game_Session->field_4;
    if (idx < 9) {
        pan = D_neo_ark_substation_8017E2C8[idx].pan;
        vol = D_neo_ark_substation_8017E2C8[idx].vol;
    } else {
        pan = 0;
        vol = 0;
    }

    switch (task->state) {
        case 0:
            SndEvt_EnqueueType6(0x55210003, (s8)pan, (s8)vol);
            task->state = task->state + 1;
            break;
        case 1:
            if (D_8007216C != Game_Session->field_4) {
                task->state = task->state + 1;
            }
            break;
        case 2:
        case 3:
        case 4:
            task->state = task->state + 1;
            break;
        case 5:
            SndEvt_EnqueueTypeA(0x55210003, (s8)pan, (s8)vol);
            task->state = 1;
            break;
    }
}

s32 func_neo_ark_substation_8017D71C(void)
{
    return 0;
}
