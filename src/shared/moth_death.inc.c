/* Part of the Moth library; see moth.h. */

/// Runs the moth's contact-triggered squash, burst and delayed teardown.
///
/// Paused actors keep their phase; hidden actors hide without advancing. Begin
/// raises the flock alert, saves the root, switches hit/grid tests to attack
/// tests, retires targeting and releases the battle reference. Burst ticks
/// 1..29 squash the saved pose; timer/3 selects cells only while timer < 24.
/// Tick 30 unlinks all spheres, then a 30-tick countdown destroys the enemy.
/// The post-compose local spin is retained without dirtying the cache: the
/// ordinary same-pass draw uses the squash pose, which is restored next tick.
/// The saved translation sinks 24 game units for the following squash tick.
static void _mothDeath(Enemy* enemy, Task* task)
{
    enum {
        MOTH_DEATH_BEGIN                = 0,
        MOTH_DEATH_BURST                = 1,
        MOTH_DEATH_WAIT                 = 2,
        MOTH_DEATH_BURST_END_TICK       = 30,
        MOTH_DEATH_SINK_STEP            = 24,
        MOTH_DEATH_SOUND                = 0x40070006,
        MOTH_DEATH_BATTLE_RELEASE_DELAY = 8,
        MOTH_DEATH_SPIN_MAGNITUDE_MASK  = 255,
        MOTH_DEATH_SPIN_POSITIVE_BIT    = 256
    };

    MothWork* work;
    GfxCoord* rootCoord;
    SVECTOR*  scratchHead;
    SVECTOR*  spinAngles;
    s32       spinRate;
    u32       spinDraw;
    u32       nextSeed;
    s32       soundId;
    s32       soundPan;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            scratchHead                   = SCRATCH_STACK_CURSOR(SVECTOR);
            spinAngles                    = scratchHead - 1;
            SCRATCH_STACK_CURSOR(SVECTOR) = spinAngles;
            switch (work->deathStep) {
                case MOTH_DEATH_BEGIN:
                    gSceneCombatState.actor00700DeathAlert = 1;
                    nextSeed                               = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    spinDraw                               = nextSeed >> 16;
                    spinRate                               = spinDraw & MOTH_DEATH_SPIN_MAGNITUDE_MASK;
                    task->extra.tmd->flags                 = TMD_OBJECT_SEMI_TRANS;
                    gRandomLcgState                        = nextSeed;
                    work->squashScale                      = ONE;
                    work->savedRootMtx                     = rootCoord->coord;
                    if (!(spinDraw & MOTH_DEATH_SPIN_POSITIVE_BIT)) {
                        spinRate = -spinRate;
                    }
                    work->deathSpinRate    = spinRate;
                    enemy->recs            = 0;
                    work->hitBody.flags    = work->hitBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->gridBody.flags   = work->gridBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
                    work->attackBody.flags = work->attackBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
                    soundId                = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MOTH_SOUND_PLACE_INDEX_SHIFT) | MOTH_DEATH_SOUND;
                    soundPan               = (s8)worldCoordGetOriginAudioPan(rootCoord);
                    sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
                    worldTargetUnlinkNode(&enemy->node);
                    sceneReleaseBattleRefWithRewards(task, MOTH_DEATH_BATTLE_RELEASE_DELAY);
                    work->timer     = 1;
                    work->deathStep = MOTH_DEATH_BURST;
                    break;
                case MOTH_DEATH_BURST:
                    _mothSquash(task);
                    // Squash already composed this tick's pose. The retained local
                    // spin does not invalidate that cache; the next squash restores it.
                    work->pitch    = (work->pitch + work->deathSpinRate) & ACTOR_TRANSFORM_ANGLE_MASK;
                    work->yaw      = (work->yaw + work->deathSpinRate) & ACTOR_TRANSFORM_ANGLE_MASK;
                    spinAngles->vx = work->pitch;
                    spinAngles->vy = work->yaw;
                    spinAngles->vz = 0;
                    RotMatrix(spinAngles, &rootCoord->coord);
                    work->savedRootMtx.t[1] += MOTH_DEATH_SINK_STEP;
                    if ((s16)(work->timer / MOTH_BURST_TICKS_PER_CELL) < MOTH_BURST_CELL_COUNT) {
                        _mothDrawBurst(task);
                    } else {
                        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->timer++;
                    if (work->timer >= MOTH_DEATH_BURST_END_TICK) {
                        worldCollisionUnlinkBody(&work->hitBody);
                        worldCollisionUnlinkBody(&work->gridBody);
                        worldCollisionUnlinkBody(&work->attackBody);
                        work->deathStep = MOTH_DEATH_WAIT;
                    }
                    break;
                case MOTH_DEATH_WAIT:
                    work->timer--;
                    if (work->timer <= 0) {
                        enemyDestroy(enemy, task);
                    }
                    break;
            }
            SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
            break;
    }
}
