/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Counts down a kill, detaches the enemy, then destroys it after the linger period.
///
/// Paused actor control stops all death progress; hidden control hides the model
/// and disables lock-on. Otherwise the root is saved and flattened each tick.
/// Countdown ticks still advance animation. After detachment the linger ticks
/// only flatten; the terminal tick frees the enemy and requests task teardown.
static void _skullStalkerDeathState(Enemy* enemy, Task* task)
{
    enum { SKULL_STALKER_DEATH_LINGER_TICKS = 61 };
    SkullStalkerWork* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;

    work      = task->work;
    model     = task->extra.tmd;
    rootCoord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (work->deathPhase != SKULL_STALKER_DEATH_PHASE_COUNTDOWN) {
        work->savedRootMtx = rootCoord->coord;
        _skullStalkerFlatten(task);
        work->phaseFrames++;
        if (work->phaseFrames >= SKULL_STALKER_DEATH_LINGER_TICKS) {
            enemyDestroy(enemy, task);
        }
        return;
    }
    work->savedRootMtx = rootCoord->coord;
    _skullStalkerFlatten(task);
    // Detach embedded records before the linger can release their owning work.
    task->killCountdown--;
    if (task->killCountdown <= 0) {
        sceneReleaseBattleRefWithRewards(task, 0x2F);
        work->deathPhase  = SKULL_STALKER_DEATH_PHASE_LINGER;
        work->phaseFrames = 0;
        enemy->recs       = 0;
        worldTargetUnlinkNode(&enemy->node);
        worldCollisionUnlinkBody(&work->senseBody);
        worldCollisionUnlinkBody(&work->frontSenseBody);
        worldCollisionUnlinkBody(&work->body);
    }
    _skullStalkerTickAnimation(task);
}
