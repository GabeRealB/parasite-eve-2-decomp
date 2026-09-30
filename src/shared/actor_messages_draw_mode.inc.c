/* Part of the actor messages library; see actor_messages.h. */

/// Message 0x7D5 handler of both of the overlay's message tables: sets the
/// draw bits of the task's `TmdObject` from the mode in `arg2`. Mode 0 sets
/// `TMD_OBJECT_SKIP_ACTIVE_DRAW` and clears `TMD_OBJECT_SKIP_AUTO_BUFFER`,
/// mode 1 clears both, mode 2 sets both.
void actorMsgSetDrawMode(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = arg0->extra.tmd;
    switch (arg2) {
        case 0:
            extra->flags = (extra->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW) & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            return;
        case 1:
            extra->flags = extra->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case 2:
            extra->flags = extra->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}
