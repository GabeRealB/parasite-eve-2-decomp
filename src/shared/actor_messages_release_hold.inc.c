/* Part of the Actor messages library; see actor_messages.h. */

/// Leaves state 0xD: to 0xE while the player has HP left, to 0x16 once it is
/// gone. Any other state is left alone.
s32 actorMsgReleaseHold(Task* task)
{
    ActorStateWork* work = (ActorStateWork*)task->work;
    PlayerStatus*   cfg  = &gPlayerStatus;

    if (work->state == 0xD) {
        if (cfg->hp > 0) {
            work->state = 0xE;
        } else {
            work->state = 0x16;
        }
    }
    return 1;
}
