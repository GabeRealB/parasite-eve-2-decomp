/* Part of the actor messages library; see actor_messages.h. */

/// Message 0x7D5 handler: shows or hides the actor's model and the helper
/// task's together. Bit 0 of `flags` clears both models' `TmdObject::flags`
/// (shown); without it both get 0x80 (hidden). Bit 1 additionally ORs in
/// `TMD_OBJECT_SKIP_AUTO_BUFFER` on both.
s32 actorMsgSetPairVisibility(Task* task, s32 arg1, s32 flags, s32 arg3)
{
    TmdObject* self;
    TmdObject* other;

    self  = gActorSelfTask->extra.tmd;
    other = gActorHelperTask->extra.tmd;

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
