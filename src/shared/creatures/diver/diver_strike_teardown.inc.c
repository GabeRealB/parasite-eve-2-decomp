/* Part of the Diver library; see diver.h. */

/// Retires a burst strike after twelve teardown updates.
///
/// Requires a live strike task with a model root and a `DiverStrikeWork` head
/// in its owned work allocation. Flight enters with `killCountdown` zero and
/// the linked `attackBody` already disabled. Each update marks the root dirty
/// and increments that counter with halfword wrapping; at twelve it unlinks
/// the body before task teardown releases the work. Does not move or draw it.
static void _diverStrikeTeardown(Task* task)
{
    DiverStrikeWork* strike;
    TmdObject*       tmd;
    u16              elapsedFrames;

    strike                    = task->work;
    tmd                       = task->extra.tmd;
    tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    elapsedFrames             = task->killCountdown + 1;
    task->killCountdown       = elapsedFrames;
    if ((s16)elapsedFrames >= DIVER_STRIKE_LINGER_FRAMES) {
        worldCollisionUnlinkBody(&strike->attackBody);
        taskKill(task);
    }
}
