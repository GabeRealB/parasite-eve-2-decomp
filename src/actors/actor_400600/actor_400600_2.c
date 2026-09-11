#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "actors/actor_400600.h"

void func_actor_400600_80139A78(Task* arg0)
{
    Actor400600Work* work;
    s32              i;

    work = (Actor400600Work*)arg0->idMap;
    i    = 1;
    do {
        work->slots[i].field_9 = work->field_726;
        Gp_AnimResetSlot(&work->anim, i, work->field_746);
        i++;
    } while (i < 0x12);
    work->field_744 = work->field_746;
}

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_400600_80139AE8(Task* arg0)
{
    Actor400600Work* work;
    s32              i;

    work = (Actor400600Work*)arg0->idMap;
    if (work->field_744 == work->field_746) {
        i = 1;
        do {
            work->slots[i].field_9 = work->field_726;
            i++;
        } while (i < 0x12);
    } else {
        i = 1;
        do {
            work->slots[i].field_9 = work->field_726;
            func_800B4114(&work->anim, i, work->field_746, 0, work->field_720);
            i++;
        } while (i < 0x12);
        work->field_720 = 0;
    }
    work->field_744 = work->field_746;
}

s16 func_actor_400600_80139BA0(Task* arg0, s16 arg1)
{
    Actor400600Work* work = (Actor400600Work*)arg0->idMap;

    if (work->field_726 == 0) {
        return 0;
    }
    return ((arg1 << 8) / work->field_726 << 12) >> 16;
}
