#include "common.h"

#include "actors/actor_103700.h"
#include "main/mem.h"

void      Gp_UpdateCoord(GsCOORDINATE2* arg0);
s32       ActorsShared801326f0(Task* arg0);
void      ActorsShared80134ff0(Task* arg0);
void      ActorsShared80135210(Task* arg0);
void      ActorsShared80133d68(Task* arg0);
void      ActorsShared8013503c(Task* arg0);
void      func_actor_103700_8013537C(Task* arg0);
extern u8 D_801153F4;

void func_actor_103700_80134E24(Actor103700Ctx* arg0, Task* task)
{
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    Actor103700Work* work;
    s32              state;
    s32              one;

    obj   = (TmdObject*)task->extra;
    state = D_801153F4;
    work  = (Actor103700Work*)task->idMap;
    coord = obj->field_8;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    obj->field_C   = 0;
    arg0->field_14 = 0;
    goto default_body;
case2:
    obj->field_C  |= 0x80;
    arg0->field_14 = one;
    return;
default_body:
    if (work->field_24E < 7) {
        func_actor_103700_8013224C(task, obj, one);
    }
    if (ActorsShared801326f0(task) != 0) {
        return;
    }
    if (work->field_254 != 0) {
        ActorsShared80133d68(task);
    }
    if (work->field_252 != 0) {
        ActorsShared8013503c(task);
    }
    if (work->field_266 != 0) {
        func_actor_103700_8013537C(task);
    }
    ActorsShared80135210(task);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
case1:
    ActorsShared80134ff0(task);
}

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
