#include "common.h"

#include "actors/actors_shared_80132604.h"

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

s32 ActorsShared80132604(Task* task, s32 arg1, ActorsShared80132604Args* msg, s32 arg3)
{
    ActorsShared80132604Work* work;
    s32                       i;
    register s32              id asm("v1");
    TmdObject*                ext;

    work = (ActorsShared80132604Work*)task->work;
    id   = msg->field_0;
    ext  = (TmdObject*)task->extra;
    if (id != work->field_47C) {
        work->field_478 = -1;
        work->field_47C = id;
        func_800B3F84(&work->anim, ActorsShared80132604Table[id], ext, work->field_334,
                      work->slots);
    }
    if (msg->field_4 != work->field_478) {
        work->field_478 = msg->field_4;
        if (msg->field_8 != 0) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->anim, i, work->field_478, 0, 6);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_478);
            }
        }
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_474 = 1;
    }
    return 0;
}
