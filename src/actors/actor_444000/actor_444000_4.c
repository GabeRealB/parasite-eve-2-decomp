#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/3A34.h"

#include "gameplay/1BC.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

extern s16 D_actor_444000_80144A72;

void func_actor_444000_8013441C(Actor444000* arg0);

/// The overlay's event/controller task, whose `idMap` holds an
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

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_4", func_actor_444000_801434C4);

void func_actor_444000_801435CC(Actor444000* arg0)
{
    Actor444000Work* work;
    Actor444000Obj*  obj;
    TmdObject*       tmd;
    s32              id;
    s32              pan;

    work = arg0->field_1C;
    obj  = arg0->field_20;
    if (work->field_4 != 0) {
        tmd             = (TmdObject*)arg0->extra;
        obj->field_14   = 0;
        tmd->field_C    = 0;
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
        pan = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    func_actor_444000_8013441C(arg0);
    if (work->slots0[1].field_10 & 1) {
        work->field_0 = 9;
    }
}
