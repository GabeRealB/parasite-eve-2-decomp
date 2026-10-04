/* Part of the stride walk library; see stride_walk.h. */

/// Script opcode: set the visibility flags of this actor's model and of the
/// carried sub-model, whose task its spawn routine parked in `pairTask`. `flags`
/// bit 0 hides both models (`TmdObject::flags` = 0) and its absence restores
/// the default 0x80; bit 1 additionally ORs in 0x4. With no sub-model spawned
/// (`Task::spawnArg1` == 0) the actor drives its own model twice.
s32 strideWalkSetVisibility(Task* task, s32 arg1, s32 flags, s32 arg3)
{
    StrideWalkWork* work;
    TmdObject*      self;
    TmdObject*      other;

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
