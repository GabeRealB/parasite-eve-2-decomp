#include "gameplay/room_effects.h"

/* Part of the Rat library; see rat.h. */

/// Resolves the rat's grid/body contacts, player hits and bite sensor.
///
/// Requires live work, model root, enemy and player actor slots 0/1. Uses all
/// four grid records and three hit records, then clears each consumed table.
/// Grid corrections use Q16 world-unit components; opposed corrections restore
/// the previous root translation. The deepest player/enemy-body overlap moves
/// X/Z along a Q12 direction. Attack keys select player slot bit 7, damage and
/// reactions; fatal hits enter the death task state. A bite contact disables
/// pairing, and the one sensor record latches a player target and attack request.
static void _ratContacts(Task* actor)
{
    enum {
        RAT_CONTACT_KIND_SHIFT    = 16,
        RAT_CONTACT_KIND_NONE     = 0,
        RAT_PLAYER_REACTION_MASK  = 0xFFFF,
        RAT_PLAYER_SLOT_SHIFT     = 7,
        RAT_PLAYER_SLOT_MASK      = 1,
        RAT_GRID_FRACTION_BITS    = 16,
        RAT_CRITICAL_DAMAGE_SCALE = 4
    };
    /// Keeps the deepest positive body overlap and its room-axis Q12 direction.
    ///
    /// `contact` is a stable WorldCollisionContact record evaluated four times;
    /// its point and the cached root translation use the same composition frame.
    /// `distance` is the summed sphere radii, in whole coordinate units. Captures
    /// `rootCoord`, `scratch`, `deepestPush`, `deltaX/Y/Z`, `overlapDepth` and
    /// `nonnegativeDepth`, plus the live `gWorldCollisionActiveGrid` view basis.
    /// The scratch vectors and depth must be writable and disjoint from inputs.
    /// Nonpositive overlaps are clamped to zero; equal depths retain the earlier
    /// direction. All squared-distance arithmetic stays s32. No body is moved
    /// here; the caller applies the selected horizontal push. One statement.
#define RAT_KEEP_DEEPEST_BODY_PUSH(contact)                                                                                     \
    (void)({                                                                                                                    \
        deltaX                   = rootCoord->workm.t[0] - (contact).point.vx;                                                  \
        scratch->delta.vector.vx = deltaX;                                                                                      \
        deltaY                   = rootCoord->workm.t[1] - (contact).point.vy;                                                  \
        scratch->delta.vector.vy = deltaY;                                                                                      \
        deltaZ                   = rootCoord->workm.t[2] - (contact).point.vz;                                                  \
        scratch->delta.vector.vz = deltaZ;                                                                                      \
        overlapDepth             = (contact).distance - SquareRoot0((deltaX * deltaX) + (deltaY * deltaY) + (deltaZ * deltaZ)); \
        nonnegativeDepth         = overlapDepth;                                                                                \
        if (overlapDepth <= 0) {                                                                                                \
            nonnegativeDepth = 0;                                                                                               \
        }                                                                                                                       \
        overlapDepth = nonnegativeDepth;                                                                                        \
        if (deepestPush < overlapDepth) {                                                                                       \
            deepestPush = overlapDepth;                                                                                         \
            VectorNormal(&scratch->delta.vector, &scratch->normal);                                                             \
            ApplyTransposeMatrixLV(&gWorldCollisionActiveGrid->viewCoord->workm, &scratch->normal, &scratch->pushDirection);    \
        }                                                                                                                       \
    })

    RatWork*                 work;
    ActorOverlapPushScratch* scratch;
    Enemy*                   enemy;
    GfxCoord*                rootCoord;
    GfxCoord*                playerCoord;
    WorldCollisionContact*   attackContacts;
    WorldCollisionContact*   sensorContacts;
    s32                      deepestPush;
    s32                      gridResult;
    s32                      contactIndex;
    s32                      overlapDepth;
    s32                      deltaX;
    s32                      deltaY;
    s32                      deltaZ;
    s32                      nonnegativeDepth;
    s32                      hitCooldownFrames;
    u32                      lastEffectKey;
    u32                      contactKey;
    u32                      playerSlotBits;
    u32                      effectKey;
    u32                      hitDamage;

    deepestPush   = 0;
    lastEffectKey = 0;
    work          = actor->work;
    SCRATCH_STACK_RESERVE_BLOCK(ActorOverlapPushScratch);
    scratch   = SCRATCH_STACK_CURSOR(ActorOverlapPushScratch);
    rootCoord = actor->extra.tmd->coords;
    enemy     = actor->spawnArg2.pointer;
    // Apply grid correction before hit damage and body-overlap selection.
    gridResult = worldCollisionResolvePushback(work->gridContacts, &scratch->delta, ARRAY_SIZE(work->gridContacts), NULL);
    switch (gridResult) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += scratch->delta.fixed.vx.word >> RAT_GRID_FRACTION_BITS;
            rootCoord->coord.t[1] += scratch->delta.fixed.vy.word >> RAT_GRID_FRACTION_BITS;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.word >> RAT_GRID_FRACTION_BITS;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevPos.vx;
            rootCoord->coord.t[1] = work->prevPos.vy;
            rootCoord->coord.t[2] = work->prevPos.vz;
            break;
    }
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hitContacts); contactIndex++) {
        contactKey = work->hitContacts[contactIndex].key.value;
        switch (contactKey >> RAT_CONTACT_KIND_SHIFT) {
            case RAT_CONTACT_KIND_NONE:
                break;
            case WORLD_COLLISION_CONTACT_ATTACK >> RAT_CONTACT_KIND_SHIFT:
                if (work->hitCooldown == 0) {
                    playerSlotBits           = contactKey >> RAT_PLAYER_SLOT_SHIFT;
                    playerCoord              = gPlayerActorTasks[playerSlotBits & RAT_PLAYER_SLOT_MASK]->extra.tmd->coords;
                    scratch->delta.vector.vx = playerCoord->coord.t[0] - rootCoord->coord.t[0];
                    scratch->delta.vector.vy = playerCoord->coord.t[1] - rootCoord->coord.t[1];
                    scratch->delta.vector.vz = playerCoord->coord.t[2] - rootCoord->coord.t[2];
                    hitDamage                = damageComputePlayerAttack(work->hitContacts[contactIndex].key.value, SquareRoot0((scratch->delta.vector.vx * scratch->delta.vector.vx) + (scratch->delta.vector.vy * scratch->delta.vector.vy) + (scratch->delta.vector.vz * scratch->delta.vector.vz)), 0, 0);
                    if (damageRollCriticalHit(actor->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, 0) != 0) {
                        hitDamage *= RAT_CRITICAL_DAMAGE_SCALE;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords, 0, NULL);
                    }
                    worldTargetAddReadoutAmount(&((Enemy*)actor->spawnArg2.pointer)->node, hitDamage, 0);
                    damageAccumulateLifeDrainHp(actor->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, hitDamage, 0);
                    enemy->hp -= hitDamage;
                    if (enemy->hp <= 0) {
                        work->mode   = RAT_MODE_DEAD;
                        work->step   = 0;
                        actor->state = RAT_TASK_DEATH;
                    } else if (work->buildupHeld == 0) {
                        work->mode = RAT_MODE_HURT;
                        work->step = 0;
                    }
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    switch (damageGetPlayerAttackReaction(work->hitContacts[contactIndex].key.value) & RAT_PLAYER_REACTION_MASK) {
                        case DAMAGE_PLAYER_REACTION_NONE:
                        case 4:
                        case 5:
                        case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        case 8:
                            break;
                        case DAMAGE_PLAYER_REACTION_BUILDUP:
                            damageStartEnemyBuildup(actor->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_POISON:
                            damageTryStartEnemyDamageOverTime(actor->spawnArg2.pointer, work->hitContacts[contactIndex].key.value, 0);
                            break;
                        case DAMAGE_PLAYER_REACTION_STAGGER:
                        case 9:
                            damageStartEnemyStagger(actor->spawnArg2.pointer);
                            break;
                    }
                    effectKey = work->hitContacts[contactIndex].key.value;
                    if (lastEffectKey != effectKey) {
                        lastEffectKey = effectKey;
                        effectSpawnHit(damageGetPlayerAttackEffectId(effectKey), rootCoord, NULL, &work->hitEffectArg);
                    }
                    hitCooldownFrames = damageGetPlayerAttackHitCooldown(work->hitContacts[contactIndex].key.value);
                    if (hitCooldownFrames > 0) {
                        work->hitCooldown = hitCooldownFrames;
                    }
                }
                break;
            case WORLD_COLLISION_CONTACT_PLAYER_BODY >> RAT_CONTACT_KIND_SHIFT:
                RAT_KEEP_DEEPEST_BODY_PUSH(work->hitContacts[contactIndex]);
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY >> RAT_CONTACT_KIND_SHIFT:
                RAT_KEEP_DEEPEST_BODY_PUSH(work->hitContacts[contactIndex]);
                break;
        }
    }
    if (deepestPush > 0) {
        rootCoord->coord.t[0] += (deepestPush * scratch->pushDirection.vx) >> RAT_DIRECTION_FRACTION_BITS;
        rootCoord->coord.t[2] += (deepestPush * scratch->pushDirection.vz) >> RAT_DIRECTION_FRACTION_BITS;
    }
    worldCollisionClearContacts(work->hitContacts);
    // The bite stops at its first contact; the sensor latches the next target.
    attackContacts = work->attackContacts;
    if (worldCollisionFindContactIndex(attackContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(attackContacts);
    }
    sensorContacts = work->sensorContacts;
    if (worldCollisionCountContactsByKind(sensorContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        playerCoord             = gPlayerActorTasks[(u8)work->sensorContacts[0].key.parts.id >> RAT_PLAYER_SLOT_SHIFT]->extra.tmd->coords;
        work->attackRequested   = 1;
        work->sensorBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->targetCoord       = playerCoord;
    }
    worldCollisionClearContacts(sensorContacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorOverlapPushScratch);
#undef RAT_KEEP_DEEPEST_BODY_PUSH
}

/// Chooses the next wander-heading offset and its stored update countdown.
///
/// Borrows writable work; consumes two LCG draws in delay/heading order.
/// The delay draw stores 0..31; idle decrements before testing, so 0 and 1
/// both schedule another draw on the next update.
/// The heading draw supplies a magnitude 0..1023 and sign bit; target yaw
/// wraps to 0..4095 around the rat's current heading.
static inline void _ratPickWanderHeading(RatWork* work)
{
    enum {
        RAT_WANDER_DELAY_MASK         = 31,
        RAT_WANDER_YAW_MAGNITUDE_MASK = ACTOR_TRANSFORM_ANGLE_TURN / 4 - 1,
        RAT_WANDER_YAW_SIGN_BIT       = ACTOR_TRANSFORM_ANGLE_TURN / 4
    };
    s32 wanderDelayRandom;
    s32 wanderYawRandom;
    s32 wanderYawOffset;

    work->turnRate    = RAT_WANDER_TURN_RATE;
    wanderDelayRandom = RANDOM_LCG_NEXT(gRandomLcgState);
    wanderYawRandom   = RANDOM_LCG_NEXT(wanderDelayRandom);
    wanderYawOffset   = ((u32)wanderYawRandom >> 16) & RAT_WANDER_YAW_MAGNITUDE_MASK;
    gRandomLcgState   = wanderDelayRandom;
    work->wanderTimer = ((u32)wanderDelayRandom >> 16) & RAT_WANDER_DELAY_MASK;
    gRandomLcgState   = wanderYawRandom;
    if ((((u32)wanderYawRandom >> 16) & RAT_WANDER_YAW_SIGN_BIT) == 0) {
        wanderYawOffset = -wanderYawOffset;
    }
    work->targetYaw = ((u16)work->yaw + wanderYawOffset) & ACTOR_TRANSFORM_ANGLE_MASK;
}

/// Wanders in timed walk/run bursts until an attack is requested.
///
/// Requires live work/model and an `Enemy` whose placement row indexes the
/// carrier's chance tables. Duration draws index 0..15. Heading changes stay
/// within 1023 angle units of the current yaw; timer values count update calls.
/// An attack request starts a 60..91-frame approach and still ticks idle audio
/// on that transition frame.
static void _ratIdle(Task* actor)
{
    enum {
        RAT_IDLE_MOVE_ROLL_FRAMES     = 30,
        RAT_IDLE_APPROACH_BASE_FRAMES = 60,
    };

    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    s32        slowChanceRandom;
    s32        slowDurationRandom;
    s32        fastChanceRandom;
    s32        fastDurationRandom;
    s32        approachDurationRandom;
    s32        moveFrames;
    s32        soundId;
    s32        audioPan;

    work      = actor->work;
    model     = actor->extra.tmd;
    rootCoord = model->coords;
    switch (work->step) {
        case RAT_IDLE_STEP_REST:
            work->forwardSpeed      = 0;
            work->sensorBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->timer++;
            if (work->timer < RAT_IDLE_MOVE_ROLL_FRAMES) {
                break;
            }
            slowChanceRandom = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState  = slowChanceRandom;
            if ((s32)(((u32)slowChanceRandom >> 16) & (RAT_CHANCE_ROLL_COUNT - 1)) <
                _gRatWalkChance[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex]) {
                work->animId    = RAT_ANIM_WALK;
                moveFrames      = _gRatWalkFrames[((u32)(slowDurationRandom = RANDOM_LCG_NEXT(slowChanceRandom)) >> 16) & (RAT_MOVE_DURATION_COUNT - 1)];
                gRandomLcgState = slowDurationRandom;
                work->step      = RAT_IDLE_STEP_WALK;
                work->timer     = moveFrames;
                break;
            }
            fastChanceRandom = RANDOM_LCG_NEXT(slowChanceRandom);
            gRandomLcgState  = fastChanceRandom;
            if ((s32)(((u32)fastChanceRandom >> 16) & (RAT_CHANCE_ROLL_COUNT - 1)) <
                _gRatRunChance[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex]) {
                work->animId    = RAT_ANIM_RUN;
                moveFrames      = _gRatRunFrames[((u32)(fastDurationRandom = RANDOM_LCG_NEXT(fastChanceRandom)) >> 16) & (RAT_MOVE_DURATION_COUNT - 1)];
                gRandomLcgState = fastDurationRandom;
                work->step      = RAT_IDLE_STEP_RUN;
                work->timer     = moveFrames;
                break;
            }
            work->timer = 0;
            break;
        case RAT_IDLE_STEP_WALK:
            work->forwardSpeed = RAT_WALK_SPEED;
            work->timer--;
            if (work->timer > 0) {
                break;
            }
            work->animId = RAT_ANIM_IDLE;
            work->timer  = 0;
            work->step   = RAT_IDLE_STEP_REST;
            break;
        case RAT_IDLE_STEP_RUN:
            work->forwardSpeed = RAT_RUN_SPEED;
            work->timer--;
            if (work->timer > 0) {
                break;
            }
            work->animId = RAT_ANIM_IDLE;
            work->timer  = 0;
            work->step   = RAT_IDLE_STEP_REST;
            break;
    }
    work->wanderTimer--;
    if (work->wanderTimer <= 0) {
        _ratPickWanderHeading(work);
    }
    // Attack entry still falls through to this update's idle-audio tick.
    if (work->attackRequested != 0) {
        work->mode             = RAT_MODE_ATTACK;
        work->attackRequested  = 0;
        work->step             = RAT_ATTACK_STEP_APPROACH;
        work->animId           = RAT_ANIM_RUN;
        approachDurationRandom = RANDOM_LCG_NEXT(gRandomLcgState);
        work->timer            = (((u32)approachDurationRandom >> 16) & 0x1F) + RAT_IDLE_APPROACH_BASE_FRAMES;
        soundId                = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << RAT_SOUND_PLACE_INDEX_SHIFT) | RAT_SOUND_ALERT;
        gRandomLcgState        = approachDurationRandom;
        audioPan               = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
    _ratIdleSound(actor);
}

/// Returns the rat's aim error with a single-turn wrap adjustment.
///
/// `targetYaw` is in 0..4095; `currentYaw` is the u16 encoding of the stored
/// heading, including a turn step just outside that range. Angles use 4096
/// units per turn. The low signed halfword chooses the arc, while the wrapped
/// arc uses the full subtraction before narrowing the result. Normalized
/// inputs give 0..2048; a heading just outside the range can give a negative
/// error when the target changes across the wrap. That retained value still
/// passes the caller's one-sided aim test. No work or coordinate is changed.
static inline s16 _ratGetAimError(u16 targetYaw, u16 currentYaw)
{
    s32 rawYawDelta;
    s16 yawDelta;
    s32 absYawDelta;
    s32 aimError;

    rawYawDelta = targetYaw - currentYaw;
    yawDelta    = rawYawDelta;
    absYawDelta = yawDelta >= 0 ? yawDelta : -yawDelta;
    if (absYawDelta < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        aimError = absYawDelta;
    } else if (yawDelta > 0) {
        aimError = ACTOR_TRANSFORM_ANGLE_TURN - rawYawDelta;
    } else {
        aimError = rawYawDelta + ACTOR_TRANSFORM_ANGLE_TURN;
    }
    return aimError;
}

/// Approaches the selected target, bites, then retreats and rolls a repeat.
///
/// Requires live work/model and an `Enemy` in `Task::spawnArg2.pointer`; a missing target borrows
/// the live player's root; an existing target must remain live. Positions share
/// the roots' parent frame; heading uses signed low-halfword X/Z offsets, while
/// range uses the full offsets. Yaw uses 4096 units per turn. The attack sphere pairs only during hit frames
/// until contact or the animation ends. The approach timeout is handled before
/// range/aim checks, so those checks still run on its expiry frame.
static void _ratAttack(Task* actor)
{
    enum {
        RAT_ATTACK_REACH           = 700,
        RAT_ATTACK_AIM_TOLERANCE   = 50,
        RAT_ATTACK_HIT_START_FRAME = 20,
        RAT_ATTACK_HIT_END_FRAME   = 32,
        RAT_ATTACK_BACK_OFF_FRAMES = 11,
        RAT_ATTACK_BACK_OFF_SPEED  = 120,
        RAT_ATTACK_RECOVER_FRAME   = 31,
    };

    VECTOR*    targetOffset;
    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    GfxCoord*  targetCoord;
    s32        distance;
    s32        backOffSpeed;
    s32        audioPan;
    s32        soundId;

    targetOffset = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work         = actor->work;
    model        = actor->extra.tmd;
    rootCoord    = model->coords;
    switch (work->step) {
        case RAT_ATTACK_STEP_APPROACH:
            sceneEngageBattle(1);
            if (work->targetCoord == 0) {
                work->targetCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
            }
            targetCoord      = work->targetCoord;
            targetOffset->vx = targetCoord->coord.t[0] - rootCoord->coord.t[0];
            targetOffset->vy = 0;
            targetOffset->vz = targetCoord->coord.t[2] - rootCoord->coord.t[2];
            work->targetYaw  = ratan2((s16)targetOffset->vx, (s16)targetOffset->vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            work->turnRate   = RAT_WANDER_TURN_RATE;
            // Expiry resets idle state before the same frame still tests range and aim.
            work->timer--;
            if (work->timer <= 0) {
                work->mode   = RAT_MODE_IDLE;
                work->step   = RAT_IDLE_STEP_REST;
                work->animId = RAT_ANIM_IDLE;
                work->timer  = 0;
            }
            distance = SquareRoot0(targetOffset->vx * targetOffset->vx + targetOffset->vz * targetOffset->vz);
            if (distance < RAT_ATTACK_REACH) {
                if (_ratGetAimError(work->targetYaw, (u16)work->yaw) < RAT_ATTACK_AIM_TOLERANCE) {
                    work->animId       = RAT_ANIM_ATTACK;
                    work->forwardSpeed = 0;
                    work->turnRate     = 0;
                    work->step         = RAT_ATTACK_STEP_BITE;
                } else {
                    work->forwardSpeed = 0;
                }
            } else {
                work->forwardSpeed = RAT_RUN_SPEED;
            }
            break;

        case RAT_ATTACK_STEP_BITE:
            if (work->animFrame == RAT_ATTACK_HIT_START_FRAME) {
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrame >= RAT_ATTACK_HIT_END_FRAME) {
                work->animId            = RAT_ANIM_BACK_OFF;
                work->step              = RAT_ATTACK_STEP_RETREAT;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;

        case RAT_ATTACK_STEP_RETREAT:
            backOffSpeed = 0;
            if (work->animFrame < RAT_ATTACK_BACK_OFF_FRAMES) {
                backOffSpeed = -RAT_ATTACK_BACK_OFF_SPEED;
            }
            work->forwardSpeed = backOffSpeed;
            if (work->animFrame >= RAT_ATTACK_RECOVER_FRAME) {
                soundId  = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << RAT_SOUND_PLACE_INDEX_SHIFT) | RAT_SOUND_ATTACK_RECOVER;
                audioPan = (s8)worldCoordGetOriginAudioPan(rootCoord);
                sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
                RANDOM_LCG_ADVANCE();
                if ((s32)(RANDOM_LCG_DRAW() & (RAT_CHANCE_ROLL_COUNT - 1)) < _gRatAttackRepeatChance[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex]) {
                    work->step   = RAT_ATTACK_STEP_APPROACH;
                    work->animId = RAT_ANIM_RUN;
                } else {
                    work->mode            = RAT_MODE_IDLE;
                    work->step            = RAT_IDLE_STEP_REST;
                    work->animId          = RAT_ANIM_IDLE;
                    work->timer           = 0;
                    work->wanderTimer     = 0;
                    work->attackRequested = 0;
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Pushes the rat away from the player, spins it, then holds or recovers.
///
/// Requires live work/model, the live player matrix in the same parent frame,
/// and an `Enemy` in `Task::spawnArg2.pointer`. The normalized direction includes Y, but only
/// X/Z move: 50 units times that Q12 direction for the first 15 animation
/// frames. Build-up enters its hold directly; ordinary recovery requests an
/// attack after frame 32.
static void _ratStagger(Task* actor)
{
    enum {
        RAT_STAGGER_PUSH_FRAMES      = 15,
        RAT_STAGGER_PUSH_SPEED       = 50,
        RAT_STAGGER_SPIN_START_FRAME = 6,
        RAT_STAGGER_SPIN_TURN_RATE   = 147,
        RAT_STAGGER_SPIN_YAW_STEP    = 1479,
        RAT_STAGGER_RECOVER_FRAME    = 32,
    };

    VECTOR     toPlayer;
    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    s32        durationRandom;
    s32        rootX;

    work      = actor->work;
    model     = actor->extra.tmd;
    rootCoord = model->coords;
    switch (work->step) {
        case RAT_STAGGER_STEP_BEGIN:
            work->animId        = RAT_ANIM_STAGGER;
            work->appliedAnimId = RAT_ANIM_IDLE;
            work->forwardSpeed  = 0;
            work->turnRate      = 0;
            work->knockedDown   = 1;
            work->step          = RAT_STAGGER_STEP_KNOCKBACK;
            durationRandom      = RANDOM_LCG_NEXT(gRandomLcgState);
            work->timer         = (((u32)durationRandom >> 16) & 0x1F) + RAT_STAGGER_PUSH_FRAMES;
            gRandomLcgState     = durationRandom;
            // Capture the full 3D direction; knockback later applies only X/Z.
            rootX       = rootCoord->coord.t[0];
            toPlayer.vx = gPlayerStatus.coordMtx->t[0] - rootX;
            toPlayer.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
            toPlayer.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            VectorNormalS(&toPlayer, &work->staggerDir);
            break;
        case RAT_STAGGER_STEP_KNOCKBACK:
            if (work->animFrame < RAT_STAGGER_PUSH_FRAMES) {
                rootCoord->coord.t[0] += -(work->staggerDir.vx * RAT_STAGGER_PUSH_SPEED) >> RAT_DIRECTION_FRACTION_BITS;
                rootCoord->coord.t[2] += -(work->staggerDir.vz * RAT_STAGGER_PUSH_SPEED) >> RAT_DIRECTION_FRACTION_BITS;
            }
            if (work->animFrame >= RAT_STAGGER_SPIN_START_FRAME && work->animFrame < RAT_STAGGER_PUSH_FRAMES) {
                work->turnRate  = RAT_STAGGER_SPIN_TURN_RATE;
                work->targetYaw = (work->targetYaw + RAT_STAGGER_SPIN_YAW_STEP) & ACTOR_TRANSFORM_ANGLE_MASK;
            } else {
                work->turnRate = 0;
            }
            work->timer--;
            if (work->timer > 0) {
                break;
            }
            if ((((Enemy*)actor->spawnArg2.pointer)->reactionFlags & ENEMY_REACTION_BUILDUP) != 0) {
                work->animId = RAT_ANIM_BUILDUP_HOLD;
                work->mode   = RAT_MODE_BUILDUP;
                work->step   = RAT_BUILDUP_STEP_HOLD;
                break;
            }
            work->animId = RAT_ANIM_STAGGER_RECOVER;
            work->step   = RAT_STAGGER_STEP_RECOVER;
            break;
        case RAT_STAGGER_STEP_RECOVER:
            if (work->animFrame < RAT_STAGGER_RECOVER_FRAME) {
                break;
            }
            work->mode            = RAT_MODE_IDLE;
            work->step            = RAT_IDLE_STEP_REST;
            work->animId          = RAT_ANIM_IDLE;
            work->timer           = 0;
            work->attackRequested = 1;
            work->knockedDown     = 0;
            break;
    }
}

/// Holds the rat down until its build-up reaction finishes.
///
/// Requires live work/model and an `Enemy` in `Task::spawnArg2.pointer`. A rat already knocked
/// down skips the collapse; both paths load a 0..15-frame count before holding.
/// Completion clears the build-up latch and requests an attack from idle.
static void _ratBuildup(Task* actor)
{
    enum {
        RAT_BUILDUP_COLLAPSE_END_FRAME = 29,
    };

    Enemy*   enemy;
    RatWork* work;
    s32      randomDelay;
    s32      randomDelayAfterCollapse;

    work = actor->work;
    switch (work->step) {
        case RAT_BUILDUP_STEP_BEGIN:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (work->knockedDown == 0) {
                work->step   = RAT_BUILDUP_STEP_COLLAPSE;
                work->animId = RAT_ANIM_COLLAPSE;
            } else {
                work->step      = RAT_BUILDUP_STEP_WAIT;
                randomDelay     = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState = randomDelay;
                work->timer     = ((u32)randomDelay >> 0x10) & 0xF;
            }
            work->knockedDown   = 1;
            work->appliedAnimId = RAT_ANIM_IDLE;
            return;
        case RAT_BUILDUP_STEP_COLLAPSE:
            if (work->animFrame >= RAT_BUILDUP_COLLAPSE_END_FRAME) {
                work->step               = RAT_BUILDUP_STEP_WAIT;
                randomDelayAfterCollapse = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState          = randomDelayAfterCollapse;
                work->timer              = ((u32)randomDelayAfterCollapse >> 0x10) & 0xF;
                return;
            }
            return;
        case RAT_BUILDUP_STEP_WAIT:
            work->timer--;
            if (work->timer <= 0) {
                work->animId = RAT_ANIM_BUILDUP_HOLD;
                work->step   = RAT_BUILDUP_STEP_HOLD;
                return;
            }
            break;
        case RAT_BUILDUP_STEP_HOLD:
            if (damageTickEnemyBuildup(actor->spawnArg2.pointer) != 0) {
                enemy                 = actor->spawnArg2.pointer;
                enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
                work->mode            = RAT_MODE_IDLE;
                work->step            = RAT_IDLE_STEP_REST;
                work->animId          = RAT_ANIM_IDLE;
                work->timer           = 0;
                work->attackRequested = 1;
                work->knockedDown     = 0;
                work->buildupHeld     = 0;
            }
            break;
    }
}

/// Turns the root toward its target heading and rebuilds a yaw-only rotation.
///
/// Requires live work/model, a target yaw in 0..4095 and a nonnegative turn
/// rate in angle units per update. The current heading is read from the local
/// matrix; the shortest arc is used and a step reaching the target snaps to it.
/// Translation is retained; pitch, roll and prior scale are replaced. The
/// caller owns invalidation/composition. Releases `ActorFaceScratch` on return.
static void _ratTurn(Task* actor)
{
    RatWork*          work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s32               currentYaw;
    u16               targetYaw;
    s16               yawDelta;
    s32               absYawDelta;
    s32               turnRate;
    s32               wrappedYaw;
    s32               nextYaw;
    s32               wrappedTurnRate;

    scratch     = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    rootCoord   = actor->extra.tmd->coords;
    work        = actor->work;
    currentYaw  = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw   = work->targetYaw;
    yawDelta    = targetYaw - currentYaw;
    absYawDelta = yawDelta >= 0 ? yawDelta : -yawDelta;

    work->yaw = currentYaw;
    if (absYawDelta < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnRate = work->turnRate;
        if (turnRate >= absYawDelta) {
            work->yaw = targetYaw;
        } else {
            nextYaw = work->yaw;
            if (yawDelta <= 0) {
                nextYaw -= turnRate;
            } else {
                nextYaw += turnRate;
            }
            work->yaw = nextYaw;
        }
    } else {
        turnRate = work->turnRate;
        if (yawDelta > 0 ? turnRate >= ACTOR_TRANSFORM_ANGLE_TURN - yawDelta : turnRate >= ACTOR_TRANSFORM_ANGLE_TURN + yawDelta) {
            work->yaw = work->targetYaw;
        } else {
            wrappedTurnRate = work->turnRate;
            wrappedYaw      = work->yaw;
            if (yawDelta > 0) {
                work->yaw = wrappedYaw - wrappedTurnRate;
            } else {
                work->yaw = wrappedYaw + wrappedTurnRate;
            }
        }
    }
    // Rebuild from yaw alone, retaining translation and replacing pitch/roll/scale.
    scratch->rot.vx = 0;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

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
    /// Advances the corpse's animation request and samples lighting at its cached root.
    ///
    /// Captures the live `actor` TMD task and overwrites `animationWork`,
    /// `animationSlot`, `blendFrames`, `colorCoord` and `colorPosition` locals.
    /// Requires a RatWork rig with slots 1..6 and a valid `animId` indexing the
    /// carrier's `_gRatAnimBlendFrames` and clip table. A changed request resets
    /// the s16 frame count and blends from frame zero with an s16 duration;
    /// otherwise it increments the count and ticks each slot. Slot 0 is skipped.
    /// Lighting uses full-width cached XYZ after playback, without composing or
    /// changing frame. The task's spawnArg2 must borrow the live Enemy to colour.
    /// Expands to one statement; no pointer to temporary storage is retained.
#define RAT_ANIMATE_AND_LIGHT_CORPSE()                                                                                      \
    (void)({                                                                                                                \
        animationWork = actor->work;                                                                                        \
        if (animationWork->animId != animationWork->appliedAnimId) {                                                        \
            animationWork->appliedAnimId = animationWork->animId;                                                           \
            animationWork->animFrame     = 0;                                                                               \
            blendFrames                  = _gRatAnimBlendFrames[animationWork->animId];                                     \
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
    })

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

/// Dispatches the rat task's spawn, update or death handler.
///
/// Requires an `Enemy` in `Task::spawnArg2.pointer` and a task state in 0..2. Copies the carrier's
/// three-entry callback table by value before dispatch; there is no bounds check.
static void _ratTask(Task* actor)
{
    EnemyTaskFuncTable3 handlers;

    handlers = _gRatStateHandlers;
    handlers.funcs[actor->state](actor->spawnArg2.pointer, actor);
}

/// Runs one living-rat update and refreshes its composed root lighting/shadow.
///
/// Requires a live enemy/model and initialized work in update task state 1.
/// Running updates enable drawing/targeting, consume reactions and contacts,
/// dispatch behavior, turn, step and animate before composing the root. Paused
/// updates only sample cached lighting and draw the shadow; hidden updates
/// disable drawing and targeting. Other control values retain the model/target
/// flags and run the update. A fatal contact schedules the death task state.
static void _ratUpdate(Enemy* enemy, Task* actor)
{
    GfxCoord*  rootCoord;
    TmdObject* model;
    RatWork*   work;

    model     = actor->extra.tmd;
    rootCoord = model->coords;
    work      = actor->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                  = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _ratUpdateColor(actor);
            _ratShadow(actor);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Contacts can select the next mode before behavior and movement run.
    if (enemy->reactionFlags != 0) {
        _ratReactions(actor);
    }
    _ratContacts(actor);
    _ratBehavior(actor);
    if (work->turnRate != 0) {
        _ratTurn(actor);
    }
    _ratStep(actor);
    _ratAnimate(actor);
    // Refresh the root cache after movement and animation, before lighting.
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _ratUpdateColor(actor);
    _ratShadow(actor);
}

/// Applies stagger, build-up and damage-over-time reactions to the rat.
///
/// Requires live work and an `Enemy` in `Task::spawnArg2.pointer`. Stagger is consumed before
/// build-up is considered; damage can override either with hurt or death.
/// Health subtraction retains its low 16 bits and tests them as signed.
static void _ratReactions(Task* actor)
{
    Enemy*   enemy;
    RatWork* work;
    s32      damage;
    u16      remainingHp;
    u8       reactionFlags;

    enemy         = actor->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = actor->work;
    if (reactionFlags & ENEMY_REACTION_STAGGER) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_STAGGER_CLEAR;
        work->mode           = RAT_MODE_STAGGER;
        work->step           = RAT_STAGGER_STEP_BEGIN;
    }
    if ((enemy->reactionFlags & ENEMY_REACTION_BUILDUP) && (work->mode != RAT_MODE_STAGGER && work->mode != RAT_MODE_BUILDUP)) {
        work->mode        = RAT_MODE_BUILDUP;
        work->step        = RAT_BUILDUP_STEP_BEGIN;
        work->buildupHeld = 1;
    }
    if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
        damage = damageTickEnemyDamageOverTime(enemy);
        if (damage != 0) {
            worldTargetAddReadoutAmount(&enemy->node, damage, 0);
            remainingHp = enemy->hp - damage;
            enemy->hp   = remainingHp;
            if ((s16)remainingHp <= 0) {
                work->mode   = RAT_MODE_DEAD;
                work->step   = 0;
                actor->state = RAT_TASK_DEATH;
            } else {
                work->mode = RAT_MODE_HURT;
                work->step = RAT_HURT_STEP_BEGIN;
            }
        }
        if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
            enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Runs the rat's current living behavior mode.
///
/// Requires live `RatWork`. Dead and unrecognized modes do nothing; task-state
/// dispatch owns the death sequence.
static void _ratBehavior(Task* actor)
{
    RatWork* work;

    work = actor->work;
    switch (work->mode) {
        case RAT_MODE_IDLE:
            _ratIdle(actor);
            break;
        case RAT_MODE_ATTACK:
            _ratAttack(actor);
            break;
        case RAT_MODE_STAGGER:
            _ratStagger(actor);
            break;
        case RAT_MODE_BUILDUP:
            _ratBuildup(actor);
            break;
        case RAT_MODE_HURT:
            _ratHurt(actor);
            break;
        case RAT_MODE_DEAD:
            break;
    }
}

/// Counts down the idle audio delay and queues the next spatial sound.
///
/// Requires live work/model and an `Enemy` in `Task::spawnArg2.pointer`. On expiry the delay
/// becomes 150..277 update calls. Placement index selects the script instance;
/// pan and depth retain only their signed low bytes.
static void _ratIdleSound(Task* actor)
{
    enum {
        RAT_IDLE_SOUND_BASE_FRAMES = 150,
    };

    RatWork*  work;
    GfxCoord* rootCoord;
    s32       soundId;
    s32       audioPan;
    u32       delayRandom;

    work      = actor->work;
    rootCoord = actor->extra.tmd->coords;
    work->idleSoundTimer--;
    if (work->idleSoundTimer <= 0) {
        delayRandom          = RANDOM_LCG_NEXT(gRandomLcgState);
        work->idleSoundTimer = ((delayRandom >> 0x10) & 0x7F) + RAT_IDLE_SOUND_BASE_FRAMES;
        gRandomLcgState      = delayRandom;
        soundId              = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << RAT_SOUND_PLACE_INDEX_SHIFT) | RAT_SOUND_IDLE;
        audioPan             = (s8)worldCoordGetOriginAudioPan(rootCoord);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
    }
}

/// Plays a stationary hit flinch, then returns to idle with an attack pending.
///
/// Requires live work/model and an `Enemy` in `Task::spawnArg2.pointer`. Entry restarts the hurt
/// clip even after another flinch; recovery begins at animation frame 24.
static void _ratHurt(Task* actor)
{
    enum {
        RAT_HURT_RECOVER_FRAME = 24,
    };

    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    s32        soundId;
    s32        audioPan;

    work      = actor->work;
    model     = actor->extra.tmd;
    rootCoord = model->coords;
    switch (work->step) {
        case RAT_HURT_STEP_BEGIN:
            work->animId        = RAT_ANIM_HURT;
            work->appliedAnimId = RAT_ANIM_IDLE;
            work->forwardSpeed  = 0;
            work->turnRate      = 0;
            work->step          = RAT_HURT_STEP_RECOVER;
            soundId             = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << RAT_SOUND_PLACE_INDEX_SHIFT) | RAT_SOUND_HURT;
            audioPan            = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            break;
        case RAT_HURT_STEP_RECOVER:
            if (work->animFrame < RAT_HURT_RECOVER_FRAME) {
                break;
            }
            work->mode            = RAT_MODE_IDLE;
            work->step            = RAT_IDLE_STEP_REST;
            work->animId          = RAT_ANIM_IDLE;
            work->timer           = 0;
            work->attackRequested = 1;
            break;
    }
}

/// Saves the root position and applies this frame's horizontal and vertical step.
///
/// Requires live work/model. Forward speed is in parent-coordinate units per
/// update; the root's third rotation column has 12 fractional bits. X/Z use
/// arithmetic right shifts, and local Y increases by 128. Grid-contact recovery
/// can restore the saved position on the following contact pass; the caller
/// owns coordinate invalidation and composition.
static void _ratStep(Task* actor)
{
    enum {
        RAT_STEP_VERTICAL_SPEED = 128,
    };

    RatWork*  work;
    GfxCoord* rootCoord;

    rootCoord              = actor->extra.tmd->coords;
    work                   = actor->work;
    work->prevPos.vx       = rootCoord->coord.t[0];
    work->prevPos.vy       = rootCoord->coord.t[1];
    work->prevPos.vz       = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> RAT_DIRECTION_FRACTION_BITS;
    rootCoord->coord.t[1] += RAT_STEP_VERTICAL_SPEED;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> RAT_DIRECTION_FRACTION_BITS;
}

/// Restarts or advances the rat's six non-root animation slots.
///
/// Requires live `RatWork` and animation/blend entries for the requested
/// `RAT_ANIM_*` value. A changed request starts slots 1..6 at frame zero with
/// the table's whole-frame blend duration; an unchanged request advances
/// the slots and the signed 16-bit elapsed-frame counter once.
static void _ratAnimate(Task* actor)
{
    RatWork* work;
    s32      slot;
    s32      blendFrames;

    work = actor->work;
    if (work->animId != work->appliedAnimId) {
        work->appliedAnimId = work->animId;
        work->animFrame     = 0;
        blendFrames         = _gRatAnimBlendFrames[work->animId];
        for (slot = 1; slot < ARRAY_SIZE(work->rig.slots); slot++) {
            animationSeekSlotWithBlend(&work->rig.anim, slot, work->animId, 0, blendFrames);
        }
    } else {
        work->animFrame++;
        for (slot = 1; slot < ARRAY_SIZE(work->rig.slots); slot++) {
            animationTickSlot(&work->rig.anim, slot);
        }
    }
}

/// Updates the rat's lighting and color from its cached root translation.
///
/// Requires a live model/`Enemy` with writable light/color matrices and a cache
/// suitable for the lighting query. Only XYZ are supplied; this function does
/// not compose the root or retain the temporary sample.
static void _ratUpdateColor(Task* actor)
{
    GfxCoord* rootCoord;
    VECTOR3   samplePosition;

    rootCoord         = actor->extra.tmd->coords;
    samplePosition.vx = rootCoord->workm.t[0];
    samplePosition.vy = rootCoord->workm.t[1];
    samplePosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(actor->spawnArg2.pointer, &samplePosition, 0, 0);
}

/// Draws the rat's ground shadow from its cached root translation.
///
/// Requires a live model with a cache suitable for the ground-shadow query.
/// The square has a half-side of 448 coordinate units and shade 128; this
/// function does not refresh the coordinate cache.
static void _ratShadow(Task* actor)
{
    enum {
        RAT_SHADOW_HALF_SIZE = 448,
        RAT_SHADOW_SHADE     = 128,
    };

    GfxCoord* rootCoord;
    VECTOR3   shadowCentre;

    rootCoord       = actor->extra.tmd->coords;
    shadowCentre.vx = rootCoord->workm.t[0];
    shadowCentre.vy = rootCoord->workm.t[1];
    shadowCentre.vz = rootCoord->workm.t[2];
    effectDrawGroundShadow(&shadowCentre, RAT_SHADOW_HALF_SIZE, RAT_SHADOW_SHADE);
}

/// Restores the saved death pose and applies its stored Q12 local Y scale.
///
/// Work/root and reserved scratch must be live and disjoint. The saved matrix
/// is the pose captured at death entry; `squashScale` is signed Q12 (4096 = 1).
/// Restoring it avoids compounding scale. Translation is copied from that pose;
/// unity X/Z factors preserve its other axes. Only the scratch matrix's 3x3
/// is initialized or read. The caller owns scale decay, invalidation and release.
static inline void _ratApplyRootScale(GfxCoord* rootCoord, const RatWork* work, ActorScaleScratch* scratch)
{
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->squashScale;
    scratch->scale.vz = ONE;
    rootCoord->coord  = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix);
}

/// Applies the corpse's decaying Q12 vertical scale to its saved root pose.
///
/// Requires live work/model and the matrix saved on death entry. Each call
/// subtracts 80 while scale exceeds 512, allowing the decrement to cross the
/// cutoff. Restoring the saved matrix avoids compounding scale; translation
/// and X/Z scale are retained. Marks the root dirty without composing it and
/// releases the reserved `ActorScaleScratch` before returning.
static void _ratSquash(Task* actor)
{
    enum {
        RAT_SQUASH_SCALE_CUTOFF = 512,
        RAT_SQUASH_SCALE_STEP   = 80,
    };

    GfxCoord*          rootCoord;
    ActorScaleScratch* scratchHead;
    ActorScaleScratch* scratch;
    RatWork*           work;

    scratchHead                             = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = actor->work;
    scratch                                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = actor->extra.tmd->coords;
    if (work->squashScale > RAT_SQUASH_SCALE_CUTOFF) {
        work->squashScale -= RAT_SQUASH_SCALE_STEP;
    }
    _ratApplyRootScale(rootCoord, work, scratch);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}
