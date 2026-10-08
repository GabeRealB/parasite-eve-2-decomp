/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Advances the specimen's dormant, awake, status-hold or puffing behaviour.
///
/// Borrows the live task and work. Status hold freezes animation until buildup
/// finishes, then restores crawl with zero speed. Puffing holds part 1 at unit
/// Q12 scale, freezes animation and emits a puff every 16 calls. Puffing death
/// arms the death task on its third puff without invoking the burst/slump roll;
/// no current state writer selects that arm. Other states leave work untouched.
static void _sucklercephReactionDispatch(Task* task)
{
    enum { SUCKLERCEPH_PUFF_INTERVAL_FRAMES = 16,
           SUCKLERCEPH_PUFF_SIZE            = 0x400,
           SUCKLERCEPH_DEATH_PUFF_COUNT     = 3 };
    SucklercephWork* work;
    Enemy*           enemy;
    u16              nextPuffFrame;

    work = task->work;
    switch (work->state) {
        case SUCKLERCEPH_STATE_DORMANT:
            _sucklercephDormantTick(task);
            return;
        case SUCKLERCEPH_STATE_AWAKE:
            _sucklercephAwakeTick(task);
            return;
        case SUCKLERCEPH_STATE_STATUS_HOLD:
            work->animFrozen = 1;
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                work->animFrozen   = 0;
                work->state        = SUCKLERCEPH_STATE_AWAKE;
                work->awakeStage   = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
                work->forwardSpeed = 0;
            }
            _sucklercephStep(task);
            return;
        case SUCKLERCEPH_STATE_PUFFING:
            work->swellScale = ONE;
            _sucklercephScalePart(task, &task->extra.tmd->coords[1]);
            nextPuffFrame    = work->animFrames + 1;
            work->animFrames = nextPuffFrame;
            if ((s16)nextPuffFrame >= SUCKLERCEPH_PUFF_INTERVAL_FRAMES) {
                effectSpawn(EFFECT_ADDITIVE_PUFF, task->extra.tmd->coords, SUCKLERCEPH_PUFF_SIZE, &gSucklercephCollapseFxOffset);
                work->animFrames = 0;
            }
            work->animFrozen = 1;
            return;
        default:
            return;
        case SUCKLERCEPH_STATE_PUFFING_DEATH:
            work->swellScale = ONE;
            _sucklercephScalePart(task, &task->extra.tmd->coords[1]);
            nextPuffFrame    = work->animFrames + 1;
            work->animFrames = nextPuffFrame;
            if ((s16)nextPuffFrame >= SUCKLERCEPH_PUFF_INTERVAL_FRAMES) {
                effectSpawn(EFFECT_ADDITIVE_PUFF, task->extra.tmd->coords, SUCKLERCEPH_PUFF_SIZE, &gSucklercephCollapseFxOffset);
                work->animFrames = 0;
                work->swellFrames++;
                if (work->swellFrames >= SUCKLERCEPH_DEATH_PUFF_COUNT) {
                    enemy               = task->spawnArg2.pointer;
                    task->killCountdown = SUCKLERCEPH_DEATH_COUNTDOWN_FRAMES;
                    work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                    task->state         = SUCKLERCEPH_TASK_DEATH;
                    enemy->hp           = 0;
                }
            }
            work->animFrozen = 1;
    }
}
