#include "common.h"

#include "actors/actor_110800.h"
#include "main/tmd.h"

/// Message 0x7D3 handler: starts animation `args->animId`, rejecting anything
/// from 6 up, and leaves the actor in step 2 with `field_47A` cleared before
/// running the animation step driver.
///
/// The actor is read into a local between the first two stores: that puts the
/// global's `lui`/`lw` ahead of the `li 2` and leaves the `field_47A` clear
/// for the call's delay slot.
s32 func_actor_110800_80132524(Task* task, s32 arg1, Actor110800AnimArgs* args)
{
    GpActorWork* actor;

    if (args->animId < 6) {
        D_actor_110800_80139F10->animId    = args->animId;
        actor                              = D_actor_110800_80139F14;
        D_actor_110800_80139F10->field_474 = 2;
        D_actor_110800_80139F10->field_47A = 0;
        func_actor_110800_80132368((Task*)actor);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: shows or hides the actor's model and the helper
/// task's together. Bit 0 of `flags` clears both models' `TmdObject::flags`
/// (shown); without it both get 0x80 (hidden). Bit 1 additionally ORs in 0x4
/// on both.
s32 func_actor_110800_80132584(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = D_actor_110800_80139F14->extra;
    other = (TmdObject*)D_actor_110800_80139F18->extra;

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
