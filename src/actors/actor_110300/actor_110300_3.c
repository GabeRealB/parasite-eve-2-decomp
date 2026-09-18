#include "common.h"
#include "actors/actor_110300.h"
#include "main/task.h"
#include "main/tmd.h"

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

/// Message 0x7D5 handler: the visibility control this actor pair takes. Bit 0
/// of `flags` zeroes both models' `TmdObject::flags` and its absence restores
/// the 0x80 default; bit 1 additionally ORs in 0x4, the same bit `Tmd_Create`
/// sets for its own `flags & 1`.
///
/// The second model is the sibling task's rather than a chained one, so `self`
/// and `other` are two independent `->extra` loads. The unused middle argument
/// is load-bearing: it is what puts `flags` in `$a2`, which frees `$a1` for the
/// first model pointer. Declaring the two used arguments alone moves the flag
/// down to `$a1` and the body stops matching.
s32 func_actor_110300_801322E0(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = D_actor_110300_8013A0A4->extra;
    other = (TmdObject*)D_actor_110300_8013A0A8->extra;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}
