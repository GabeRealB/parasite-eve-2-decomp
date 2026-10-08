/* Part of the Rat library; see rat.h. */

/// Unlinks the dying rat, collapses its corpse and releases the enemy.
///
/// Requires the enemy/model/work of death task state 2. Paused updates sample
/// cached root lighting; hidden updates suppress drawing. Entry saves the root
/// pose, unlinks the target and four bodies, releases battle rewards and starts
/// collapse animation. Subsequent updates squash that pose, fade at frame 10,
/// burn at frame 15 and enter teardown at frame 60. Animation precedes each
/// running lighting sample; this handler does not compose the changed root.
static void _ratDeath(Enemy* enemy, Task* actor)
{
    enum {
        RAT_DEATH_STEP_BEGIN    = 0,
        RAT_DEATH_STEP_COLLAPSE = 1,
        RAT_DEATH_STEP_DESTROY  = 2,
        RAT_DEATH_FADE_FRAME    = 10,
        RAT_DEATH_BURN_FRAME    = 15,
        RAT_DEATH_DESTROY_FRAME = 60,
        RAT_SOUND_DEATH         = 0x40070005
    };
    // Advance slots 1..6 and sample cached root XYZ after animation, without
    // composing it. Captures actor, animationWork, animationSlot, blendFrames,
    // colorCoord and colorPosition; work/model remain live throughout.
#define RAT_ANIMATE_AND_LIGHT_CORPSE()                                                                                      \
    {                                                                                                                       \
        animationWork = actor->work;                                                                                        \
        if (animationWork->animId != animationWork->appliedAnimId) {                                                        \
            animationWork->appliedAnimId = animationWork->animId;                                                           \
            animationWork->animFrame     = 0;                                                                               \
            blendFrames                  = gRatAnimBlend[animationWork->animId];                                            \
            for (animationSlot = 1; animationSlot < ARRAY_SIZE(animationWork->rig.slots); animationSlot++) {                \
                animationSeekSlotWithBlend(&animationWork->rig.anim, animationSlot, animationWork->animId, 0, blendFrames); \
            }                                                                                                               \
        } else {                                                                                                            \
            animationWork->animFrame++;                                                                                     \
            for (animationSlot = 1; animationSlot < ARRAY_SIZE(animationWork->rig.slots); animationSlot++) {                \
                animationTickSlot(&animationWork->rig.anim, animationSlot);                                                 \
            }                                                                                                               \
        }                                                                                                                   \
        colorCoord       = actor->extra.tmd->coords;                                                                        \
        colorPosition.vx = colorCoord->workm.t[0];                                                                          \
        colorPosition.vy = colorCoord->workm.t[1];                                                                          \
        colorPosition.vz = colorCoord->workm.t[2];                                                                          \
        worldCoordUpdateActorColor(actor->spawnArg2.pointer, &colorPosition, 0, 0);                                         \
    }

    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    RatWork*   animationWork;
    GfxCoord*  colorCoord;
    VECTOR3    colorPosition;
    s32        animationSlot;
    s16        blendFrames;
    s32        soundId;
    s32        audioPan;

    model     = actor->extra.tmd;
    work      = actor->work;
    rootCoord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            colorPosition.vx = rootCoord->workm.t[0];
            colorPosition.vy = rootCoord->workm.t[1];
            colorPosition.vz = rootCoord->workm.t[2];
            worldCoordUpdateActorColor(actor->spawnArg2.pointer, &colorPosition, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            break;
    }
    switch (work->step) {
        case RAT_DEATH_STEP_BEGIN:
            // Remove all collision and target links before the corpse animation.
            work->animId       = RAT_ANIM_COLLAPSE;
            work->timer        = 0;
            work->squashScale  = ONE;
            work->savedRootMtx = rootCoord->coord;
            enemy->recs        = 0;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->sensorBody);
            worldCollisionUnlinkBody(&work->hitBody);
            worldCollisionUnlinkBody(&work->gridBody);
            worldCollisionUnlinkBody(&work->attackBody);
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
            sceneReleaseBattleRefWithRewards(actor, 7);
            work->step = RAT_DEATH_STEP_COLLAPSE;
            RAT_ANIMATE_AND_LIGHT_CORPSE();
            soundId  = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << RAT_SOUND_PLACE_INDEX_SHIFT) | RAT_SOUND_DEATH;
            audioPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            return;
        case RAT_DEATH_STEP_COLLAPSE:
            _ratSquash(actor);
            work->timer++;
            if (work->timer == RAT_DEATH_FADE_FRAME) {
                model->flags = TMD_OBJECT_SEMI_TRANS;
            }
            if (work->timer == RAT_DEATH_BURN_FRAME) {
                effectSpawn(EFFECT_CORPSE_BURN, rootCoord, 1, NULL);
            }
            if (work->timer >= RAT_DEATH_DESTROY_FRAME) {
                work->step = RAT_DEATH_STEP_DESTROY;
            }
            RAT_ANIMATE_AND_LIGHT_CORPSE();
            return;
        case RAT_DEATH_STEP_DESTROY:
            enemyDestroy(enemy, actor);
            return;
    }
#undef RAT_ANIMATE_AND_LIGHT_CORPSE
}
