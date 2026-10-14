/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Finishes the grab after the player's hold animation stops and resumes walking.
///
/// Eases roll toward level while waiting. Unless the bite killed the player,
/// releases the player's scripted control. Seeds the attack timer to 60..138
/// update ticks, levels roll, resets the walk sub-state and clears `holding`.
/// Requires the live player task; the animation is advanced by the caller.
static void _stalkerZebraIvoryReleaseHold(Task* task)
{
    enum { STALKER_ZEBRA_IVORY_RELEASE_ATTACK_BASE_TICKS = 60 };
    StalkerZebraIvoryWork* work = task->work;

    work->roll += -work->roll >> 2;
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        if (work->holdKilledPlayer == 0) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        }
        _stalkerZebraIvorySeedTimer(task, STALKER_ZEBRA_IVORY_RELEASE_ATTACK_BASE_TICKS);
        work->roll = 0;
        _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_WALK);
        work->holding = 0;
    }
}
