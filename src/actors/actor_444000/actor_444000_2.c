#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/session.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_444000_80131E90;

extern s16 D_actor_444000_80144A72;
extern s8  D_8007272D;

/// The overlay's event/controller task, whose `idMap` holds an
/// `Actor444000EventWork`.
extern Task*        D_actor_444000_80161860;
extern Actor444000* D_actor_444000_80161878;
/// Arm the actor's death sequence once: reset the `Gp_StateF0` claim block,
/// flag the session and pick area script 0xD, then latch `field_30` so a later
/// call does nothing.
void func_actor_444000_80132778(void)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;

    if (work->field_30 == 0) {
        Gp_StateF0.field_6      = 0;
        Gp_StateF0.field_1      = 0xF;
        Gp_StateF0.field_0      = 0;
        Gp_StateF0.field_2      = 0;
        Gp_StateF0.field_3      = 0;
        Game_Session->field_69 |= 0x80;
        D_8007272D              = 0xD;
        work->field_30          = 1;
    }
}

/// Set the actor's action index, resetting the sub-state counter that goes
/// with it.
void func_actor_444000_801327E8(s16 action)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;

    work->field_2C = action;
    work->field_2E = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80132808);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80132B14);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80132CB8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80133010);

/// Walk the actor's stored yaw toward `arg1`, moving at most 0x71 per call and
/// clamping the request to +/-0x200, then push the new angle onto the model:
/// part 3's coordinate is rebuilt twice around the yaw helper, the fifth
/// escort's root coordinate is refreshed, and part 4 follows.
void func_actor_444000_80133C58(Actor444000* task, s16 arg1)
{
    Actor444000Work* work = task->field_1C;
    s16              value;

    value = arg1;
    if (arg1 > 0x200) {
        value = 0x200;
    }
    if (arg1 < -0x200) {
        value = -0x200;
    }

    if (work->field_7C8 < value) {
        if (value - work->field_7C8 >= 0x72) {
            work->field_7C8 = work->field_7C8 + 0x71;
        } else {
            work->field_7C8 = value;
        }
    } else if (value < work->field_7C8) {
        if (abs(work->field_7C8 - value) >= 0x72) {
            work->field_7C8 = work->field_7C8 - 0x71;
        } else {
            work->field_7C8 = value;
        }
    }

    ((TmdObject*)task->extra)->field_8[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[3]);
    func_actor_444000_80132808(&((TmdObject*)task->extra)->field_8[3], work->field_7C8);
    ((TmdObject*)task->extra)->field_8[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[3]);
    ((TmdObject*)work->field_ECC[4]->task->extra)->field_8[0].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)work->field_ECC[4]->task->extra)->field_8[0]);
    ((TmdObject*)task->extra)->field_8[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[4]);
}
