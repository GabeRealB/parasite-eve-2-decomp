#include "common.h"

#include "actors/actors_shared_801324c8.h"

#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void ActorsShared801324c8(Task* task)
{
    ActorsShared801324c8Work* work;
    s32                       i;

    work = (ActorsShared801324c8Work*)task->idMap;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->field_4B8, 0, work->field_4EC);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}
