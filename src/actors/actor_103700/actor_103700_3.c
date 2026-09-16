#include "common.h"

#include "actors/actor_103700.h"
#include "main/mem.h"

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_3", func_actor_103700_80134E24);

/// Asks the player for the melee hold (message 0x3F8, range 8) and, once it is
/// accepted, starts the grab on the actor's animation slot (message 0x3FF) and
/// flags `Actor103700Work::field_262`. The task's own unit is held for as long
/// as `GameActor::field_954` stays out of mode 2; the two message buffers come
/// from one 0x2C-byte `G_SCRATCH_HEAD` push.
s32 func_actor_103700_80134F50(Task* task)
{
    Actor103700Work*        work;
    Task*                   player;
    void*                   head;
    Actor103700HoldScratch* scratch;
    s32                     ret;

    work                    = (Actor103700Work*)task->idMap;
    player                  = Game_GetPtrSlot(3);
    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - sizeof(Actor103700HoldScratch);
    scratch                 = (Actor103700HoldScratch*)*(void**)G_SCRATCH_HEAD;

    ret = 0;
    if (((GpActorWork*)player)->actor->field_954 != 2) {
        scratch->query.field_14 = 8;
        if (Gp_DispatchMsg(player, 0x3F8, (s32)scratch, 0) == 0) {
            scratch->anim.field_0  = D_actor_103700_80139F1C;
            scratch->anim.field_4  = 1;
            scratch->anim.field_8  = 0;
            scratch->anim.field_C  = 0;
            scratch->anim.field_10 = 1;
            Gp_DispatchMsg(player, 0x3FF, (s32)&scratch->anim, 0);
            work->field_262 = 1;
            ret             = 1;
        }
    }
    *(u32*)G_SCRATCH_HEAD += sizeof(Actor103700HoldScratch);
    return ret;
}

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_3", func_actor_103700_8013503C);

void func_actor_103700_801350DC(Task* task, s32 arg1, s32 arg2)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;

    work  = (Actor103700Work*)task->idMap;
    coord = ((TmdObject*)task->extra)->field_8;

    frame           = work->field_25E + 1;
    work->field_25E = frame;
    if (arg2 < (s16)frame) {
        work->field_25E = 0;
    }
    coord->coord.t[1] += D_actor_103700_80139DB8[(arg1 * 15) + (s16)work->field_25E];
}

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_3", func_actor_103700_80135140);
