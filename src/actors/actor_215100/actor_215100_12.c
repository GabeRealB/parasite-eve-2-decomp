#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_215100.h"

/// Script opcode: plays clip `args->animId` (ids from 0x19 up are refused
/// with -1). With `args->withArg` set the slots are reseeded with
/// `args->animArg`, otherwise they are reset; the step body then applies it
/// straight away.
s32 func_actor_215100_8014CCE0(Task* task, s32 arg1, Actor215100AnimArgs* args)
{
    Actor215100Work* work;

    work = (Actor215100Work*)task->work;
    if (args->animId >= 0x19) {
        return -1;
    }

    work->animId = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_215100_8014C874(task);
    return 0;
}

/// Script opcode: sets the visibility flags of the actor's model and of the
/// model of the enemy spawned alongside it. `flags` bit 0 hides both
/// (`TmdObject::flags` 0) and its absence restores 0x80; bit 1 also sets 0x4.
/// The middle argument is the one every opcode of the table receives.
s32 func_actor_215100_8014CD4C(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor215100Work*)task->work)->field_4F0->extra;

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
