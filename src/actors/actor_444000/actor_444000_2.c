#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/session.h"

#include "gameplay/1BC.h"
#include "main/mem.h"
#include "main/task.h"

#include "actors/actor_444000_rotation.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_444000_80131E90;

void func_8004BFF8(s16 angle, MATRIX* matrix);

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

/// Re-aim one of the actor's joints by `yaw` about Y, in world space: build the
/// joint's absolute rotation from its parent chain, turn it, then express the
/// result back in the parent's frame and write it into the joint. The working
/// matrix is one 0x20-byte frame carved off the scratchpad head.
void func_actor_444000_80132808(GsCOORDINATE2* coord, s16 yaw)
{
    MATRIX*        rotation;
    GsCOORDINATE2* out;

    *(MATRIX**)G_SCRATCH_HEAD -= 1;
    rotation                   = *(MATRIX**)G_SCRATCH_HEAD;
    Actor444000_AccumulateRotation(coord, rotation, &Gfx_ViewCoord);
    func_8004BFF8(yaw, rotation);
    out = Actor444000_LocalizeRotation(coord, rotation);
    __builtin_memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->flg = 0;
    Gp_UpdateCoord(out);
    *(MATRIX**)G_SCRATCH_HEAD += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80132B14);
