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

/// Reseeds animation slots 1..0x12 from `animId`, forcing each slot's set
/// index to 1 first, and latches that id into `field_47E` as the one now
/// playing.
///
/// The same body as `func_actor_521100_8013677C`: the third argument is the
/// loop counter itself, and the two scaled induction variables are the
/// compiler's own, not a pair of source level pointers.
void func_actor_202900_8014A260(void)
{
    s32 i;

    i = 1;
    do {
        ActorsShared80131f9cWork->slots[i].field_9 = 1;
        Gp_AnimInitSlot(&ActorsShared80131f9cWork->anim, &ActorsShared80131f9cWork->slots[i], i,
                        (s16)ActorsShared80131f9cWork->animId);
        i++;
    } while (i < 0x13);
    ActorsShared80131f9cWork->field_47E = ActorsShared80131f9cWork->animId;
}

/// Reseeds animation slots 1..0x12 from `animId` and latches that id into
/// `field_47E` as the one now playing.
///
/// The third argument is the loop counter itself. Giving the call its own
/// counter copy (as m2c does) makes the preheader's `a2` initialisation a
/// separate pseudo, and the scheduler then orders the prologue saves around it
/// instead of leaving each `sw` paired with the load that overwrites it.
void func_actor_202900_8014A304(void)
{
    s32 i;

    i = 1;
    do {
        func_800B3AA4(&ActorsShared80131f9cWork->anim, &ActorsShared80131f9cWork->slots[i], i,
                      (s16)ActorsShared80131f9cWork->animId, 0, 8);
        i++;
    } while (i < 0x13);
    ActorsShared80131f9cWork->field_47E = ActorsShared80131f9cWork->animId;
}

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

/// Message 0x7D5 handler: applies the draw-state flags to the model the actor's
/// work block points at and to the one its display task carries. Bit 0 picks
/// visible (`field_C` cleared) or hidden, the 0x80 `Task_Kill` also ORs in; bit
/// 1 sets 0x4, the flag `Gp_UpdateActorColor` reads as "rebuild the colour
/// matrix". The two models are the same object reached two ways, and both are
/// updated in the same order everywhere.
///
/// The handler is declared with the message arguments it does not read so the
/// flags arrive in `$a2` as they do for every other handler: with a single
/// parameter the compiler copies the incoming `$a0` into the pseudo global
/// allocation gave `$a2`, one instruction the target does not have.
s32 func_actor_202900_8014A440(Task* task, s32 arg1, s32 flags)
{
    TmdObject* actorModel;
    TmdObject* taskModel;

    actorModel = D_actor_202900_80156E58->extra;
    taskModel  = (TmdObject*)D_actor_202900_80156E5C->extra;
    if (flags & 1) {
        actorModel->field_C = 0;
        taskModel->field_C  = 0;
    } else {
        actorModel->field_C = 0x80;
        taskModel->field_C  = 0x80;
    }
    if (flags & 2) {
        actorModel->field_C |= 4;
        taskModel->field_C  |= 4;
    }
    return 0;
}
