/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Eases the roll back to level while the player still holds; once the
/// player's message 0x3ED reports the hold over, releases the player (message
/// 0x3F1, unless the hold killed the player), seeds the timer from 0x3C and
/// returns to state 2 with `holding` cleared.
void stalkerZebraIvoryReleaseHold(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;
    StalkerZebraIvoryWork* work2;

    work->roll += -work->roll >> 2;
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        if (work->holdKilledPlayer == 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        }
        _stalkerZebraIvorySeedTimer(arg0, 0x3C);
        work->roll      = 0;
        work2           = (StalkerZebraIvoryWork*)arg0->work;
        work2->state    = 2;
        work2->subState = 0;
        work->holding   = 0;
    }
}
