#include "common.h"
#include "actors/actor_510900.h"

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

void func_actor_510900_8013BC38(Actor510900* arg0, Actor510900Coord* arg1)
{
    VECTOR pos;

    pos.vx = arg1->field_0.workm.t[0];
    pos.vy = arg1->field_0.workm.t[1];
    pos.vz = arg1->field_0.workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &pos, 0, 0);
}

void func_actor_510900_8013BC80(Actor510900* arg0)
{
    Actor510900Work* work = arg0->field_1C;

    if ((u16)(work->field_594 - 1) < 2) {
        if (--work->field_598 <= 0) {
            work->field_594 = 3;
            if (work->field_57C != 0) {
                SndEvt_EnqueueType7(work->field_57C, 0);
                work->field_57C = 0;
            }
            if (work->field_580 != 0) {
                SndEvt_EnqueueType7(work->field_580, 0);
                work->field_580 = 0;
            }
            work->obj4E4.flags &= 0x7FFF;
            work->obj504.flags &= 0x7FFF;
        }
    }
    if (work->field_594 != work->field_596) {
        if (work->field_564 != NULL) {
            work->field_564[0xD] = work->field_594;
        }
        work->field_596 = work->field_594;
    }
}

s32 func_actor_510900_8013BD5C(Actor510900* arg0)
{
    if (D_80073BA0 > 0) {
        arg0->field_1C->field_5BC = 1;
    }
    return 0;
}

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

s32 func_actor_510900_8013BD84(Actor510900* arg0, s32 arg1, Actor510900AnimArgs* arg2)
{
    Actor510900Work* work;
    s32              blend;
    s32              i;

    blend           = (arg2->field_8 != 0) * 8;
    work            = arg0->field_1C;
    work->field_586 = arg2->field_4 + 0x1B;
    for (i = 1; i < 0x13; i++) {
        func_800B4114((GpAnimCtx*)work, i, work->field_586, 0, blend);
    }
    work->field_58A = 0;
    return 0;
}
