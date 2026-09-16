#include "common.h"
#include "actors/actor_202900.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

extern Task*        D_actor_202900_80156E5C;
extern GpActorWork* D_actor_202900_80156E58;

void func_actor_202900_8014A088(GpActorWork* arg0)
{
    GsCOORDINATE2* parent;
    GsCOORDINATE2* coord;
    TmdObject*     extra;

    extra          = arg0->extra;
    parent         = D_actor_202900_80156E58->extra->field_8;
    coord          = extra->field_8;
    coord->flg     = 0;
    extra->field_C = 0;
    coord->sub     = parent + 4;
}

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", ActorsShared80131f9cSub1);

void func_actor_202900_8014A158(Task* arg0)
{
    Task_Kill(D_actor_202900_80156E5C);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives.
void func_actor_202900_8014A194(GpActorWork* arg0)
{
    if (ActorsShared80131f9cWork->field_47C == 1) {
        func_actor_202900_8014A304();
        ActorsShared80131f9cWork->field_47C = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_47C == 2) {
        func_actor_202900_8014A260();
        ActorsShared80131f9cWork->field_47C = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_47C == 3) {
        func_actor_202900_8014A208();
    }
}

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_202900_8014A208(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickSlot(&ActorsShared80131f9cWork->anim, &ActorsShared80131f9cWork->slots[i]);
        i++;
    } while (i < 0x13);
}

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A260);

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A304);

/// Watches the second animation slot for the frame the overlay reacts to:
/// while it holds 0x15, records it in `field_484` and reports whether that is
/// a change.
///
/// The mask is written at each use rather than hoisted into a `u16` local.
/// Hoisting makes the local a copy of the masked word, and combine then folds
/// the compare's zero-extension into a `move`; masking where the value is read
/// keeps the `andi $a1,$a0,0xffff`.
s32 func_actor_202900_8014A394(void)
{
    u16 frame;

    frame = ActorsShared80131f9cWork->slots[1].field_2;
    if ((frame & 0x3FF) == 0x15) {
        if (ActorsShared80131f9cWork->field_484 != (frame & 0x3FF)) {
            ActorsShared80131f9cWork->field_484 = frame & 0x3FF;
            return 1;
        }
        ActorsShared80131f9cWork->field_484 = frame & 0x3FF;
    }
    return 0;
}

/// Animation-start handler: seeds the work block's `animId` with the requested
/// one, rejecting anything from 5 up, and leaves the actor in step 2 with
/// `field_482` cleared before running the step dispatcher.
///
/// The actor is read into a local between the first two stores on purpose: that
/// is where the original evaluates it, and it is what puts the global's
/// `lui`/`lw` ahead of the `li 2` and leaves the `field_482` clear for the
/// call's delay slot.
s32 func_actor_202900_8014A3E0(Task* task, s32 arg1, Actor202900AnimArgs* args)
{
    GpActorWork* actor;

    if (args->animId < 5) {
        ActorsShared80131f9cWork->animId    = args->animId;
        actor                               = D_actor_202900_80156E58;
        ActorsShared80131f9cWork->field_47C = 2;
        ActorsShared80131f9cWork->field_482 = 0;
        func_actor_202900_8014A194(actor);
        return 0;
    }
    return -1;
}

INCLUDE_ASM("actors/nonmatchings/actor_202900/actor_202900_2", func_actor_202900_8014A440);
