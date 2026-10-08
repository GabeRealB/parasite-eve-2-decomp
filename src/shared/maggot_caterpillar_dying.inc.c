/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Detaches the dying enemy from combat, collapses its model and eventually destroys it.
///
/// Requires the live enemy/task pair and initialized work. PAUSED updates only
/// colour; HIDDEN suppresses drawing without advancing death. Other controls
/// unlink target/collision state and release the battle hold once, then squash
/// and fade for 60 ticks. A burst substitutes a detached husk after two ticks.
/// The final phase waits another 60 ticks before teardown; callers must not
/// access the enemy or task after that teardown.
static void _maggotCaterpillarDyingState(Enemy* enemy, Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_DEATH_DETACH           = 0,
        MAGGOT_CATERPILLAR_DEATH_COLLAPSE         = 1,
        MAGGOT_CATERPILLAR_DEATH_WAIT             = 2,
        MAGGOT_CATERPILLAR_DEATH_FADE_FRAME       = 10,
        MAGGOT_CATERPILLAR_DEATH_FIRE_FRAME       = 15,
        MAGGOT_CATERPILLAR_DEATH_PHASE_TICKS      = 60,
        MAGGOT_CATERPILLAR_DEATH_ALERT_CLASS      = 2,
        MAGGOT_CATERPILLAR_MAGGOT_REWARD_ARG      = 26,
        MAGGOT_CATERPILLAR_CATERPILLAR_REWARD_ARG = 55
    };
    VECTOR                 colorPosition;
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    GfxCoord*              colorCoord;
    TmdObject*             model;
    s32                    rewardKindArg;

    // Stable local arguments are evaluated repeatedly; the VECTOR pad is untouched.
#define MAGGOT_CATERPILLAR_UPDATE_CORPSE_POSE(actor, colorCoord, colorPosition)         \
    do {                                                                                \
        _maggotCaterpillarTickAnimInline(actor);                                        \
        (colorCoord)       = (actor)->extra.tmd->coords;                                \
        (colorPosition).vx = (colorCoord)->workm.t[0];                                  \
        (colorPosition).vy = (colorCoord)->workm.t[1];                                  \
        (colorPosition).vz = (colorCoord)->workm.t[2];                                  \
        worldCoordUpdateActorColor((actor)->spawnArg2.pointer, &(colorPosition), 0, 0); \
    } while (0)

    model = actor->extra.tmd;
    work  = actor->work;
    coord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            colorPosition.vx = coord->workm.t[0];
            colorPosition.vy = coord->workm.t[1];
            colorPosition.vz = coord->workm.t[2];
            worldCoordUpdateActorColor(actor->spawnArg2.pointer, &colorPosition, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            switch (work->step) {
                case MAGGOT_CATERPILLAR_DEATH_DETACH:
                    // Remove every borrowed list link before releasing combat ownership.
                    work->vertical.squashScale = ONE;
                    work->baseMatrix           = coord->coord;
                    enemy->recs                = NULL;
                    worldTargetUnlinkNode(&enemy->node);
                    worldCollisionUnlinkBody(&work->gridBody);
                    worldCollisionUnlinkBody(&work->body);
                    worldCollisionUnlinkBody(&work->attackBody);
                    worldCollisionUnlinkBody(&work->flameBody);
                    rewardKindArg = MAGGOT_CATERPILLAR_CATERPILLAR_REWARD_ARG;
                    if (work->isCaterpillar == 0) {
                        rewardKindArg = MAGGOT_CATERPILLAR_MAGGOT_REWARD_ARG;
                    }
                    sceneReleaseBattleRefWithRewards(actor, rewardKindArg);
                    sceneSetEnemyAlert(MAGGOT_CATERPILLAR_DEATH_ALERT_CLASS);
                    work->stateCounter = 0;
                    work->step         = MAGGOT_CATERPILLAR_DEATH_COLLAPSE;
                    worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
                    if (work->burst != 0) {
                        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    work->animId = MAGGOT_CATERPILLAR_ANIM_HURT;
                    MAGGOT_CATERPILLAR_UPDATE_CORPSE_POSE(actor, colorCoord, colorPosition);
                    return;
                case MAGGOT_CATERPILLAR_DEATH_COLLAPSE:
                    // Burst replacement owns a separate effect; the corpse task still times teardown.
                    if (work->burst != 0) {
                        if (work->burst >= 2) {
                            work->burst = 0;
                            tmdFreePrimitiveBuffer(model);
                            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
                            _maggotCaterpillarSpawnHusk(actor);
                            _maggotCaterpillarShrinkNode2(actor);
                        } else {
                            work->burst++;
                        }
                    }
                    _maggotCaterpillarSquash(actor);
                    work->stateCounter++;
                    if (work->stateCounter == MAGGOT_CATERPILLAR_DEATH_FADE_FRAME) {
                        model->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    if (work->stateCounter == MAGGOT_CATERPILLAR_DEATH_FIRE_FRAME) {
                        effectSpawn(EFFECT_CORPSE_BURN, coord, 2, NULL);
                    }
                    if (work->stateCounter >= MAGGOT_CATERPILLAR_DEATH_PHASE_TICKS) {
                        work->step         = MAGGOT_CATERPILLAR_DEATH_WAIT;
                        work->stateCounter = 0;
                        model->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    MAGGOT_CATERPILLAR_UPDATE_CORPSE_POSE(actor, colorCoord, colorPosition);
                    return;
                case MAGGOT_CATERPILLAR_DEATH_WAIT:
                    work->stateCounter++;
                    if (work->stateCounter >= MAGGOT_CATERPILLAR_DEATH_PHASE_TICKS) {
                        enemyDestroy(enemy, actor);
                    }
                    return;
            }
            break;
    }
#undef MAGGOT_CATERPILLAR_UPDATE_CORPSE_POSE
}
