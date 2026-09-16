#include "common.h"
#include "actors/actor_110300.h"
#include "main/task.h"

/// Message 0x7D3 handler: starts animation `args->animId`, rejecting anything
/// from 6 up, and leaves the actor in step 2 with `field_47A` cleared before
/// running the step dispatcher.
///
/// The actor is read into a local between the first two stores on purpose:
/// that is where the original evaluates it, and it is what puts the global's
/// `lui`/`lw` ahead of the `li 2` and leaves the `field_47A` clear for the
/// call's delay slot.
s32 func_actor_110300_80132280(Task* task, s32 arg1, Actor110300AnimArgs* args)
{
    GpActorWork* actor;

    if (args->animId < 6) {
        ActorsShared80131f9cWork->animId    = args->animId;
        actor                               = D_actor_110300_8013A0A4;
        ActorsShared80131f9cWork->field_474 = 2;
        ActorsShared80131f9cWork->field_47A = 0;
        func_actor_110300_801320C4(actor);
        return 0;
    }
    return -1;
}

INCLUDE_ASM("actors/nonmatchings/actor_110300/actor_110300_4", func_actor_110300_801322E0);
