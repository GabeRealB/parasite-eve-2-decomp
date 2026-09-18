#include "common.h"

#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/3CD8.h"
#include "actors/actor_206100.h"

void func_actor_206100_8014F18C(Task* task)
{
    Actor206100Work* work;

    work = (Actor206100Work*)task->work;

    work->obj_364.field_8  = &((TmdObject*)task->extra)->coords[1];
    work->obj_364.field_C  = work->rec_384;
    work->obj_364.field_10 = 0;
    work->obj_364.field_12 = 0;
    work->obj_364.field_14 = 0;
    work->obj_364.field_18 = 0x3003D;
    work->obj_364.field_1C = 0x400;
    work->obj_364.flags    = 1;
    Gp_LinkObj(2, &work->obj_364);
    Gp_InitRec18Table(work->rec_384, 6, 0);
    work->obj_364.flags &= 0x7FFF;

    work->obj_414.field_8  = &((TmdObject*)task->extra)->coords[4];
    work->obj_414.field_C  = work->rec_384;
    work->obj_414.field_10 = 0;
    work->obj_414.field_12 = 0;
    work->obj_414.field_14 = 0;
    work->obj_414.field_18 = 0x3003D;
    work->obj_414.field_1C = 0x200;
    work->obj_414.flags    = 1;
    Gp_LinkObj(2, &work->obj_414);
    work->obj_414.flags &= 0x7FFF;
}

void func_actor_206100_8014F284(Task* task)
{
    Actor206100Work*       work;
    Actor206100AnimStride* stride;
    s32                    i;

    work   = (Actor206100Work*)task->work;
    i      = 1;
    stride = (Actor206100AnimStride*)work + 1;
    do {
        Gp_AnimResetSlot(&work->anim, i, work->field_510);
        i++;
        stride->field_1D = (u8)work->field_51A;
        stride++;
    } while (i < 0xF);
    work->field_50E = (u16)work->field_510;
}

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Re-arms every animation slot for the pending request: writes the request's
/// step scale (`field_51A`) into each slot's `field_1D` and re-seeks the slot to
/// the requested clip with `func_800B4114`, whose fifth argument is the request's
/// own value at `field_524`.  When the clip already playing (`field_50E`) is not
/// the one requested, `field_524` is cleared as well.  `field_50E` latches the
/// clip either way, which is what lets the next frame tell the two cases apart.
/// The call sits *inside* the loop and takes a fresh `work` in `$a0` each
/// iteration, the same shape `func_actor_405800_80138294` has.
void func_actor_206100_8014F2F0(Task* arg0)
{
    Actor206100Work* work;
    s32              i;

    work = (Actor206100Work*)arg0->work;
    if (work->field_50E == work->field_510) {
        i = 1;
        do {
            ((Actor206100AnimStride*)work)[i].field_1D = (u8)work->field_51A;
            func_800B4114(&work->anim, i, work->field_510, 0, work->field_524);
            i++;
        } while (i < 0xF);
    } else {
        i = 1;
        do {
            ((Actor206100AnimStride*)work)[i].field_1D = (u8)work->field_51A;
            func_800B4114(&work->anim, i, work->field_510, 0, work->field_524);
            i++;
        } while (i < 0xF);
        work->field_524 = 0;
    }
    work->field_50E = work->field_510;
}
s16 func_actor_206100_8014F3C8(Task* arg0, s16 arg1)
{
    Actor206100Work* work = (Actor206100Work*)arg0->work;

    if (work->field_51A == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->field_51A << 12) >> 16;
}

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100_2", func_actor_206100_8014F428);
