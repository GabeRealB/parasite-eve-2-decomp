#include "common.h"

#include "actors/actors_shared_80132640.h"

#include "gameplay/1BC.h"

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void ActorsShared80132640(Task* task)
{
    ActorsShared80132640Work* work;
    s32                       i;

    work = (ActorsShared80132640Work*)task->idMap;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, (s16)work->field_480, 0, work->field_4B4);
        i++;
    } while (i < 0x13);
    work->field_47E = work->field_480;
}
