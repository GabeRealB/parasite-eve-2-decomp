/* Part of the Actor messages library; see actor_messages.h. */

/// Returns 1 while the actor's enemy still has HP. Once it is down, returns 0
/// if the model carries bit 0x80 or lacks bit 2, and 1 otherwise.
s32 actorMsgIsPresent(Task* task)
{
    u16 flags;

    if (((Enemy*)task->spawnArg2.pointer)->hp <= 0) {
        flags = task->extra.tmd->flags;
        if (flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
            return 0;
        }
        if (flags & 2) {
            return 0;
        }
    }
    return 1;
}
