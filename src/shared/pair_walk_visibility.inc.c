/* Part of the pair walk library; see pair_walk.h. */

/// Script opcode: set the visibility of the actor's model and of its sub-model
/// (the model of the task in `pairTask`) together. `flags` bit 0 shows both
/// (`TmdObject::flags` = 0) and its absence hides them (0x80); bit 1
/// additionally sets `TMD_OBJECT_SKIP_AUTO_BUFFER` on both.
s32 pairWalkSetVisibility(Task* task, s32 arg1, s32 flags, s32 arg3)
{
    TmdObject* self;
    TmdObject* other;

    self  = task->extra.tmd;
    other = ((PairWalkWork*)task->work)->pairTask->extra.tmd;

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
