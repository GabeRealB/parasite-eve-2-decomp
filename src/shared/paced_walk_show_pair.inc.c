/* Part of the paced walk library; see paced_walk.h. */

/// Script opcode: shows or hides this actor's model and the model of the task
/// parked in `pairTask`. With `flags` bit 0 both models get `TmdObject::flags`
/// 0, which shows them; without it they get 0x80, which hides them. Bit 1
/// additionally ORs in 0x4. With `Task::spawnArg1` clear the actor drives its
/// own model twice.
s32 pacedWalkShowPair(Task* task, s32 arg1, s32 flags, s32 arg3)
{
    PacedWalkWork* work;
    TmdObject*     self;
    TmdObject*     other;

    self = task->extra.tmd;
    work = task->work;
    if (task->spawnArg1.value != 0) {
        other = work->pairTask->extra.tmd;
    } else {
        other = self;
    }
    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        other->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & 2) {
        self->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        other->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}
