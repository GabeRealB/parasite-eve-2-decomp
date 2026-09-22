#include "common.h"
#include "actors/actors_shared_8013257c.h"
#include "gameplay/1BC.h"
#include "actors/actors_shared_801325c8.h"
#include "actors/actors_shared_80132640.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void ActorsShared8013257c(Task* task)
{
    ActorsShared8013257cWork* work;
    s32                       i;

    work = (ActorsShared8013257cWork*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x13);
}

void ActorsShared801325c8(Task* task)
{
    ActorsShared801325c8Work* work;
    s32                       i;

    work = (ActorsShared801325c8Work*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_480);
    }
    work->field_47E = work->field_480;
}

void ActorsShared80132640(Task* task)
{
    ActorsShared80132640Work* work;
    s32                       i;

    work = (ActorsShared80132640Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, (s16)work->field_480, 0, work->field_4B4);
        i++;
    } while (i < 0x13);
    work->field_47E = work->field_480;
}
