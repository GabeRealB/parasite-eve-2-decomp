#include "common.h"

#include "actors/actor_161500.h"
#include "main/task.h"
#include "main/tmd.h"

/// Script opcode: hides or shows this actor's model and the model of the pair
/// task its spawn routine parked in `pairTask`. Without `flags` bit 0 both
/// models get `TmdObject::flags` 0x80, which hides them; with it the flags are
/// cleared. Bit 1 additionally ORs in 0x4. With no pair spawned
/// (`Task::spawnArg1` == 0) the actor drives its own model twice.
s32 func_actor_161500_80132A94(Task* task, s32 arg1, s32 flags)
{
    Actor161500Work* work;
    TmdObject*       self;
    TmdObject*       other;

    self = (TmdObject*)task->extra;
    work = (Actor161500Work*)task->work;
    if (task->spawnArg1 != 0) {
        other = (TmdObject*)work->pairTask->extra;
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
