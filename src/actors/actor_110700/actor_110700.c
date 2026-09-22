#include "common.h"
#include "actors/actor_110700.h"
#include "gameplay/3A34.h"
#include "main/mem.h"
#include "main/tmd.h"

// Typed accesses change GCC 2.8.1's alias/CSE decisions in this initializer.
#define ACTOR_FIELD(expr, type_ptr, offset) (*(type_ptr)((s8*)(expr) + (offset)))

void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task)
{
    s32*  coordFlags;
    void* obj;
    void* work;

    obj        = ACTOR_FIELD(task, void**, 0x2C);
    coordFlags = ACTOR_FIELD(obj, s32**, 8);
    work       = memCalloc(0x480, false);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    ACTOR_FIELD(task, void**, 0x1C) = work;
    ACTOR_FIELD(obj, void**, 0x1C)  = work + 0x45C;
    ACTOR_FIELD(obj, void**, 0x20)  = work + 0x43C;
    ACTOR_FIELD(obj, s16*, 0xC)     = 0;
    func_800B3F84(work, D_actor_110700_8013BFC0, obj, work + 0x30C, work + 0x14);
    ACTOR_FIELD(work, s32*, 0x47C)  = 0;
    ACTOR_FIELD(task, void**, 0x24) = D_actor_110700_8013BFA0;
    *coordFlags                     = 0;
    ACTOR_FIELD(task, s32*, 0x30)   = 1;
}

#undef ACTOR_FIELD

/// Ticks animation slots 1..0x12 while an animation id is set, then puts the
/// model's second attach coordinate's world translation on `G_SCRATCH_HEAD`
/// and hands it to `Gp_UpdateActorColor`. The scratch push is written as a
/// compound `-=` followed by a reload: the decremented head stays a
/// short-lived pseudo in `$v0` and the reload is CSE'd into the copy of it
/// that survives the animation call.
void ActorsShared80131e24Sub1(GpEnemy* enemy, Task* task)
{
    Actor110700Work* work;
    GsCOORDINATE2*   coord;
    VECTOR*          block;
    s32              i;

    work                    = (Actor110700Work*)task->work;
    coord                   = &((TmdObject*)task->extra)->coords[1];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - 0x10;
    block                   = (VECTOR*)*(void**)G_SCRATCH_HEAD;
    if (work->field_47C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// Copies the animation id from `preset` into the work block parked in
/// `task->work` and reseeds slots 1..0x12 through `Gp_AnimResetSlot`.
s32 func_actor_110700_8013201C(Task* task, s32 arg1, Actor110700AnimPreset* preset)
{
    Actor110700Work* work;
    s32              i;

    work            = (Actor110700Work*)task->work;
    work->field_47C = preset->field_4;
    i               = 1;
    do {
        Gp_AnimResetSlot(&work->anim, i, work->field_47C);
        i++;
    } while (i < 0x13);
    return 0;
}
