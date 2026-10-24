#include "gameplay/room_effects.h"

/* Part of the Generator library; see generator.h. */

/// Scratch-stack block of the body's hit handler, reserved for the length of
/// the call.
typedef struct {
    VECTOR  toPlayer;     // the player's position less the body's root, the range a hit's damage is computed from
    SVECTOR effectOffset; // where a hit effect appears, as an offset from the body's root: the kind's entry of the hit-effect offsets
} _GeneratorBodyHitScratch;
STATIC_ASSERT_SIZEOF(_GeneratorBodyHitScratch, 0x18);

/// Applies body HP loss and chooses the protected floor, hit reaction or held death.
///
/// Requires the initialized body task and its work and Enemy. Retains the
/// original positive-HP reaction even for zero damage. No resource is released.
/// A protected lethal hit restores one HP without starting the hit animation;
/// an unprotected lethal hit leaves battle rewards and teardown held for messages.
static inline void _generatorApplyBodyDamage(Task* task, GeneratorWork* work, Enemy* enemy, s32 damage)
{
    enum { GENERATOR_PROTECTED_HP_FLOOR = 1 };

    enemy->hp -= damage;
    if (enemy->hp <= 0) {
        if (work->lifeSupportDestroyed == 0) {
            enemy->hp = GENERATOR_PROTECTED_HP_FLOOR;
        } else {
            task->state           = GENERATOR_TASK_TEARDOWN;
            work->deathState      = GENERATOR_DEATH_WAIT;
            work->battleExitState = GENERATOR_BATTLE_EXIT_HELD;
            work->alive           = 0;
            work->animSet         = GENERATOR_ANIM_DEATH;
        }
    } else {
        work->pulseState  = GENERATOR_PULSE_HIT;
        work->stateFrames = 0;
        work->animSet     = GENERATOR_ANIM_HIT;
    }
}

/// Applies queued player attacks to the generator body and consumes its contacts.
///
/// Requires a live model task with an Enemy and GeneratorWork. Both contact
/// slots are checked after the signed frame cooldown expires. Life Support
/// reduces damage to one tenth and preserves at least one HP; after it breaks,
/// critical hits deal four times the damage and a killing hit starts the held
/// death sequence. Effects are suppressed only for consecutive equal attack
/// keys; every processed contact can arm a cooldown and play a sound.
/// Reserves one _GeneratorBodyHitScratch block for this call.
static void _generatorBodyHit(Task* task)
{
    enum {
        GENERATOR_PROTECTED_DAMAGE_DIVISOR = 10,
        GENERATOR_BODY_HIT_SOUND_INDEX     = 2
    };
    _GeneratorBodyHitScratch* scratch;
    GeneratorWork*            work;
    Enemy*                    enemy;
    GfxCoord*                 rootCoord;
    s32                       damage;
    s32                       lastAttackKey;
    s32                       hitParameter;
    s16                       cooldownFrames;
    s32                       soundId;
    s32                       contactIndex;

    scratch       = SCRATCH_STACK_RESERVE_BLOCK(_GeneratorBodyHitScratch);
    rootCoord     = task->extra.tmd->coords;
    work          = task->work;
    enemy         = task->spawnArg2.pointer;
    lastAttackKey = 0;
    if (work->hitCooldown != 0) {
        cooldownFrames    = work->hitCooldown - 1;
        work->hitCooldown = cooldownFrames;
        if (cooldownFrames <= 0) {
            work->hitCooldown = 0;
        }
    }
    if (work->hitCooldown == 0) {
        for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
            if ((work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_ATTACK) {
                continue;
            }
            // Measure damage in the player and root coordinates' common parent frame.
            scratch->toPlayer.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            scratch->toPlayer.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
            scratch->toPlayer.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            damage               = damageComputePlayerAttack(work->contacts[contactIndex].key.value, SquareRoot0(scratch->toPlayer.vx * scratch->toPlayer.vx + scratch->toPlayer.vy * scratch->toPlayer.vy + scratch->toPlayer.vz * scratch->toPlayer.vz), 0, 0);
            if (work->lifeSupportDestroyed == 0) {
                damage /= GENERATOR_PROTECTED_DAMAGE_DIVISOR;
            } else if (damageRollCriticalHit(enemy, work->contacts[contactIndex].key.value, 0) != 0) {
                damage                  *= 4;
                scratch->effectOffset.vx = _gGeneratorHitEffectOffsets[work->kind].vx;
                scratch->effectOffset.vy = _gGeneratorHitEffectOffsets[work->kind].vy;
                scratch->effectOffset.vz = _gGeneratorHitEffectOffsets[work->kind].vz;
                effectSpawn(EFFECT_CRITICAL_HIT, rootCoord, 0, &scratch->effectOffset);
            }
            worldTargetAddReadoutAmount(&enemy->node, damage, 0);
            damageAccumulateLifeDrainHp(enemy, work->contacts[contactIndex].key.value, damage, 0);
            _generatorApplyBodyDamage(task, work, enemy, damage);
            if (lastAttackKey != work->contacts[contactIndex].key.value) {
                lastAttackKey            = work->contacts[contactIndex].key.value;
                hitParameter             = damageGetPlayerAttackEffectId(lastAttackKey);
                scratch->effectOffset.vx = _gGeneratorHitEffectOffsets[work->kind].vx;
                scratch->effectOffset.vy = _gGeneratorHitEffectOffsets[work->kind].vy;
                scratch->effectOffset.vz = _gGeneratorHitEffectOffsets[work->kind].vz;
                if (hitParameter == EFFECT_HIT_KIND_BLAST) {
                    effectSpawn(EFFECT_HIT_BLAST, rootCoord, work->effectArg.spawnArgLo | (work->effectArg.spawnArgHi << 16), &scratch->effectOffset);
                } else {
                    effectSpawnHit((u16)hitParameter, rootCoord, &scratch->effectOffset, &work->effectArg);
                }
            }
            hitParameter = damageGetPlayerAttackHitCooldown(work->contacts[contactIndex].key.value);
            if (hitParameter > 0) {
                work->hitCooldown = hitParameter;
            }
            soundId = _gGeneratorHitSoundIds[GENERATOR_BODY_HIT_SOUND_INDEX] | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
        }
    }
    // Discard contacts even while the hit cooldown blocks damage.
    worldCollisionClearContacts(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(_GeneratorBodyHitScratch);
}

#include "main/random.h"

/* Part of the Generator library; see generator.h. */

/// Advances the body's scale pulse and updates its running sound for the view.
///
/// Requires initialized body work and model coordinates. Idle pulses repeat
/// after a random 30..93-frame delay; hit pulses play once, then wait until
/// 35 frames beyond the requested animation's blend duration before requesting
/// idle. Clip terminator rows are applied. Scale has twelve fractional bits
/// and replaces the saved root matrix without compounding previous scales.
/// Sound mixing requires viewReady == 1 and a valid per-view sound-table index.
static void _generatorPulse(Task* task)
{
    enum {
        GENERATOR_PULSE_MIN_DELAY_FRAMES  = 30,
        GENERATOR_PULSE_DELAY_JITTER_MASK = 63,
        GENERATOR_HIT_RECOVERY_FRAMES     = 35
    };
    GeneratorWork* work;
    GfxCoord*      rootCoord;
    s16            scale;
    s32            panOffset;
    s32            soundId;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    scale     = ONE;
    switch (work->pulseState) {
        case GENERATOR_PULSE_IDLE:
            if (work->pulseTimer <= 0) {
                scale = _gGeneratorIdlePulse[work->stateFrames].scale;
                if (_gGeneratorIdlePulse[work->stateFrames].last != 0) {
                    work->stateFrames = 0;
                    RANDOM_LCG_ADVANCE();
                    work->pulseTimer = (RANDOM_LCG_DRAW() & GENERATOR_PULSE_DELAY_JITTER_MASK) + GENERATOR_PULSE_MIN_DELAY_FRAMES;
                    soundId          = _gGeneratorPulseSoundId |
                              ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    panOffset = (s8)worldCoordGetOriginAudioPan(rootCoord);
                    sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(rootCoord));
                } else {
                    work->stateFrames = work->stateFrames + 1;
                }
            } else {
                work->pulseTimer = work->pulseTimer - 1;
            }
            break;
        case GENERATOR_PULSE_HIT:
            scale = _gGeneratorHitPulse[work->stateFrames].scale;
            if (_gGeneratorHitPulse[work->stateFrames].last != 0) {
                work->stateFrames = 0;
                work->pulseState  = GENERATOR_PULSE_HIT_RECOVER;
                RANDOM_LCG_ADVANCE();
                work->pulseTimer = (RANDOM_LCG_DRAW() & GENERATOR_PULSE_DELAY_JITTER_MASK) + GENERATOR_PULSE_MIN_DELAY_FRAMES;
            } else {
                work->stateFrames = work->stateFrames + 1;
            }
            break;
        case GENERATOR_PULSE_HIT_RECOVER:
            if (work->animFrames >= _gGeneratorAnimationBlendFrames[work->animSet] + GENERATOR_HIT_RECOVERY_FRAMES) {
                work->animSet    = GENERATOR_ANIM_IDLE;
                work->pulseState = GENERATOR_PULSE_IDLE;
            }
            break;
    }
    // Always start from the saved matrix so pulse scales do not accumulate.
    _modelPlacementSetScaled(task, &work->unscaledMtx, scale, MODEL_PLACEMENT_SCALE_UNIFORM);
    if (gGameSession->viewReady == 1) {
        sndEvtRequestScriptMix(work->runningSoundId, (s8)_gGeneratorViewSound[gGameSession->location.view].panOffset,
                               (s8)_gGeneratorViewSound[gGameSession->location.view].attenuation);
    }
}

/// Applies or ticks the body's animation-set request inside the death handler.
///
/// Requires initialized GeneratorWork; a changed request must be in 1..3.
/// Initial equal zero requests retain the idle slots started at spawn. A changed
/// request blends slots 1..9 from frame zero and resets animFrames; otherwise
/// those slots tick and the signed halfword counter advances. Slot 0 is unused.
/// The ordinary frame handler uses the same operation out of line.
static inline void _generatorUpdateAnimationInline(Task* task)
{
    GeneratorWork* work;
    s32            slotIndex;
    s32            blendFrames;

    work = task->work;
    if (work->animSet != work->appliedAnimSet) {
        work->appliedAnimSet = work->animSet;
        work->animFrames     = 0;
        blendFrames          = _gGeneratorAnimationBlendFrames[work->animSet];
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->anim, slotIndex, work->animSet, 0, blendFrames);
        }
    } else {
        work->animFrames++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
            animationTickSlot(&work->anim, slotIndex);
        }
    }
}

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
        RANDOM_LCG_ADVANCE();                                                                      \
        flashX = RANDOM_LCG_DRAW() & GENERATOR_DEATH_OFFSET_MAGNITUDE_MASK;                        \
        if (!(RANDOM_LCG_DRAW() & GENERATOR_DEATH_OFFSET_SIGN_BIT)) {                              \
            flashX = -flashX;                                                                      \
        }                                                                                          \
        effectOffset.vx = flashX;                                                                  \
        effectOffset.vy = GENERATOR_DEATH_FLASH_Y;                                                 \
        RANDOM_LCG_ADVANCE();                                                                      \
        flashZ = RANDOM_LCG_DRAW() & GENERATOR_DEATH_OFFSET_MAGNITUDE_MASK;                        \
        if (!(RANDOM_LCG_DRAW() & GENERATOR_DEATH_OFFSET_SIGN_BIT)) {                              \
            flashZ = -flashZ;                                                                      \
        }                                                                                          \
        effectOffset.vz = flashZ;                                                                  \
        effectSpawn(EFFECT_FLASH_BURST, rootCoord, GENERATOR_DEATH_FLASH_ARGUMENT, &effectOffset); \
        RANDOM_LCG_ADVANCE();                                                                      \
        smokeX = RANDOM_LCG_DRAW() & GENERATOR_DEATH_OFFSET_MAGNITUDE_MASK;                        \
        if (!(RANDOM_LCG_DRAW() & GENERATOR_DEATH_OFFSET_SIGN_BIT)) {                              \
            smokeX = -smokeX;                                                                      \
        }                                                                                          \
        effectOffset.vx = smokeX;                                                                  \
        effectOffset.vy = GENERATOR_DEATH_SMOKE_Y;                                                 \
        RANDOM_LCG_ADVANCE();                                                                      \
        smokeZ = RANDOM_LCG_DRAW() & GENERATOR_DEATH_OFFSET_MAGNITUDE_MASK;                        \
        if (!(RANDOM_LCG_DRAW() & GENERATOR_DEATH_OFFSET_SIGN_BIT)) {                              \
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
            randomBits                              = RANDOM_LCG_NEXT(gRandomLcgState);
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
                        RANDOM_LCG_ADVANCE();
                        work->pulseTimer = (RANDOM_LCG_DRAW() & GENERATOR_DEATH_PULSE_DELAY_MASK) + GENERATOR_DEATH_PULSE_MIN_DELAY;
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

/// Creates the Generator's targetable Life Support sphere and marks it intact.
///
/// Requires a coordinate-body child of a live Generator task whose kind is
/// GENERATOR_BETA or GENERATOR_PROTO. Positions use world game units beneath
/// the view. Owns zeroed part work and one contact slot until enemy teardown;
/// the parent must outlive it. Hides the room's Life Support sprites, clears
/// its part-down flag and enters the active state. Allocation failure destroys
/// the child before linking any target or collision records.
static void _generatorLifeSupportSpawn(Enemy* enemy, Task* task)
{
    enum {
        GENERATOR_LIFE_SUPPORT_RADIUS               = 200,
        GENERATOR_LIFE_SUPPORT_HIT_EFFECT_MAGNITUDE = 1280,
        GENERATOR_LIFE_SUPPORT_HIT_EFFECT_COUNT     = 2
    };
    GeneratorWork*            parentWork;
    GeneratorWork*            reloadedParentWork;
    GeneratorLifeSupportWork* part;
    GfxCoord*                 coord;
    WorldCollisionContact*    contacts;
    s32                       flag;
    s16                       kind;

    coord      = task->extra.coordBody->coord;
    parentWork = task->parent->work;
    part       = memCalloc(sizeof(GeneratorLifeSupportWork), 0);
    if (part == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work          = part;
    coord->parent       = &gGfxViewCoord;
    coord->coord.t[0]   = _gGeneratorLifeSupportPositions[parentWork->kind].x;
    coord->coord.t[1]   = _gGeneratorLifeSupportPositions[parentWork->kind].y;
    coord->coord.t[2]   = _gGeneratorLifeSupportPositions[parentWork->kind].z;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    worldTargetLinkNode(&enemy->node);
    contacts                    = part->contacts;
    enemy->coord                = coord;
    enemy->bodyPos.vx           = 0;
    enemy->bodyPos.vy           = 0;
    enemy->bodyPos.vz           = 0;
    enemy->param                = &_gGeneratorLifeSupportParams;
    enemy->recs                 = contacts;
    enemy->hp                   = _gGeneratorLifeSupportParams.hpMax;
    part->effectArg.spawnArgLo  = GENERATOR_LIFE_SUPPORT_HIT_EFFECT_MAGNITUDE;
    part->effectArg.coord       = coord;
    part->effectArg.spawnArgHi  = GENERATOR_LIFE_SUPPORT_HIT_EFFECT_COUNT;
    part->body.coord            = coord;
    part->body.context.contacts = contacts;
    part->body.pos.vx           = 0;
    part->body.pos.vy           = 0;
    part->body.pos.vz           = 0;
    // Inherit the parent's collision key for this independently linked sphere.
    reloadedParentWork = task->parent->work;
    part->body.key     = reloadedParentWork->rootBody.key;
    part->body.radius  = GENERATOR_LIFE_SUPPORT_RADIUS;
    part->body.flags   = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &part->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(part->contacts), 0);
    part->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    kind              = parentWork->kind;
    part->kind        = kind;
    // Use the same variant for sprite visibility and the saved intact flag.
    if (kind == GENERATOR_BETA) {
        neoArkPowerPlant2SetView6SpritesHidden(1);
        flag = GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN;
    } else {
        neoArkPowerPlant1SetLifeSupportSpritesHidden(1);
        flag = GAME_FLAG_POWER_PLANT_1_GENERATOR_PART_DOWN;
    }
    gameFlagSetNibble(flag, 0);
    task->state = GENERATOR_TASK_ACTIVE;
}

#include "gameplay/room_effects.h"

/* Part of the Generator library; see generator.h. */

/// Processes the Life Support part's attack contact and starts teardown on death.
///
/// Requires initialized part work and a live parent GeneratorWork. Paused or
/// hidden actor control leaves contacts and timers intact. Attachment attacks
/// show a zero readout without dealing damage; weapon attacks use root-to-player
/// distance and fourfold critical damage. Surviving positive damage arms the
/// attack cooldown and rate-limited effects. A killing hit disables the body's
/// protection and regeneration immediately. Borrows one scratch VECTOR.
static void _generatorLifeSupportHit(Enemy* enemy, Task* task)
{
    enum {
        GENERATOR_LIFE_SUPPORT_ATTACHMENT_ATTACK   = 0x8000,
        GENERATOR_LIFE_SUPPORT_HIT_EFFECT_FRAMES   = 10,
        GENERATOR_LIFE_SUPPORT_HIT_SOUND_INDEX     = 0,
        GENERATOR_LIFE_SUPPORT_BREAK_SOUND_INDEX   = 1,
        GENERATOR_LIFE_SUPPORT_BREAK_EXPLOSION_ARG = 0x10002400,
        GENERATOR_LIFE_SUPPORT_BREAK_SMOKE_ARG     = 0x32FF1400
    };
    VECTOR*                   toPlayer;
    GeneratorLifeSupportWork* part;
    GfxCoord*                 coord;
    s32                       damage;
    s32                       soundId;
    s32                       hitCooldownFrames;

    coord = task->extra.coordBody->coord;
    part  = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    toPlayer = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    if (part->hitCooldown != 0) {
        part->hitCooldown--;
        if (part->hitCooldown <= 0) {
            part->hitCooldown = 0;
        }
    }
    if (part->hitEffectCooldown != 0) {
        part->hitEffectCooldown--;
    }
    // The part rejects attachment/PE attacks without consuming a new cooldown.
    if (part->hitCooldown == 0 && (part->contacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_ATTACK) {
        if (part->contacts[0].key.value & GENERATOR_LIFE_SUPPORT_ATTACHMENT_ATTACK) {
            worldTargetAddReadoutAmount(&enemy->node, 0, 0);
        } else {
            toPlayer->vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            toPlayer->vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
            toPlayer->vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            damage       = damageComputePlayerAttack(part->contacts[0].key.value, SquareRoot0(toPlayer->vx * toPlayer->vx + toPlayer->vy * toPlayer->vy + toPlayer->vz * toPlayer->vz), 0, 0);
            if (damageRollCriticalHit(enemy, part->contacts[0].key.value, 0) != 0) {
                damage *= 4;
                effectSpawn(EFFECT_CRITICAL_HIT, coord, 0, NULL);
            }
            worldTargetAddReadoutAmount(&enemy->node, damage, 0);
            enemy->hp -= damage;
            if (enemy->hp <= 0) {
                // Drop the parent's protection now; the part tears down on later ticks.
                task->state                                                = GENERATOR_TASK_TEARDOWN;
                part->teardownFrames                                       = 0;
                ((GeneratorWork*)task->parent->work)->lifeSupportDestroyed = 1;
                effectSpawn(EFFECT_EXPLOSION, coord, GENERATOR_LIFE_SUPPORT_BREAK_EXPLOSION_ARG, NULL);
                effectSpawn(EFFECT_SMOKE_PUFF, coord, GENERATOR_LIFE_SUPPORT_BREAK_SMOKE_ARG, NULL);
                soundId  = _gGeneratorHitSoundIds[GENERATOR_LIFE_SUPPORT_BREAK_SOUND_INDEX];
                soundId |= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else if (damage > 0) {
                if (part->hitEffectCooldown == 0) {
                    if ((u16)damageGetPlayerAttackReaction(part->contacts[0].key.value) == DAMAGE_PLAYER_REACTION_INCENDIARY) {
                        effectSpawnHit(EFFECT_HIT_KIND_BLAST, coord, NULL, &part->effectArg);
                    }
                    effectSpawnHit(EFFECT_HIT_KIND_SPARK_BURST, coord, NULL, &part->effectArg);
                    part->hitEffectCooldown = GENERATOR_LIFE_SUPPORT_HIT_EFFECT_FRAMES;
                }
                hitCooldownFrames = damageGetPlayerAttackHitCooldown(part->contacts[0].key.value);
                if (hitCooldownFrames > 0) {
                    part->hitCooldown = hitCooldownFrames;
                }
                soundId  = _gGeneratorHitSoundIds[GENERATOR_LIFE_SUPPORT_HIT_SOUND_INDEX];
                soundId |= (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
        }
    }
    worldCollisionClearContacts(part->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Initializes a generator body, its animation rig and collision, and its child part.
///
/// Requires a model task whose spawnArg2.pointer is enemy, and a placement
/// index valid in the synchronized area variant. Allocates zeroed primary-heap
/// GeneratorWork owned by the task; allocation failure destroys the enemy.
/// Initializes slots 1..9, borrows the package's animation and message tables,
/// acquires one battle reference and parents a Life Support task to the body.
/// Child spawning is assumed to succeed, as in the original implementation.
/// Installs the running sound and enters the active state.
static void _generatorSpawn(Enemy* enemy, Task* task)
{
    enum {
        GENERATOR_BODY_RADIUS           = 1500,
        GENERATOR_TARGET_RADIUS         = 300,
        GENERATOR_HIT_EFFECT_MAGNITUDE  = 1280,
        GENERATOR_BODY_HIT_EFFECT_COUNT = 3
    };
    GeneratorWork*        work;
    TmdObject*            bodyModel;
    GfxCoord*             rootCoord;
    GameLocationKey*      sessionLocation;
    AreaVariant*          areaVariant;
    AreaPlacement*        placement;
    ModelObjectCoordBody* lifeSupportBody;
    TmdObject*            lifeSupportModelView;
    Enemy*                lifeSupportEnemy;
    GameLocationKey       location;
    u16                   placementIndex;
    s32                   soundId;
    s32                   slotIndex;

    bodyModel = task->extra.tmd;
    rootCoord = bodyModel->coords;
    work      = memCalloc(sizeof(GeneratorWork), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    bodyModel->flags        = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    bodyModel->lightMtx     = &work->lightMtx;
    bodyModel->colorMtx     = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord               = rootCoord;
    enemy->bodyPos.vx          = _gGeneratorTargetOffsets[GENERATOR_KIND].vx;
    enemy->bodyPos.vy          = _gGeneratorTargetOffsets[GENERATOR_KIND].vy;
    enemy->bodyPos.vz          = _gGeneratorTargetOffsets[GENERATOR_KIND].vz;
    enemy->param               = &_gGeneratorBodyParams;
    enemy->recs                = work->contacts;
    enemy->hp                  = _gGeneratorBodyParams.hpMax;
    work->effectArg.coord      = rootCoord;
    work->effectArg.spawnArgLo = GENERATOR_HIT_EFFECT_MAGNITUDE;
    work->effectArg.spawnArgHi = GENERATOR_BODY_HIT_EFFECT_COUNT;
    // The task owns the context and ten encoded pose buffers for the rig.
    animationInitContext(&work->anim, _gGeneratorAnimSets, bodyModel, work->poses, work->slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
        animationResetSlot(&work->anim, slotIndex, GENERATOR_ANIM_IDLE);
    }
    sceneAcquireBattleRef(0);
    work->kind        = GENERATOR_KIND;
    work->shrinkScale = ONE;
    work->unscaledMtx = rootCoord->coord;
    work->alive       = 1;
    work->hpCeiling   = _gGeneratorBodyParams.hpMax;
    // Both spheres share the two-contact table; the target sphere uses bodyPos.
    work->rootBody.coord            = rootCoord;
    work->rootBody.context.contacts = work->contacts;
    work->rootBody.pos.vx           = 0;
    work->rootBody.pos.vy           = 0;
    work->rootBody.pos.vz           = 0;
    work->rootBody.key              = GENERATOR_COLLISION_KEY;
    work->rootBody.radius           = GENERATOR_BODY_RADIUS;
    work->rootBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->rootBody);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->rootBody.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->targetBody.coord            = rootCoord;
    work->targetBody.context.contacts = work->contacts;
    work->targetBody.pos.vx           = _gGeneratorTargetOffsets[GENERATOR_KIND].vx;
    work->targetBody.pos.vy           = _gGeneratorTargetOffsets[GENERATOR_KIND].vy;
    work->targetBody.pos.vz           = _gGeneratorTargetOffsets[GENERATOR_KIND].vz;
    work->targetBody.key              = GENERATOR_COLLISION_KEY;
    work->targetBody.radius           = GENERATOR_TARGET_RADIUS;
    work->targetBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->targetBody);
    work->targetBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    lifeSupportEnemy        = enemySpawnFromTable(GENERATOR_TASK_TABLE, GENERATOR_LIFE_SUPPORT_TASK_INDEX, 0, enemy);
    // Keep the original TMD texture/buffer operations on the coordinate-only child.
    // These offsets address bytes in ownedCoord; their purpose is unproven.
    lifeSupportBody      = lifeSupportEnemy->task->extra.coordBody;
    lifeSupportModelView = (TmdObject*)lifeSupportBody;
    placementIndex       = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    sessionLocation      = &gGameSession->location;
    location.stage       = sessionLocation->stage;
    location.area        = sessionLocation->area;
    location.roomState   = sessionLocation->roomState;
    location.view        = sessionLocation->view;
    areaSyncLocationVariant(&location);
    areaVariant                             = areaGetVariant(&location);
    placement                               = AREA_PLACEMENT_AT(areaVariant->placements, placementIndex);
    lifeSupportModelView->texturePageOffset = placement->texturePageOffset;
    lifeSupportModelView->clutRowOffset     = placement->clutRowOffset;
    if (lifeSupportModelView->buffer != NULL) {
        tmdBuildBufferHalf(lifeSupportModelView);
        tmdBuildBufferHalf(lifeSupportModelView);
    }
    soundId              = _gGeneratorRunningSoundId | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    work->runningSoundId = soundId;
    sndEvtRequestScriptStart(soundId, (s8)_gGeneratorViewSound[gGameSession->location.view].panOffset,
                             (s8)_gGeneratorViewSound[gGameSession->location.view].attenuation);
    task->msgTable = _gGeneratorMessages;
    task->state    = GENERATOR_TASK_ACTIVE;
}

/// Runs one active body update subject to the scene's actor-control mode.
///
/// Running mode restores drawing and hides the HP readout. Paused mode only
/// updates color; hidden mode suppresses drawing and lock-on. Other values
/// run the full update while retaining drawing and target flags. Damage,
/// pulse scaling and animation precede coordinate composition, lighting and
/// regeneration; a killing hit still completes this frame before death dispatch.
static void _generatorTickState(Enemy* enemy, Task* task)
{
    GfxCoord*  rootCoord;
    TmdObject* bodyModel;

    bodyModel = task->extra.tmd;
    rootCoord = bodyModel->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            bodyModel->flags              = 0;
            enemy->node.state.parts.flags = WORLD_TARGET_HIDE_HP;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _generatorUpdateColor(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            bodyModel->flags              = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        default:
            break;
    }
    // Finish the active frame even if damage switches the next dispatch to death.
    _generatorBodyHit(task);
    _generatorPulse(task);
    _generatorUpdateAnimation(task);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _generatorUpdateColor(task);
    _generatorRegenerate(task);
}

/// Restores one body HP every five active updates while Life Support survives.
///
/// Requires initialized body work and Enemy. Healing stops at hpCeiling and
/// reports -1 to the target readout. The signed halfword countdown is retained
/// while healing is disabled; its initial zero makes the first missing HP due.
static void _generatorRegenerate(Task* task)
{
    enum { GENERATOR_REGEN_INTERVAL_FRAMES = 5 };
    GeneratorWork* work;
    Enemy*         enemy;
    s16            regenFrames;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if ((work->lifeSupportDestroyed == 0) && (enemy->hp < work->hpCeiling)) {
        regenFrames      = work->regenTimer - 1;
        work->regenTimer = regenFrames;
        if (regenFrames <= 0) {
            enemy->hp = enemy->hp + 1;
            worldTargetAddReadoutAmount(&enemy->node, -1, 0);
            work->regenTimer = GENERATOR_REGEN_INTERVAL_FRAMES;
        }
    }
}

/// Updates body lighting and color at its root's cached translation.
///
/// Requires a live Enemy/model with writable light and color matrices and a
/// composed root cache. Passes its three cached translation components without
/// refreshing composition. No pointer to the temporary VECTOR is retained.
static void _generatorUpdateColor(Task* task)
{
    GfxCoord* coord;
    VECTOR    samplePosition;

    coord             = task->extra.tmd->coords;
    samplePosition.vx = coord->workm.t[0];
    samplePosition.vy = coord->workm.t[1];
    samplePosition.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &samplePosition, 0, 0);
}

/// Applies the body's animation-set request or advances its current playback.
///
/// Requires initialized GeneratorWork; changed requests are GENERATOR_ANIM_*
/// values 1..3. Initial equal zero requests retain the idle slots started at
/// spawn. A changed request resets animFrames and blends slots 1..9 from frame
/// zero over the set's blend duration; unchanged requests tick those slots and
/// advance the signed halfword frame counter. Slot 0 remains untouched.
static void _generatorUpdateAnimation(Task* task)
{
    _generatorUpdateAnimationInline(task);
}
