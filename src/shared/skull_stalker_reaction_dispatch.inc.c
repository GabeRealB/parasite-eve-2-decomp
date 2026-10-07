/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Per-frame dispatch on the second enemy's `state`: the idle state runs the
/// idle tick and the inert one does nothing. The status hold clears
/// `field_292` and turns the fade toward in sight, resets the remembered
/// animation id to the idle one and the counters every fourth frame, and
/// returns to the idle state once `damageTickEnemyBuildup` reports the reaction over.
void skullStalkerReactionDispatch(Task* task)
{
    SkullStalkerWork* work;

    work = task->work;
    switch (work->state) {
        case SKULL_STALKER_STATE_IDLE:
            skullStalkerIdleTick(task);
            break;
        case SKULL_STALKER_STATE_INERT:
            break;
        case SKULL_STALKER_STATE_STATUS_HOLD:
            work->field_292   = 0;
            work->hiding      = 0;
            work->phaseFrames = work->phaseFrames + 1;
            if (work->phaseFrames >= 4) {
                work->appliedAnim = SKULL_STALKER_ANIM_IDLE;
                work->animFrames  = 0;
                work->phaseFrames = 0;
            }
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                work->state = SKULL_STALKER_STATE_IDLE;
            }
            break;
    }
}
