#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actor_341700.h"

/// Moves the task to `state` and rewinds the state machine to state 0.
static __inline__ void enter_state(Task* arg0, s32 state)
{
    Actor341700Work* w = (Actor341700Work*)arg0->work;

    arg0->state  = state;
    w->field_420 = 0;
    w->field_422 = 0;
}

/// Death: plays sound 3 unless the enemy's HP is already negative, releases
/// `Gp_StateF0`'s hold if it points at this enemy, unlinks the enemy node and
/// its three hit bodies, moves the task to state 5, tells slot-4 task 0 with
/// message 0x13F4, and hides the model.
void func_actor_341700_80167744(Task* arg0)
{
    Actor341700Work* objs;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    Actor341700Work* work;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    tmd             = (TmdObject*)arg0->extra;
    work->field_438 = 1;
    if (enemy->hp >= 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC) << 8) | 0x402C0003;
        pan     = (s8)Gp_GetObjPan(((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(((TmdObject*)arg0->extra)->coords));
    }
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->recs = 0;
    objs        = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&objs->obj_2AC);
    Gp_UnlinkObj(&objs->obj_2CC);
    Gp_UnlinkObj(&objs->obj_3AC);
    enter_state(arg0, 5);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 0, 0);
    tmd->flags |= 0x80;
}
