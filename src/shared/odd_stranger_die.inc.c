/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Burns, flattens and fades the defeated enemy before hiding its model.
///
/// Handles `ODD_STRANGER_STATE_DEATH_BURN` on a live Odd Stranger task and
/// enemy. Entry prevents locking on. The pre-increment tick releases the battle
/// reference and credits rewards at 24, spawns the corpse burn at 29, switches
/// to black lighting at 41, enables translucency at 47 and hides the model at
/// 63. From post-increment tick 26, Y scale falls by 11 Q12 units per tick
/// relative to tick 20 while X/Z keep the normal scale. The signed-halfword
/// scale is retained even after it becomes negative; timing stops at 1025.
/// The corpse burn attaches to model part 2 and requests three bursts of three
/// flames; the burst count also supplies the child flames' size/speed tuning.
static void _oddStrangerDeathBurn(Task* task)
{
    enum {
        ODD_STRANGER_DEATH_REWARD_TICK       = 24,
        ODD_STRANGER_DEATH_BURN_TICK         = 29,
        ODD_STRANGER_DEATH_BLACK_TICK        = 41,
        ODD_STRANGER_DEATH_FADE_TICK         = 47,
        ODD_STRANGER_DEATH_HIDE_TICK         = 63,
        ODD_STRANGER_DEATH_FLATTEN_START     = 26,
        ODD_STRANGER_DEATH_FLATTEN_ORIGIN    = 20,
        ODD_STRANGER_DEATH_FLATTEN_RATE      = 11,
        ODD_STRANGER_DEATH_LAST_TICK         = 1024,
        ODD_STRANGER_DEATH_REWARD_UNUSED_ARG = 10,
        ODD_STRANGER_DEATH_BURN_PART         = 2,
        ODD_STRANGER_DEATH_BURN_BURST_COUNT  = 3 // Also the child flames' size/speed parameter
    };
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    s16              elapsedTicks;

    work  = task->work;
    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model->flags            = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_BODY2_GRID
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer              = 0;
    }
    if (work->stateTimer < (ODD_STRANGER_DEATH_LAST_TICK + 1)) {
        // Dispatch one-shot events before applying this tick's flattened pose.
        switch ((s16)work->stateTimer++) {
            case ODD_STRANGER_DEATH_REWARD_TICK:
                sceneReleaseBattleRefWithRewards(task, ODD_STRANGER_DEATH_REWARD_UNUSED_ARG);
                break;
            case ODD_STRANGER_DEATH_BURN_TICK:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                effectSpawn(EFFECT_CORPSE_BURN, task->extra.tmd->coords + ODD_STRANGER_DEATH_BURN_PART, ODD_STRANGER_DEATH_BURN_BURST_COUNT, NULL);
                break;
            case ODD_STRANGER_DEATH_FADE_TICK:
                task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case ODD_STRANGER_DEATH_BLACK_TICK:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case ODD_STRANGER_DEATH_HIDE_TICK:
                task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        elapsedTicks = work->stateTimer;
        if (elapsedTicks >= ODD_STRANGER_DEATH_FLATTEN_START) {
            _actorRenderRescaleYawY(task->extra.tmd->coords, ODD_STRANGER_ROOT_SCALE, ODD_STRANGER_ROOT_SCALE - (elapsedTicks - ODD_STRANGER_DEATH_FLATTEN_ORIGIN) * ODD_STRANGER_DEATH_FLATTEN_RATE);
        }
    }
}
