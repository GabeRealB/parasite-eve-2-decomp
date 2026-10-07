/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Emergence tick: `stateTimer` counts up from the state's 0x18 seed and the
/// deltas dispatch the one-shot actions — release state F0 at 0x18, the
/// light-mode pair at 0x1D / 0x29, the 0x600A5 spawn at 0x1D, and the
/// `TmdObject.flags` writes at 0x2F / 0x3F. From 0x1A on the tail rebuilds
/// the actor's root coordinate: a Y rotation taken from the model root's
/// facing, scaled by 0x1194 less 0xB per tick past 0x14, written back through
/// `coord.m` with `composeStamp` cleared so the local matrix is recomputed.
/// Same body as `Actor01900_Fn06904`, minus that one's 0x13 release argument;
/// the second body's grid collision follows `ODD_STRANGER_BODY2_GRID`.
void oddStrangerDie(Task* arg0)
{
    OddStrangerWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    s16              cur;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj->flags              = 0;
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
#if ODD_STRANGER_BODY2_GRID
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#else
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
#endif
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->stateTimer              = 0;
    }
    if (work->stateTimer < 0x401) {
        switch ((s16)(work->stateTimer++ - 0x18)) {
            case 0:
                sceneReleaseBattleRefWithRewards(arg0, 0xA);
                break;
            case 5:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                effectSpawn(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords + 2, 3, NULL);
                break;
            case 23:
                arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                break;
            case 17:
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
                break;
            case 39:
                arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                break;
        }
        cur = work->stateTimer;
        if (cur >= 0x1A) {
            actorRescaleYawY(arg0->extra.tmd->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
        }
    }
}
