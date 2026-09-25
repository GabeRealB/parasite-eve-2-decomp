#include "common.h"

#include "actors/actor_511000.h"
#include "actors/actors_shared_80132604.h"

#include "gameplay/1BC.h"

#include "main/task.h"
#include "main/tmd.h"

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Animation sources the message below selects by index.
extern void* D_actor_511000_801472E4[];

/// Animation message handler: when the source index in the payload changes,
/// reseeds the animation context from that entry of the source table; when the
/// animation id changes, restarts slots 1..19 on it, blended when the
/// payload's third word is set, steps them once and turns on the tick state's
/// per-frame stepping.
s32 func_actor_511000_80132604(Task* task, s32 arg1, ActorsShared80132604Args* msg, s32 arg3)
{
    Actor511000Work2* work;
    s32               i;
    register s32      id asm("v1");
    TmdObject*        ext;

    work = (Actor511000Work2*)task->work;
    id   = msg->field_0;
    ext  = (TmdObject*)task->extra;
    if (id != work->field_47C) {
        work->field_478 = -1;
        work->field_47C = id;
        func_800B3F84(&work->anim, D_actor_511000_801472E4[id], ext, work->field_334, work->slots);
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
