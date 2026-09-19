#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/3A34.h"

#include "gameplay/1BC.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

extern s16 D_actor_444000_80144A72;

extern void func_80185220(void);

/// The overlay's event/controller task, whose `work` holds an
/// `Actor444000EventWork`.
extern Actor444000* D_actor_444000_80161878;

void func_actor_444000_80143490(s8 arg0)
{
    D_actor_444000_80161878->field_1C->field_EAC = arg0;
}

void func_actor_444000_801434A8(s16 arg0)
{
    D_actor_444000_80144A72 = arg0;
}

s16 func_actor_444000_801434B4(void)
{
    return D_actor_444000_80144A72;
}

/// The re-arm's counterpart: on a reset request it sets the two 0xEF4 counters
/// and the 0x7B0 pair rather than clearing them, and pushes `field_E = 2` onto
/// the first two escorts' model objects. Every tick it also parks one of two
/// yaw presets in `field_7C4`, alternating every 60 counts.
void func_actor_444000_801434C4(Actor444000* arg0)
{
    Actor444000Work* work;
    s16              tick;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        work->field_EF4                                         = 1;
        work->field_EF6                                         = 1;
        work->field_EFA                                         = 0;
        work->field_7B3                                         = 1;
        work->field_7B0                                         = 1;
        work->field_EFE                                         = 0;
        work->field_F1A                                         = 0;
        ((TmdObject*)work->field_ECC[0]->task->extra)->otOffset = 2;
        ((TmdObject*)work->field_ECC[1]->task->extra)->otOffset = 2;
        func_80185220();
    }
    func_actor_444000_8013441C(arg0);
    tick = work->field_6;
    if (tick % 60 == 0) {
        if (tick % 120 == 0) {
            work->field_7C4 = 0x2B2;
        } else {
            work->field_7C4 = -0x1A2;
        }
    }
}

void func_actor_444000_801435CC(Actor444000* arg0)
{
    Actor444000Work* work;
    GpEnemy*         obj;
    TmdObject*       tmd;
    s32              id;
    s32              pan;

    work = arg0->field_1C;
    obj  = arg0->field_20;
    if (work->field_4 != 0) {
        tmd             = (TmdObject*)arg0->extra;
        obj->node.flags = 0;
        tmd->flags      = 0;
        work->field_7B3 = 0xC;
        work->field_7B0 = 2;
        work->field_EF4 = 0;
        work->field_EF6 = 0;
        work->field_EFA = 0;
        work->field_7B6 = 0x10;
        work->field_EFE = 0;
        work->field_6   = 0;
    }
    if (work->field_6 == 0xA) {
        id  = ((obj->field_8 >> 12) << 8) | 0x40200017;
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->coords);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->coords));
    }
    func_actor_444000_8013441C(arg0);
    if (work->slots0[1].field_10 & 1) {
        work->field_0 = 9;
    }
}
