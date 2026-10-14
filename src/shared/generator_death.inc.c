#include "main/random.h"

/* Part of the Generator library; see generator.h. */

/// Runs the Generator body's message-gated death and battle-release sequence.
///
/// Requires initialized enemy/work/model, kind BETA or PROTO and live room
/// resources. Paused actor control refreshes colour only; hidden control skips
/// drawing. Death release unlinks targeting/collision and starts a 120-tick
/// Y-only shrink towards Q12 scale 512, flickering through the four-frame hit
/// pulse. The held wait and shrink both emit flash/smoke every fourth tick;
/// shrink also enables translucency at tick 20, burning at 30 and black colour
/// at 110. Battle release independently grants rewards once and clears the map
/// mark. Animation/colour keep updating until both sequences permit destruction.
/// Effect offsets are root-local signed halfwords, snapshotted during spawning;
/// these callbacks never read the retained offset address. The work is retained
/// until enemy teardown.
static void _generatorDeathState(Enemy* enemy, Task* task)
{
    enum {
        GENERATOR_DEATH_MIN_SCALE_Q12         = 512,
        GENERATOR_DEATH_SHRINK_STEP_Q12       = 32,
        GENERATOR_DEATH_PULSE_FRACTION_BITS   = 12,
        GENERATOR_DEATH_EFFECT_PERIOD_MASK    = 3,
        GENERATOR_DEATH_TRANSLUCENT_FRAME     = 20,
        GENERATOR_DEATH_BURN_FRAME            = 30,
        GENERATOR_DEATH_BLACK_FRAME           = 110,
        GENERATOR_DEATH_END_FRAME             = 120,
        GENERATOR_DEATH_PULSE_DELAY_MASK      = 15,
        GENERATOR_DEATH_PULSE_MIN_DELAY       = 10,
        GENERATOR_DEATH_OFFSET_MAGNITUDE_MASK = 1023,
        GENERATOR_DEATH_OFFSET_SIGN_BIT       = 1024,
        GENERATOR_DEATH_FLASH_Y               = -2500,
        GENERATOR_DEATH_SMOKE_Y               = -2400,
        GENERATOR_DEATH_FLASH_ARGUMENT        = 0x400,
        GENERATOR_DEATH_SMOKE_ARGUMENT        = 0x30011600,
        GENERATOR_DEATH_BURN_ARGUMENT         = 5,
    };
    SVECTOR              effectOffset;
    VECTOR               worldPosition;
    TmdObject*           model;
    GeneratorWork*       work;
    GfxCoord*            rootCoord;
    GfxCoord*            colorCoord;
    GeneratorPulseFrame* frame;
    u16                  scaleQ12;
    s32                  randomBits;
    s32                  flashX;
    s32                  flashZ;
    s32                  smokeX;
    s32                  smokeZ;

/// Emits a flash followed by smoke at random offsets beneath the Generator root.
///
/// Captures live `rootCoord`, writable SVECTOR `effectOffset`, s32
/// `flashX/flashZ/smokeX/smokeZ` and the handler's `GENERATOR_DEATH_*` constants.
/// Each X/Z component advances the shared u32 LCG, selecting a signed magnitude
/// 0..1023 in root-local units; Y is -2500 for flash and -2400 for smoke.
/// The flash spawn precedes the smoke draws, preserving any random consumption
/// inside spawning. Offset XYZ narrows to s16 and its pad is untouched.
/// Spawning snapshots XYZ; both callbacks ignore the retained offset address.
/// The root remains borrowed for effect lifetime. Spawn failures do not stop
/// the second effect or undo random draws. Expands to one statement.
#define GENERATOR_DEATH_SPAWN_EFFECTS()                                                            \
    do {                                                                                           \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;          \
        flashX          = (gRandomLcgState >> 16) & GENERATOR_DEATH_OFFSET_MAGNITUDE_MASK;         \
        if (!((gRandomLcgState >> 16) & GENERATOR_DEATH_OFFSET_SIGN_BIT)) {                        \
            flashX = -flashX;                                                                      \
        }                                                                                          \
        effectOffset.vx = flashX;                                                                  \
        effectOffset.vy = GENERATOR_DEATH_FLASH_Y;                                                 \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;          \
        flashZ          = (gRandomLcgState >> 16) & GENERATOR_DEATH_OFFSET_MAGNITUDE_MASK;         \
        if (!((gRandomLcgState >> 16) & GENERATOR_DEATH_OFFSET_SIGN_BIT)) {                        \
            flashZ = -flashZ;                                                                      \
        }                                                                                          \
        effectOffset.vz = flashZ;                                                                  \
        effectSpawn(EFFECT_FLASH_BURST, rootCoord, GENERATOR_DEATH_FLASH_ARGUMENT, &effectOffset); \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;          \
        smokeX          = (gRandomLcgState >> 16) & GENERATOR_DEATH_OFFSET_MAGNITUDE_MASK;         \
        if (!((gRandomLcgState >> 16) & GENERATOR_DEATH_OFFSET_SIGN_BIT)) {                        \
            smokeX = -smokeX;                                                                      \
        }                                                                                          \
        effectOffset.vx = smokeX;                                                                  \
        effectOffset.vy = GENERATOR_DEATH_SMOKE_Y;                                                 \
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;          \
        smokeZ          = (gRandomLcgState >> 16) & GENERATOR_DEATH_OFFSET_MAGNITUDE_MASK;         \
        if (!((gRandomLcgState >> 16) & GENERATOR_DEATH_OFFSET_SIGN_BIT)) {                        \
            smokeZ = -smokeZ;                                                                      \
        }                                                                                          \
        effectOffset.vz = smokeZ;                                                                  \
        effectSpawn(EFFECT_SMOKE_PUFF, rootCoord, GENERATOR_DEATH_SMOKE_ARGUMENT, &effectOffset);  \
    } while (0)

    model     = task->extra.tmd;
    work      = task->work;
    rootCoord = model->coords;
    scaleQ12  = ONE;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            worldPosition.vx = rootCoord->workm.t[0];
            worldPosition.vy = rootCoord->workm.t[1];
            worldPosition.vz = rootCoord->workm.t[2];
            worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            break;
    }
    // Death playback and battle release are separately held by room messages.
    if ((work->releaseBits & GENERATOR_RELEASE_DEATH) && work->deathState == GENERATOR_DEATH_WAIT) {
        work->deathState = GENERATOR_DEATH_START;
    }
    if ((work->releaseBits & GENERATOR_RELEASE_BATTLE_EXIT) && work->battleExitState == GENERATOR_BATTLE_EXIT_HELD) {
        work->battleExitState = GENERATOR_BATTLE_EXIT_DUE;
    }
    switch (work->deathState) {
        case GENERATOR_DEATH_START:
            work->shrinkScale = ONE;
            work->unscaledMtx = rootCoord->coord;
            enemy->recs       = 0;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->rootBody);
            worldCollisionUnlinkBody(&work->targetBody);
            worldCoordSetActorColorMode(enemy, ENEMY_COLOR_WEIGHTED);
            work->stateFrames                       = 0;
            work->pulseState                        = GENERATOR_PULSE_IDLE;
            work->deathState                        = GENERATOR_DEATH_SHRINK;
            randomBits                              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState                         = randomBits;
            work->pulseTimer                        = (((u32)randomBits >> 16) & GENERATOR_DEATH_PULSE_DELAY_MASK) + GENERATOR_DEATH_PULSE_MIN_DELAY;
            gSceneCombatState.generatorDeathStarted = 1;
            break;
        case GENERATOR_DEATH_SHRINK:
            if (work->shrinkScale > GENERATOR_DEATH_MIN_SCALE_Q12) {
                work->shrinkScale -= GENERATOR_DEATH_SHRINK_STEP_Q12;
            }
            switch (work->pulseState) {
                case GENERATOR_PULSE_IDLE:
                    work->pulseTimer--;
                    if (work->pulseTimer <= 0) {
                        work->pulseState = GENERATOR_PULSE_HIT;
                    }
                    scaleQ12 = work->shrinkScale;
                    break;
                case GENERATOR_PULSE_HIT:
                    frame    = &_gGeneratorHitPulse[work->pulseTimer];
                    scaleQ12 = (work->shrinkScale * frame->scale) >> GENERATOR_DEATH_PULSE_FRACTION_BITS;
                    if (frame->last != 0) {
                        work->pulseState = GENERATOR_PULSE_IDLE;
                        gRandomLcgState  = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->pulseTimer = ((gRandomLcgState >> 16) & GENERATOR_DEATH_PULSE_DELAY_MASK) + GENERATOR_DEATH_PULSE_MIN_DELAY;
                    } else {
                        work->pulseTimer++;
                    }
                    break;
            }
            _modelPlacementSetScaled(task, &work->unscaledMtx, scaleQ12, MODEL_PLACEMENT_SCALE_Y_ONLY);
            if (!(work->stateFrames & GENERATOR_DEATH_EFFECT_PERIOD_MASK)) {
                GENERATOR_DEATH_SPAWN_EFFECTS();
            }
            work->stateFrames++;
            if (work->stateFrames == GENERATOR_DEATH_TRANSLUCENT_FRAME) {
                model->flags |= TMD_OBJECT_SEMI_TRANS;
            }
            if (work->stateFrames == GENERATOR_DEATH_BURN_FRAME) {
                effectSpawn(EFFECT_CORPSE_BURN, rootCoord, GENERATOR_DEATH_BURN_ARGUMENT, NULL);
            }
            if (work->stateFrames == GENERATOR_DEATH_BLACK_FRAME) {
                worldCoordSetActorColorMode(enemy, ENEMY_COLOR_BLACK);
            }
            if (work->stateFrames >= GENERATOR_DEATH_END_FRAME) {
                work->deathState = GENERATOR_DEATH_DONE;
            }
            colorCoord       = task->extra.tmd->coords;
            worldPosition.vx = colorCoord->workm.t[0];
            worldPosition.vy = colorCoord->workm.t[1];
            worldPosition.vz = colorCoord->workm.t[2];
            worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
            break;
        case GENERATOR_DEATH_DONE:
            break;
        case GENERATOR_DEATH_WAIT:
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            work->stateFrames++;
            if (!(work->stateFrames & GENERATOR_DEATH_EFFECT_PERIOD_MASK)) {
                GENERATOR_DEATH_SPAWN_EFFECTS();
            }
            break;
    }
    // Release rewards independently; the body stays live until the shrink ends.
    if (work->battleExitState == GENERATOR_BATTLE_EXIT_DUE) {
        sceneReleaseBattleRefWithRewards(task, _gGeneratorUnusedBattleReleaseArgs[work->kind]);
        work->battleExitState = GENERATOR_BATTLE_EXIT_DONE;
        areaClearMapMark(&gGameSession->location);
    }
    _generatorUpdateAnimationInline(task);
    colorCoord       = task->extra.tmd->coords;
    worldPosition.vx = colorCoord->workm.t[0];
    worldPosition.vy = colorCoord->workm.t[1];
    worldPosition.vz = colorCoord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
    if (work->deathState == GENERATOR_DEATH_DONE && work->battleExitState == GENERATOR_BATTLE_EXIT_DONE) {
        enemyDestroy(enemy, task);
    }
}
#undef GENERATOR_DEATH_SPAWN_EFFECTS
