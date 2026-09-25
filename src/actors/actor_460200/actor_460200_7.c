#include "common.h"

#include "actors/actor_460200.h"
#include "main/tmd.h"

/// Script opcode: set the visibility flags of this actor's model and of the
/// model of the partner task its spawn routine parked in `field_4F4`. `flags`
/// bit 0 hides both models (`TmdObject::flags` = 0) and its absence restores
/// the default 0x80; bit 1 additionally ORs in 0x4. With no partner spawned
/// (`Task::spawnArg1` == 0) the actor drives its own model twice.
s32 func_actor_460200_80133474(Task* task, s32 arg1, s32 flags)
{
    Actor460200PairWork* work;
    TmdObject*           self;
    TmdObject*           other;

    self = (TmdObject*)task->extra;
    work = (Actor460200PairWork*)task->work;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->field_4F4->extra;
    } else {
        other = self;
    }
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
