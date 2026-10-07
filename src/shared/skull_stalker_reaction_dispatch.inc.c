/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Runs the idle behavior or holds the enemy visible during status buildup.
///
/// The status hold resets animation bookkeeping every four ticks and returns
/// to idle when buildup ends. The inert behavior runs nothing; unknown state
/// values also leave the work unchanged. Requires initialized work and enemy.
static void _skullStalkerUpdateBehavior(Task* task)
{
    enum { SKULL_STALKER_STATUS_RESET_TICKS = 4 };
    SkullStalkerWork* work;

    work = task->work;
    switch (work->state) {
        case SKULL_STALKER_STATE_IDLE:
            _skullStalkerIdleTick(task);
            break;
        case SKULL_STALKER_STATE_INERT:
            break;
        case SKULL_STALKER_STATE_STATUS_HOLD:
            work->field_292   = 0;
            work->hiding      = 0;
            work->phaseFrames = work->phaseFrames + 1;
            if (work->phaseFrames >= SKULL_STALKER_STATUS_RESET_TICKS) {
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
