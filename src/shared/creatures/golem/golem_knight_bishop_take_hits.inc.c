#include "gameplay/room_effects.h"

/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Scratch-stack block of the per-frame hit handler.
///
/// Reserved for the length of the call. Only the two vectors are used; what
/// the rest of the block was laid out for is unproven.
typedef struct {
    WorldCollisionDelta delta;          // push-back resolved from a contact table, in its fixed-point view; then, as a whole-unit vector, the player's position less the golem's root, for a weapon hit
    byte                field_10[0x10]; // never accessed; role unproven
    SVECTOR             effectOffset;   // where the hit effect appears, as an offset from part 3 along that part's axes
    byte                field_28[8];    // never accessed; role unproven
} _GolemKnightBishopHitScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopHitScratch, 0x30);

/// Facing value and model-coordinate index used by the impact-effect macro.
enum {
    GOLEM_KNIGHT_BISHOP_HIT_FROM_FRONT        = 1,
    GOLEM_KNIGHT_BISHOP_HIT_EFFECT_PART_INDEX = 3
};

/// Resolves contacts, applies attack damage and updates the GOLEM's hit response.
///
/// `task` owns a live GOLEM model/work block and Enemy spawn argument. Contact
/// arrays cover their full declared extent and are cleared after consumption.
/// Grid push-back uses the integer halves of Q16 deltas, restoring the previous
/// root position when opposed; attacks measure full XYZ distance in world units.
/// Feints only break, cooldown blocks further attacks, and an active grab records
/// a weapon break instead of immediately changing sequence. Hit effects suppress
/// consecutive repeated keys within this pass. Reserves 48 scratch bytes.
static void _golemKnightBishopTakeHits(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_ATTACK_DOUBLE_DAMAGE = 5,
        GOLEM_KNIGHT_BISHOP_ATTACK_INTERRUPT     = 9,
        GOLEM_KNIGHT_BISHOP_GRAB_HOLDING         = 1,
        GOLEM_KNIGHT_BISHOP_GRAB_BREAK_WEAPON    = 2,
        GOLEM_KNIGHT_BISHOP_HIT_EFFECT_BEHIND    = -150,
        GOLEM_KNIGHT_BISHOP_HIT_EFFECT_FRONT     = 200,
    };
    s32                           lastEffectKey;
    GolemKnightBishopWork*        work;
    _GolemKnightBishopHitScratch* previousTop;
    _GolemKnightBishopHitScratch* scratch;
    _GolemKnightBishopHitScratch* reservedBlock;
    Enemy*                        enemy;
    GfxCoord*                     root;
    s32                           contactIndex;
    s32                           damage;
    s32                           attackAttribute;
    s32                           cooldownFrames;
    s16                           cooldownRemaining;
    s16                           effectForwardOffset;

    lastEffectKey = 0;
    work          = task->work;
    // Keep the cursor, reservation and working views distinct for the matched address calculations.
    previousTop                                        = SCRATCH_STACK_CURSOR(_GolemKnightBishopHitScratch);
    reservedBlock                                      = previousTop - 1;
    SCRATCH_STACK_CURSOR(_GolemKnightBishopHitScratch) = reservedBlock;
    scratch                                            = reservedBlock;
    root                                               = task->extra.tmd->coords;
    enemy                                              = task->spawnArg2.pointer;

    // Resolve floor motion before horizontal hurt-body obstruction.
    switch (worldCollisionResolvePushback(work->groundContacts, &scratch->delta, ARRAY_SIZE(work->groundContacts), NULL)) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            root->coord.t[0] += previousTop[-1].delta.fixed.vx.halves.integer;
            root->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            root->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            root->coord.t[0] = work->prevRootPos.vx;
            root->coord.t[1] = work->prevRootPos.vy;
            root->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    worldCollisionClearContacts(work->groundContacts);

    if (work->hurtBody.flags & WORLD_COLLISION_BODY_GRID_ENABLED) {
        switch (worldCollisionResolvePushback(work->hurtContacts, &scratch->delta, ARRAY_SIZE(work->hurtContacts), NULL)) {
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
                break;
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:
                root->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
                root->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case WORLD_COLLISION_PUSHBACK_OPPOSED:
                root->coord.t[0] = work->prevRootPos.vx;
                root->coord.t[2] = work->prevRootPos.vz;
                break;
        }
    }

    if (work->hitCooldown != 0) {
        cooldownRemaining = work->hitCooldown - 1;
        work->hitCooldown = cooldownRemaining;
        if (cooldownRemaining <= 0) {
            work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->hitCooldown     = 0;
            work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
        }
    }

    /// Places the impact effect on part 3, on the side facing the hit.
    ///
    /// Captures live `Task* task`, `GolemKnightBishopWork* work`, writable hit
    /// scratch, an in-range hurt-contact `contactIndex`, and writable s16
    /// `effectForwardOffset`. The model must include coordinate 3; its local Z
    /// offset is -150 behind or 200 in front, in whole coordinate units. Writes
    /// only scratch XYZ and the temporary before reading the contact key once.
    /// `hitEffectArg` must select that same live part; the effect spawner may
    /// update it and borrow the part for spawned tasks. Scratch is consumed
    /// synchronously. Use this statement list only inside a braced body;
    /// undefined after this handler.
#define GOLEM_KNIGHT_BISHOP_SPAWN_HIT_EFFECT()                                                                  \
    scratch->effectOffset.vx = 0;                                                                               \
    scratch->effectOffset.vy = 0;                                                                               \
    effectForwardOffset      = GOLEM_KNIGHT_BISHOP_HIT_EFFECT_BEHIND;                                           \
    if (work->hitFromFront == GOLEM_KNIGHT_BISHOP_HIT_FROM_FRONT) {                                             \
        effectForwardOffset = GOLEM_KNIGHT_BISHOP_HIT_EFFECT_FRONT;                                             \
    }                                                                                                           \
    scratch->effectOffset.vz = effectForwardOffset;                                                             \
    effectSpawnHit(damageGetPlayerAttackEffectId(work->hurtContacts[contactIndex].key.value),                   \
                   &task->extra.tmd->coords[GOLEM_KNIGHT_BISHOP_HIT_EFFECT_PART_INDEX], &scratch->effectOffset, \
                   &work->hitEffectArg)

    // Process attack keys in table order; the shared cooldown can suppress later hits.
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->hurtContacts); contactIndex++) {
        switch ((u32)work->hurtContacts[contactIndex].key.value >> 16) {
            case 0:
                break;
            case (WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16):
                if (work->feinting == 1) {
                    work->feintBroken = 1;
                }
                break;
            case (WORLD_COLLISION_CONTACT_ATTACK >> 16):
                if (work->hitCooldown != 0) {
                    break;
                }
                if (work->feinting == 1) {
                    work->feintBroken = 1;
                    break;
                }
                scratch->delta.vector.vx = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
                scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - root->coord.t[1];
                scratch->delta.vector.vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
                // The complement and logical shift yield one for a nonnegative facing dot product.
                work->hitFromFront = (u32) ~(scratch->delta.vector.vx * root->coord.m[0][2] +
                                             scratch->delta.vector.vy * root->coord.m[1][2] +
                                             scratch->delta.vector.vz * root->coord.m[2][2]) >>
                                     31;
                damage          = damageComputePlayerAttack(work->hurtContacts[contactIndex].key.value,
                                                            SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx +
                                                                        scratch->delta.vector.vy * scratch->delta.vector.vy +
                                                                        scratch->delta.vector.vz * scratch->delta.vector.vz),
                                                            0, 0);
                attackAttribute = damageGetPlayerAttackReaction(work->hurtContacts[contactIndex].key.value);
                if ((u16)attackAttribute == GOLEM_KNIGHT_BISHOP_ATTACK_DOUBLE_DAMAGE) {
                    damage *= 2;
                    effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[3], 2, NULL);
                }
                if (damageRollCriticalHit(enemy, work->hurtContacts[contactIndex].key.value, 0) != 0) {
                    damage *= 4;
                    if ((u16)attackAttribute != GOLEM_KNIGHT_BISHOP_ATTACK_DOUBLE_DAMAGE) {
                        effectSpawn(EFFECT_CRITICAL_HIT, &task->extra.tmd->coords[3], 0, NULL);
                    }
                }
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                damageAccumulateLifeDrainHp(enemy, work->hurtContacts[contactIndex].key.value, damage, 0);
                enemy->hp             -= damage;
                work->interruptDamage += damage;
                switch ((u16)attackAttribute) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case DAMAGE_PLAYER_REACTION_POISON:
                    case 4:
                    case GOLEM_KNIGHT_BISHOP_ATTACK_DOUBLE_DAMAGE:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                    case 8:
                        break;
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        if (work->flickerStage == 0) {
                            work->flickerStage = 1;
                            work->fadeState    = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                        }
                        break;
                    case GOLEM_KNIGHT_BISHOP_ATTACK_INTERRUPT:
                        work->interruptDamage += GOLEM_KNIGHT_BISHOP_BOX_INTERRUPT_DAMAGE;
                        break;
                }
                if (lastEffectKey != work->hurtContacts[contactIndex].key.value) {
                    lastEffectKey = work->hurtContacts[contactIndex].key.value;
                    GOLEM_KNIGHT_BISHOP_SPAWN_HIT_EFFECT();
                }
                cooldownFrames = damageGetPlayerAttackHitCooldown(work->hurtContacts[contactIndex].key.value);
                if (cooldownFrames > 0) {
                    work->hitCooldown = cooldownFrames;
                }
                if (work->fadeState >= GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM) {
                    work->tintRequest = GOLEM_KNIGHT_BISHOP_TINT_WHITE;
                }
                if (work->grabStage != GOLEM_KNIGHT_BISHOP_GRAB_HOLDING) {
                    _golemKnightBishopPickHitReaction(task, damage);
                } else {
                    work->grabBreak = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_WEAPON;
                }
                break;
        }
    }
    worldCollisionClearContacts(work->hurtContacts);
    if (work->strikeContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(work->strikeContacts);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemKnightBishopHitScratch);
}

#undef GOLEM_KNIGHT_BISHOP_SPAWN_HIT_EFFECT

/// Starts an upright reaction and prevents the interrupted strike from hitting.
///
/// Requires live GOLEM work. `sequence` selects light/heavy flinch, knockdown
/// or collapse death; resetting its step restarts that sequence on the next
/// dispatch. Disables body pairing without unlinking the strike sphere.
static inline void _golemKnightBishopBeginHitReaction(GolemKnightBishopWork* work, s16 sequence)
{
    enum { GOLEM_KNIGHT_BISHOP_HIT_REACTION_START_STEP = 0 };

    work->sequence          = sequence;
    work->step              = GOLEM_KNIGHT_BISHOP_HIT_REACTION_START_STEP;
    work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

/// Selects the GOLEM's reaction after HP loss or an interrupted grab.
///
/// `task` owns a live GOLEM work block and Enemy with valid parameters;
/// `damage` is HP damage, possibly accumulated across a grab. Zero HP selects
/// death and stops appearance sounds. Below one tenth maximum HP selects a
/// knockdown or downed hit. Other unlocked or flickering hits flinch, with
/// damage below 80 selecting the light reaction. A locked downed pose delays
/// its hit/death sequence; upright reactions restart and disarm the strike.
static void _golemKnightBishopPickHitReaction(Task* task, s32 damage)
{
    enum {
        GOLEM_KNIGHT_BISHOP_REACTION_NONE           = 0,
        GOLEM_KNIGHT_BISHOP_REACTION_LIGHT_FLINCH   = 1,
        GOLEM_KNIGHT_BISHOP_REACTION_HEAVY_FLINCH   = 2,
        GOLEM_KNIGHT_BISHOP_REACTION_KNOCKDOWN      = 3,
        GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_HIT     = 4,
        GOLEM_KNIGHT_BISHOP_REACTION_COLLAPSE_DEATH = 5,
        GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_DEATH   = 6,
        GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_DAMAGE     = 80,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_HP_DIVISOR    = 10,
    };
    Enemy*                 enemy    = task->spawnArg2.pointer;
    s16                    hp       = enemy->hp;
    GolemKnightBishopWork* work     = task->work;
    u32                    reaction = GOLEM_KNIGHT_BISHOP_REACTION_NONE;
    s32                    maxHp;

    if (hp <= 0) {
        reaction = GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_DEATH;
        if (work->downedPose == 0) {
            reaction = GOLEM_KNIGHT_BISHOP_REACTION_COLLAPSE_DEATH;
        }
        if (work->appearSound != 0) {
            sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            work->appearSound = 0;
        }
        if (work->vanishSound != 0) {
            sndEvtRequestScriptStop(work->vanishSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            work->vanishSound = 0;
        }
    } else if (maxHp = enemy->param->hpMax, hp < maxHp / GOLEM_KNIGHT_BISHOP_KNOCKDOWN_HP_DIVISOR) {
        reaction = GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_HIT;
        if (work->downedPose == 0) {
            reaction = GOLEM_KNIGHT_BISHOP_REACTION_KNOCKDOWN;
        }
    } else if (work->reactionLock == 0 || work->flickerStage != 0) {
        work->reactionLock = 0;
        reaction           = GOLEM_KNIGHT_BISHOP_REACTION_HEAVY_FLINCH;
        if (damage < GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_DAMAGE) {
            reaction = GOLEM_KNIGHT_BISHOP_REACTION_LIGHT_FLINCH;
        }
    }

    // Downed reactions wait for the fall lock; upright reactions cancel the strike.
    switch (reaction) {
        case GOLEM_KNIGHT_BISHOP_REACTION_NONE:
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_LIGHT_FLINCH:
            _golemKnightBishopBeginHitReaction(work, GOLEM_KNIGHT_BISHOP_SEQUENCE_LIGHT_FLINCH);
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_HEAVY_FLINCH:
            _golemKnightBishopBeginHitReaction(work, GOLEM_KNIGHT_BISHOP_SEQUENCE_HEAVY_FLINCH);
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_KNOCKDOWN:
            _golemKnightBishopBeginHitReaction(work, GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL);
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_HIT:
            if (work->reactionLock == 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_HIT;
                work->step     = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_COLLAPSE_DEATH:
            _golemKnightBishopBeginHitReaction(work, GOLEM_KNIGHT_BISHOP_SEQUENCE_COLLAPSE_DEATH);
            break;
        case GOLEM_KNIGHT_BISHOP_REACTION_DOWNED_DEATH:
            if (work->reactionLock == 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL_DEATH;
                work->step     = 0;
            }
            break;
    }
}

/// Scratch-stack block of the region scan's circle test.
///
/// Reserved for the length of the call. A box region is tested without it.
typedef struct {
    VECTOR  offset;         // the circle's centre less the player's position, x and z only; then the spot behind the player as an offset along the world axes
    byte    field_10[0x10]; // never accessed; role unproven
    SVECTOR localOffset;    // that spot in the player's frame: 0x5AA behind them
} _GolemKnightBishopRegionScanScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopRegionScanScratch, 0x28);

/// Waits for the player to enter a room region and starts its attack.
///
/// `task` owns a live GOLEM work block and a borrowed `regionCount`-entry table.
/// Circles propose a grab behind the player, then wait one collision pass to
/// check the probes; boxes immediately select their post approach. Circle
/// radii and box edges are exclusive, in world units. Reserves 40 scratch bytes.
static void _golemKnightBishopRegionScanSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_REGION_SCAN_SEARCH   = 0,
        GOLEM_KNIGHT_BISHOP_REGION_SCAN_PROBE    = 1,
        GOLEM_KNIGHT_BISHOP_REGION_SCAN_YAW_MASK = 0xFFF,
    };
    _GolemKnightBishopRegionScanScratch* scratch;
    GolemKnightBishopWork*               work;
    GfxCoord*                            playerRoot;
    s32                                  regionIndex;

    work    = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GolemKnightBishopRegionScanScratch);
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_REGION_SCAN_SEARCH:
            for (regionIndex = 0; regionIndex < work->regionCount; regionIndex++) {
                switch (work->regions[regionIndex].kind) {
                    case GOLEM_KNIGHT_BISHOP_REGION_CIRCLE:
                        scratch->offset.vx = work->regions[regionIndex].x - gPlayerStatus.coordMtx->t[0];
                        scratch->offset.vz = work->regions[regionIndex].z - gPlayerStatus.coordMtx->t[2];
                        if (SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vz * scratch->offset.vz) < work->regions[regionIndex].param.radius) {
                            work->step              = GOLEM_KNIGHT_BISHOP_REGION_SCAN_PROBE;
                            playerRoot              = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                            work->targetYaw         = ratan2(playerRoot->coord.m[0][2], playerRoot->coord.m[2][2]) & GOLEM_KNIGHT_BISHOP_REGION_SCAN_YAW_MASK;
                            scratch->localOffset.vx = 0;
                            scratch->localOffset.vy = 0;
                            scratch->localOffset.vz = -GOLEM_KNIGHT_BISHOP_GRAB_TARGET_DISTANCE;
                            gte_SetRotMatrix(&playerRoot->coord);
                            gte_ldv0(&scratch->localOffset);
                            gte_rtv0();
                            gte_stlvnl(&scratch->offset);
                            work->targetPos.vx = gPlayerStatus.coordMtx->t[0] + scratch->offset.vx;
                            work->targetPos.vy = gPlayerStatus.coordMtx->t[1];
                            SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopRegionScanScratch));
                            // The retained order reads Z after release, before any nested reservation.
                            work->targetPos.vz         = gPlayerStatus.coordMtx->t[2] + scratch->offset.vz;
                            work->pathProbeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                            work->spotProbeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                            return;
                        }
                        break;
                    case GOLEM_KNIGHT_BISHOP_REGION_BOX:
                        if (GOLEM_KNIGHT_BISHOP_PLAYER_INSIDE_BOX(&work->regions[regionIndex], gPlayerStatus.coordMtx)) {
                            work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
                            work->step       = 0;
                            work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
                            work->boxRegion  = regionIndex;
                            SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopRegionScanScratch));
                            return;
                        }
                        break;
                }
            }
            break;
        case GOLEM_KNIGHT_BISHOP_REGION_SCAN_PROBE:
            // The probes now contain the collision pass's answer for the proposed grab.
            if (work->probeContacts[0].key.value == 0) {
                work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
                work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
            }
            work->step                 = 0;
            work->pathProbeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            work->spotProbeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            worldCollisionClearContacts(work->probeContacts);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopRegionScanScratch));
}

/// Waits while hidden, chooses an attack and checks its placement.
///
/// The wait scales down with attack count; a broken feint goes straight to a
/// counterattack placement. One-frame grid probes must be resolved between
/// placement and the next sequence update. Their shared contact record decides
/// whether the grab or strike spot is clear. The roll tables have 16 entries;
/// failed placement retries the pick without changing retained attack history.
static void _golemKnightBishopIdleSeq(Task* task)
{
    enum { GOLEM_KNIGHT_BISHOP_IDLE_WAIT_SCALE = 16 };
    enum {
        GOLEM_KNIGHT_BISHOP_IDLE_START       = 0,
        GOLEM_KNIGHT_BISHOP_IDLE_WAIT        = 1,
        GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK = 2,
        GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB   = 3,
        GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE = 4,
        GOLEM_KNIGHT_BISHOP_IDLE_START_BOX   = 5,
    };
    GolemKnightBishopWork* work;

    work = task->work;
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_IDLE_START:
            work->forwardSpeed     = 0;
            work->flickerStage     = GOLEM_KNIGHT_BISHOP_FLICKER_NONE;
            work->counterattacking = 0;
            if (work->feintBroken != 0) {
                RANDOM_LCG_ADVANCE();
                work->step             = _gGolemKnightBishopCounterattackStepOffsets[RANDOM_LCG_DRAW() & (ARRAY_SIZE(_gGolemKnightBishopCounterattackStepOffsets) - 1)] + GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                work->counterattacking = 1;
                _golemKnightBishopPlaceTarget(task);
                work->feinting = 0;
            } else if (work->feinting == 0) {
                work->timer = (_gGolemKnightBishopIdleWaitFrames[((RANDOM_LCG_ADVANCE()) >> 16) & (ARRAY_SIZE(_gGolemKnightBishopIdleWaitFrames) - 1)] * (GOLEM_KNIGHT_BISHOP_IDLE_WAIT_SCALE - work->attackCount)) / GOLEM_KNIGHT_BISHOP_IDLE_WAIT_SCALE;
                work->step  = GOLEM_KNIGHT_BISHOP_IDLE_WAIT;
            } else {
                work->timer    = 0;
                work->step     = GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                work->feinting = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_WAIT:
            work->timer--;
            if (work->timer <= 0) {
                work->step  = GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                work->timer = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK:
            if (work->lastAttack != GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH && _golemKnightBishopPlayerInBox(task) != 0) {
                work->step        = GOLEM_KNIGHT_BISHOP_IDLE_START_BOX;
                work->repeatCount = 0;
                break;
            }
            if (work->lastAttack == GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB) {
                RANDOM_LCG_ADVANCE();
                if ((s32)(RANDOM_LCG_DRAW() & 0xF) < work->repeatCount + 10) {
                    work->step        = GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE;
                    work->repeatCount = 0;
                } else {
                    work->step = GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB;
                    work->repeatCount++;
                }
            } else if (work->lastAttack == GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE) {
                RANDOM_LCG_ADVANCE();
                if ((s32)(RANDOM_LCG_DRAW() & 0xF) < work->repeatCount + 8) {
                    work->step        = GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB;
                    work->repeatCount = 0;
                } else {
                    work->step = GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE;
                    work->repeatCount++;
                }
            } else {
                RANDOM_LCG_ADVANCE();
                work->step        = (RANDOM_LCG_DRAW() & 0xF) < 8 ? GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB : GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE;
                work->repeatCount = 0;
            }
            _golemKnightBishopPlaceTarget(task);
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_TEST_GRAB:
            if (work->probeContacts[0].key.value == 0) {
                work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
                work->step       = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
                if (work->attackCount < GOLEM_KNIGHT_BISHOP_ATTACK_COUNT_LIMIT) {
                    work->attackCount++;
                }
            } else {
                work->step = GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                if (work->repeatCount != 0) {
                    work->repeatCount--;
                }
            }
            work->pathProbeBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            work->spotProbeBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            worldCollisionClearContacts(work->probeContacts);
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_TEST_STRIKE:
            if (work->probeContacts[0].key.value == 0) {
                work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE;
                work->step       = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_STRIKE;
                if (work->attackCount < GOLEM_KNIGHT_BISHOP_ATTACK_COUNT_LIMIT) {
                    work->attackCount++;
                }
            } else {
                work->step = GOLEM_KNIGHT_BISHOP_IDLE_PICK_ATTACK;
                if (work->repeatCount != 0) {
                    work->repeatCount--;
                }
            }
            work->pathProbeBody.flags &= ~WORLD_COLLISION_BODY_GRID_ENABLED;
            worldCollisionClearContacts(work->probeContacts);
            break;
        case GOLEM_KNIGHT_BISHOP_IDLE_START_BOX:
            work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
            work->step       = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
            worldCollisionClearContacts(work->aimBeamContacts);
            if (work->attackCount < GOLEM_KNIGHT_BISHOP_ATTACK_COUNT_LIMIT) {
                work->attackCount++;
            }
            break;
    }
}

/// Finds the first box region containing the player's world X/Z position.
///
/// `task` owns a live GOLEM work block and its borrowed room-region table.
/// Returns 1 and saves the index in `boxRegion` on a strict interior hit;
/// otherwise returns 0 and leaves the previous index unchanged.
static s32 _golemKnightBishopPlayerInBox(Task* task)
{
    GolemKnightBishopWork* work;
    s16                    regionCount;
    s32                    regionIndex;

    work        = task->work;
    regionCount = work->regionCount;
    for (regionIndex = 0; regionIndex < regionCount; regionIndex++) {
        if (work->regions[regionIndex].kind == GOLEM_KNIGHT_BISHOP_REGION_BOX) {
            if (GOLEM_KNIGHT_BISHOP_PLAYER_INSIDE_BOX(&work->regions[regionIndex], gPlayerStatus.coordMtx)) {
                work->boxRegion = regionIndex;
                return 1;
            }
        }
    }
    return 0;
}

#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Places and probes the next grab or strike relative to the player.
///
/// `task` owns a live GOLEM work block; idle steps 3 and 4 select grab and
/// strike, respectively. A grab is 1450 world units behind the player's heading.
/// A strike is 1200 units away, within a quarter turn of that heading unless a
/// broken feint allows a full turn. `targetYaw` uses 4096 units per turn.
/// The probe endpoint uses the relative angle before the player's yaw is added;
/// both probes are enabled for a grab, only the path probe for a strike.
/// Reserves and releases 24 scratch bytes and changes the GTE rotation state.
static void _golemKnightBishopPlaceTarget(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_TARGET_GRAB_STEP             = 3,
        GOLEM_KNIGHT_BISHOP_TARGET_STRIKE_STEP           = 4,
        GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK              = 0xFFF,
        GOLEM_KNIGHT_BISHOP_TARGET_QUARTER_TURN_MASK     = 0x3FF,
        GOLEM_KNIGHT_BISHOP_TARGET_YAW_SIGN_BIT          = 0x400,
        GOLEM_KNIGHT_BISHOP_PROBE_HEIGHT                 = -1000,
        GOLEM_KNIGHT_BISHOP_GRAB_PROBE_BACK              = -2000,
        GOLEM_KNIGHT_BISHOP_STRIKE_PROBE_DISTANCE_DIV16  = 125,
        GOLEM_KNIGHT_BISHOP_STRIKE_TARGET_DISTANCE_DIV16 = 75,
        GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS = 8,
    };
    GolemKnightBishopOffsetScratch* scratch;
    GolemKnightBishopWork*          work;
    GfxCoord*                       playerRoot;
    u32                             randomDraw;
    s32                             relativeYaw;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GolemKnightBishopOffsetScratch);
    work    = task->work;
    if (work->step == GOLEM_KNIGHT_BISHOP_TARGET_GRAB_STEP) {
        playerRoot          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        work->targetYaw     = ratan2(playerRoot->coord.m[0][2], playerRoot->coord.m[2][2]) & GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK;
        scratch->operand.vz = -GOLEM_KNIGHT_BISHOP_GRAB_TARGET_DISTANCE;
        scratch->operand.vx = 0;
        scratch->operand.vy = 0;
        gte_SetRotMatrix(&playerRoot->coord);
        gte_ldv0(&scratch->operand);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        work->targetPos.vx                = gPlayerStatus.coordMtx->t[0] + scratch->offset.vx;
        work->targetPos.vy                = gPlayerStatus.coordMtx->t[1];
        work->targetPos.vz                = gPlayerStatus.coordMtx->t[2] + scratch->offset.vz;
        work->pathProbeCapsule.ends[0].vy = GOLEM_KNIGHT_BISHOP_PROBE_HEIGHT;
        work->pathProbeCapsule.ends[0].vz = GOLEM_KNIGHT_BISHOP_GRAB_PROBE_BACK;
        work->pathProbeCapsule.ends[0].vx = 0;
        work->pathProbeBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->spotProbeBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
    } else if (work->step == GOLEM_KNIGHT_BISHOP_TARGET_STRIKE_STEP) {
        if (work->feintBroken != 0) {
            RANDOM_LCG_ADVANCE();
            work->targetYaw = RANDOM_LCG_DRAW() & GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK;
        } else {
            randomDraw      = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = randomDraw;
            relativeYaw     = (randomDraw >> 16) & GOLEM_KNIGHT_BISHOP_TARGET_QUARTER_TURN_MASK;
            if (!((randomDraw >> 16) & GOLEM_KNIGHT_BISHOP_TARGET_YAW_SIGN_BIT)) {
                relativeYaw = -relativeYaw;
            }
            work->targetYaw = relativeYaw;
        }
        // Trig is Q12; distances divided by 16 and a logical shift retain the halfword result.
        work->pathProbeCapsule.ends[0].vx = (u32)(rsin(work->targetYaw) * GOLEM_KNIGHT_BISHOP_STRIKE_PROBE_DISTANCE_DIV16) >> GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS;
        work->pathProbeCapsule.ends[0].vy = GOLEM_KNIGHT_BISHOP_PROBE_HEIGHT;
        work->pathProbeCapsule.ends[0].vz = (u32)(rcos(work->targetYaw) * GOLEM_KNIGHT_BISHOP_STRIKE_PROBE_DISTANCE_DIV16) >> GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS;
        playerRoot                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        work->targetYaw                   = (work->targetYaw + (ratan2(playerRoot->coord.m[0][2], playerRoot->coord.m[2][2]) & GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK)) & GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK;
        scratch->operand.vx               = (u32)(rsin(work->targetYaw) * GOLEM_KNIGHT_BISHOP_STRIKE_TARGET_DISTANCE_DIV16) >> GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS;
        scratch->operand.vz               = (u32)(rcos(work->targetYaw) * GOLEM_KNIGHT_BISHOP_STRIKE_TARGET_DISTANCE_DIV16) >> GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS;
        work->targetPos.vx                = gPlayerStatus.coordMtx->t[0] + scratch->operand.vx;
        work->targetPos.vy                = gPlayerStatus.coordMtx->t[1];
        work->targetPos.vz                = gPlayerStatus.coordMtx->t[2] + scratch->operand.vz;
        work->pathProbeBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopOffsetScratch));
}

#include "gameplay/room_effects.h"

/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Scratch-stack block of the grab sequence.
///
/// It holds the payloads of the messages the grab sends the player, each
/// borrowed for one synchronous dispatch, and the vectors the hold's placement
/// is worked out in. Reserved for the length of the call.
typedef struct {
    GameActorButtonPressHold buttonPressHold; // payload of the request that takes the player into the hold; only `pressCount` is set
    AnimationPlayRequest     playerAnim;      // animation the player is told to play at each stage of the hold
    ActorTransform           playerPlacement; // where the player is put as the hold is taken: 0x5AA in front of the golem, turned the way it faces
    VECTOR                   offset;          // that distance along the world axes: `operand` after the GTE rotation through the golem's root
    SVECTOR                  operand;         // short vector being worked on: the Euler angles the root's rotation is built from, the local offset to rotate, or the kill effect's offset from the player's part 4
} _GolemKnightBishopGrabScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopGrabScratch, 0x5C);

/// Replaces the player's animation bank and resets playback to one grab clip.
///
/// `clipId` is 1 start, 2 hold, 3 kill or 4 release in the carrier's five-entry
/// player bank. `request` is writable storage borrowed only through synchronous
/// dispatch; the bank and clip data must remain loaded through playback.
/// Requests grid collision enablement and resets without interpolation.
///
/// `player` is a live `Task*`, `request` an `AnimationPlayRequest*` and `clipId`
/// an integer. Use as a standalone statement inside a braced block. The request
/// expression is evaluated six times and must have no side effects; player and
/// clip are evaluated once. Borrows `_gGolemKnightBishopPlayerGrabAnimSets` from the
/// carrier and captures no caller locals.
#define GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, request, clipId)                    \
    {                                                                                              \
        (request)->source.sets          = _gGolemKnightBishopPlayerGrabAnimSets;                   \
        (request)->animationId          = (clipId);                                                \
        (request)->blend                = ANIMATION_BLEND_RESET;                                   \
        (request)->blendFrames          = 0;                                                       \
        (request)->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                        \
        TASK_MESSAGE_DISPATCH_POINTER((player), ANIMATION_MESSAGE_REPLACE_AND_PLAY, (request), 0); \
    }

/// Takes scripted control of the player for a damaging hold or fatal grab.
///
/// The live player must accept the button-press hold before either actor is placed.
/// Message payloads borrow this call's scratch storage only for synchronous
/// dispatch. After the initial grace period, per-kind checks apply damage or
/// select a kill from the difficulty row (0..4), HP and accumulated damage ticks. The first
/// above-limit check arms the kill roll without rolling. Struggle or weapon
/// damage can break the hold; release playback finishes through
/// `_golemKnightBishopTickGrabRelease`. A fatal grab loads and queues the player's
/// death sound after the killing animation frame.
static void _golemKnightBishopGrabSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_DEATH_SOUND_LOAD = 0,
        GOLEM_KNIGHT_BISHOP_DEATH_SOUND_WAIT = 1,
        GOLEM_KNIGHT_BISHOP_DEATH_SOUND_DONE = 2,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_GRAB_KILL_FRAME              = 26,
        GOLEM_KNIGHT_BISHOP_GRAB_ESCAPE_PRESSES          = 25,
        GOLEM_KNIGHT_BISHOP_PLAYER_SCRIPTED_ATTACK_STATE = 10,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_PLAYER_ENTER_SCRIPTED_ATTACK = 1024,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_GRAB_TAKE        = 0,
        GOLEM_KNIGHT_BISHOP_GRAB_PLAY_START  = 1,
        GOLEM_KNIGHT_BISHOP_GRAB_WAIT_START  = 2,
        GOLEM_KNIGHT_BISHOP_GRAB_HOLD        = 3,
        GOLEM_KNIGHT_BISHOP_GRAB_BACK_AWAY   = 4,
        GOLEM_KNIGHT_BISHOP_GRAB_KILL        = 5,
        GOLEM_KNIGHT_BISHOP_GRAB_EARLY_BREAK = 6,
        GOLEM_KNIGHT_BISHOP_GRAB_DEATH_SOUND = 7,
    };
    GolemKnightBishopWork*         work;
    GfxCoord*                      root;
    Task*                          player;
    _GolemKnightBishopGrabScratch* scratch;
    GfxCoord*                      playerRoot;
    s32                            killSelected;
    s32                            sound;
    s32                            killChance;
    s16                            framesLeft;
    s16                            retreatSpeed;
    s16                            deathSoundStep;

    work         = task->work;
    root         = task->extra.tmd->coords;
    player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(_GolemKnightBishopGrabScratch);
    playerRoot   = player->extra.tmd->coords;
    killSelected = 0;
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_GRAB_TAKE:
            // Take control before moving either actor; refusal leaves placement intact.
            if (((GameActor*)player->work)->mode != GAME_ACTOR_MODE_SCRIPTED && gPlayerStatus.hp > 0) {
                scratch->buttonPressHold.pressCount = GOLEM_KNIGHT_BISHOP_GRAB_ESCAPE_PRESSES;
                if (TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &scratch->buttonPressHold, 0) == 0) {
                    work->anim          = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_START;
                    work->step          = GOLEM_KNIGHT_BISHOP_GRAB_PLAY_START;
                    work->grabBreak     = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE;
                    work->feintBroken   = 0;
                    work->grabStage     = GOLEM_KNIGHT_BISHOP_GRAB_HOLDING;
                    scratch->operand.vx = 0;
                    scratch->operand.vy = work->targetYaw;
                    scratch->operand.vz = 0;
                    RotMatrix(&scratch->operand, &root->coord);
                    root->coord.t[0]    = work->targetPos.vx;
                    root->coord.t[1]    = work->targetPos.vy;
                    root->coord.t[2]    = work->targetPos.vz;
                    scratch->operand.vx = 0;
                    scratch->operand.vy = 0;
                    scratch->operand.vz = GOLEM_KNIGHT_BISHOP_GRAB_TARGET_DISTANCE;
                    gte_SetRotMatrix(&root->coord);
                    gte_ldv0(&scratch->operand);
                    gte_rtv0();
                    gte_stlvnl(&scratch->offset);
                    scratch->playerPlacement.pos.vx = root->coord.t[0] + scratch->offset.vx;
                    scratch->playerPlacement.pos.vy = root->coord.t[1] + scratch->offset.vy;
                    scratch->playerPlacement.pos.vz = root->coord.t[2] + scratch->offset.vz;
                    scratch->playerPlacement.rot.vx = 0;
                    scratch->playerPlacement.rot.vy = work->targetYaw;
                    scratch->playerPlacement.rot.vz = 0;
                    TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &scratch->playerPlacement, 0);
                    padScriptSpawnVariableMotorRamp(0xA, 0xFF, 0x80);
                    sound = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 6;
                    sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(playerRoot), (s8)worldCoordGetOriginAudioDepth(playerRoot));
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                    work->step     = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                }
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_PLAY_START:
            GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_START);
            work->step                   = GOLEM_KNIGHT_BISHOP_GRAB_WAIT_START;
            work->translucencyFadeFrames = 0x3C;
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->colorBlendFadeFrames   = 0x1E;
            work->appearSound            = _gGolemKnightBishopAppearSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            sndEvtRequestScriptStart(work->appearSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_WAIT_START:
            if (work->animFrame >= 0x29) {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_HOLD;
                work->step            = GOLEM_KNIGHT_BISHOP_GRAB_HOLD;
                work->auxTimer        = 0x1E;
                work->timer           = 0;
                work->grabDamageTicks = 0;
                GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_HOLD);
                sceneEngageBattle(1);
                work->interruptDamage = 0;
                if (work->hitCooldown == 0) {
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
                }
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_HOLD:
            // Delay the first damage check, then roll a kill only after arming it.
            if (work->auxTimer > 0) {
                work->auxTimer--;
            } else {
                if (work->timer == 2) {
                    padScriptSpawnVariableMotorRamp(5, 0xC0, 0x80);
                }
                framesLeft  = work->timer - 1;
                work->timer = framesLeft;
                if (framesLeft <= 0) {
                    if (gPlayerStatus.hp > _gGolemKnightBishopGrabKillHpThresholds[gSceneCombatState.difficulty]) {
                        if (work->grabKillRollArmed == 0) {
                            work->grabKillRollArmed = 1;
                        } else {
                            killChance = work->grabDamageTicks * (0x32 - (gPlayerStatus.hp * 100) / gPlayerStatus.hpMax) / 2;
                            if (killChance > 0) {
                                killChance = (killChance * 0xFFF) / 100;
                                RANDOM_LCG_ADVANCE();
                                if ((s32)(RANDOM_LCG_DRAW() & 0xFFF) < killChance) {
                                    killSelected = 1;
                                }
                            }
                        }
                    } else {
                        killSelected = 1;
                    }
                    if (killSelected != 0) {
                        work->anim = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_KILL;
                        work->step = GOLEM_KNIGHT_BISHOP_GRAB_KILL;
                        GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_KILL);
                    } else {
                        work->timer = GOLEM_KNIGHT_BISHOP_GRAB_CHECK_INTERVAL_FRAMES;
                        taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackAttackKey(_gGolemKnightBishopAttacks, GOLEM_KNIGHT_BISHOP_ATTACK_GRAB), 0);
                        work->grabDamageTicks++;
                    }
                }
            }
            if (work->grabBreak != GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE) {
                if (work->grabBreak == GOLEM_KNIGHT_BISHOP_GRAB_BREAK_STRUGGLE) {
                    if (work->auxTimer > 0 && work->counterattacking == 0) {
                        work->anim             = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_EARLY_RELEASE;
                        work->step             = GOLEM_KNIGHT_BISHOP_GRAB_EARLY_BREAK;
                        work->grabReleaseTimer = 0;
                        if (work->appearSound != 0) {
                            sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                            work->appearSound = 0;
                        }
                        work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                    } else {
                        work->anim                   = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_RELEASE;
                        work->step                   = GOLEM_KNIGHT_BISHOP_GRAB_BACK_AWAY;
                        work->timer                  = 0x69;
                        work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                        work->translucencyFadeFrames = 0x4B;
                        work->grabReleaseTimer       = 0;
                        work->colorBlendFadeFrames   = 0x1E;
                        work->vanishSound            = _gGolemKnightBishopVanishSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                        sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                    }
                } else {
                    _golemKnightBishopPickHitReaction(task, work->interruptDamage);
                    work->grabReleaseTimer = 0;
                }
                work->grabStage = GOLEM_KNIGHT_BISHOP_GRAB_RELEASED;
                work->grabBreak = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE;
                GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_RELEASE);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_BACK_AWAY:
            retreatSpeed = 0;
            if (work->animFrame < 0x5F) {
                retreatSpeed = -0xA;
            }
            work->forwardSpeed = retreatSpeed;
            framesLeft         = work->timer - 1;
            work->timer        = framesLeft;
            if (framesLeft <= 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step     = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            _golemKnightBishopTickGrabRelease(task);
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_KILL:
            // The killing frame commits death; earlier interruptions still release.
            if (work->animFrame < GOLEM_KNIGHT_BISHOP_GRAB_KILL_FRAME) {
                if (work->grabBreak != GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE) {
                    work->anim      = GOLEM_KNIGHT_BISHOP_ANIM_GRAB_RELEASE;
                    work->grabBreak = GOLEM_KNIGHT_BISHOP_GRAB_BREAK_NONE;
                    GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION(player, &scratch->playerAnim, GOLEM_KNIGHT_BISHOP_PLAYER_GRAB_RELEASE);
                    work->timer                  = 0x69;
                    work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                    work->translucencyFadeFrames = 0x4B;
                    work->step                   = GOLEM_KNIGHT_BISHOP_GRAB_BACK_AWAY;
                    work->colorBlendFadeFrames   = 0x1E;
                    work->vanishSound            = _gGolemKnightBishopVanishSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                }
            } else if (work->animFrame == GOLEM_KNIGHT_BISHOP_GRAB_KILL_FRAME) {
                ((GameActor*)player->work)->state = GOLEM_KNIGHT_BISHOP_PLAYER_SCRIPTED_ATTACK_STATE;
                work->step                        = GOLEM_KNIGHT_BISHOP_GRAB_DEATH_SOUND;
                work->timer                       = 0;
                gGameSession->deathRestartDelay   = 0x5A;
                gGameSession->deathSoundCountdown = GAME_SESSION_DEATH_SOUND_HOLD;
                scratch->operand.vy               = -0x96;
                scratch->operand.vx               = 0;
                scratch->operand.vz               = 0xC8;
                effectSpawnHit(EFFECT_HIT_KIND_WEAPON_PUFF, &gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords[4], &scratch->operand, &_gGolemKnightBishopGrabKillEffectArg);
                padScriptSpawnVariableMotorRamp(0xA, 0xFF, 8);
                taskMessageDispatch(player, GOLEM_KNIGHT_BISHOP_PLAYER_ENTER_SCRIPTED_ATTACK, 0, 0);
                gPlayerStatus.hp = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_EARLY_BREAK:
            work->forwardSpeed = -0xA;
            _golemKnightBishopTickGrabRelease(task);
            if (work->grabStage == GOLEM_KNIGHT_BISHOP_GRAB_NONE) {
                work->forwardSpeed = 0;
                work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step         = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_GRAB_DEATH_SOUND:
            deathSoundStep = work->timer;
            switch (deathSoundStep) {
                case GOLEM_KNIGHT_BISHOP_DEATH_SOUND_LOAD:
                    cdCmdEnqueueDisplayResource(9, 0x1E, CD_COMMAND_DISPLAY_LOAD_DEFAULT);
                    work->timer = GOLEM_KNIGHT_BISHOP_DEATH_SOUND_WAIT;
                    break;
                case GOLEM_KNIGHT_BISHOP_DEATH_SOUND_WAIT:
                    if (cdCmdIsIdle() == 1) {
                        // Use the player's root for the death cue.
                        root = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                        sndEvtRequestScriptStart(SOUND_PLAYER_DEATH, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                        work->timer = GOLEM_KNIGHT_BISHOP_DEATH_SOUND_DONE;
                    }
                    break;
                case GOLEM_KNIGHT_BISHOP_DEATH_SOUND_DONE:
                    break;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemKnightBishopGrabScratch);
}

#undef GOLEM_KNIGHT_BISHOP_PLAY_GRAB_PLAYER_ANIMATION

#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Appears beside the player as a feint or a two-part strike.
///
/// Requires a target previously placed and collision-tested by the idle sequence.
/// A feint takes no weapon damage and shrinks away, or triggers an immediate
/// counterattack when broken. A real appearance can be interrupted before the
/// blow. The strike sphere is live on part 8 at frame 20, part 12 at frame 28,
/// and disabled at frame 35 as vanishing begins. Fade durations are positive
/// frame divisors, split from the sampled appearance length.
static void _golemKnightBishopStrikeSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_STRIKE_FIRST_FRAME  = 20,
        GOLEM_KNIGHT_BISHOP_STRIKE_SECOND_FRAME = 28,
        GOLEM_KNIGHT_BISHOP_STRIKE_END_FRAME    = 35,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_STRIKE_START        = 0,
        GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_APPEAR = 1,
        GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_WAIT   = 2,
        GOLEM_KNIGHT_BISHOP_STRIKE_APPEAR       = 3,
        GOLEM_KNIGHT_BISHOP_STRIKE_WAIT         = 4,
        GOLEM_KNIGHT_BISHOP_STRIKE_BLOW         = 5,
        GOLEM_KNIGHT_BISHOP_STRIKE_VANISH       = 6,
    };
    GolemKnightBishopOffsetScratch* scratch;
    GolemKnightBishopWork*          work;
    GfxCoord*                       root;
    s32                             strikeSound;
    u32                             randomDraw;
    u16                             appearanceFrames;
    s16                             translucencyFrames;
    s16                             framesLeft;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GolemKnightBishopOffsetScratch);
    work    = task->work;
    root    = task->extra.tmd->coords;
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_STRIKE_START:
            work->anim          = GOLEM_KNIGHT_BISHOP_ANIM_STAND;
            work->feinting      = _gGolemKnightBishopFeintRoll[((RANDOM_LCG_ADVANCE()) >> 16) & (ARRAY_SIZE(_gGolemKnightBishopFeintRoll) - 1)];
            scratch->operand.vx = 0;
            scratch->operand.vy = (work->targetYaw + GOLEM_KNIGHT_BISHOP_YAW_HALF_TURN) & GOLEM_KNIGHT_BISHOP_YAW_MASK;
            scratch->operand.vz = 0;
            RotMatrix(&scratch->operand, &root->coord);
            root->coord.t[0] = work->targetPos.vx;
            root->coord.t[1] = work->targetPos.vy;
            root->coord.t[2] = work->targetPos.vz;
            if (work->feinting == 1 && work->feintBroken == 0) {
                randomDraw                   = RANDOM_LCG_NEXT(gRandomLcgState);
                appearanceFrames             = ((randomDraw >> 16) & 0xF) + 0xF;
                work->step                   = GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_APPEAR;
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_FEINT_APPEAR;
                gRandomLcgState              = randomDraw;
                work->timer                  = appearanceFrames;
                translucencyFrames           = appearanceFrames * 2 / 3;
                work->translucencyFadeFrames = translucencyFrames;
                work->colorBlendFadeFrames   = appearanceFrames - translucencyFrames;
            } else {
                work->feinting = 0;
                if (work->feintBroken == 0) {
                    framesLeft                   = (((RANDOM_LCG_ADVANCE()) >> 16) & 0xF) + 0x1E;
                    work->step                   = GOLEM_KNIGHT_BISHOP_STRIKE_APPEAR;
                    work->timer                  = framesLeft;
                    translucencyFrames           = framesLeft * 2 / 3;
                    work->translucencyFadeFrames = translucencyFrames;
                    work->colorBlendFadeFrames   = work->timer - translucencyFrames;
                } else {
                    work->step                   = GOLEM_KNIGHT_BISHOP_STRIKE_WAIT;
                    work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
                    work->timer                  = 0;
                    work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
                }
                work->fadeState   = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
                work->appearSound = _gGolemKnightBishopAppearSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(work->appearSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                work->interruptDamage = 0;
                work->reactionLock    = GOLEM_KNIGHT_BISHOP_REACTION_ATTACKING;
            }
            work->auxTimer    = 1;
            work->feintBroken = 0;
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_APPEAR:
            if (work->auxTimer != 0) {
                if (work->hitCooldown == 0) {
                    work->hurtBody.key    = 0;
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                }
                work->auxTimer = 0;
            }
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0 || work->feintBroken != 0) {
                work->step = GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_WAIT;
                RANDOM_LCG_ADVANCE();
                work->timer = (RANDOM_LCG_DRAW() & 0xF) + 0x3C;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_FEINT_WAIT:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0 || work->feintBroken != 0) {
                work->step      = GOLEM_KNIGHT_BISHOP_STRIKE_VANISH;
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHRINK;
                if (work->feintBroken != 0) {
                    work->translucencyFadeFrames = 5;
                    work->colorBlendFadeFrames   = 3;
                    work->timer                  = work->translucencyFadeFrames + work->colorBlendFadeFrames;
                } else {
                    work->translucencyFadeFrames = 8;
                    work->colorBlendFadeFrames   = 8;
                    work->timer                  = work->translucencyFadeFrames + work->colorBlendFadeFrames;
                }
                worldTargetDisableNodeLockOn(&((Enemy*)task->spawnArg2.pointer)->node);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_APPEAR:
            if (work->auxTimer != 0) {
                if (work->hitCooldown == 0) {
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
                }
                work->auxTimer = 0;
            }
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->step = GOLEM_KNIGHT_BISHOP_STRIKE_WAIT;
                RANDOM_LCG_ADVANCE();
                work->timer = RANDOM_LCG_DRAW() & 0xF;
            } else if (work->interruptDamage > 0) {
                work->sequence  = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step      = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
                if (work->appearSound != 0) {
                    sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                    work->appearSound = 0;
                }
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_WAIT:
            if (work->auxTimer != 0) {
                if (work->hitCooldown == 0) {
                    work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
                }
                work->auxTimer = 0;
            }
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->anim  = GOLEM_KNIGHT_BISHOP_ANIM_STRIKE;
                work->step  = GOLEM_KNIGHT_BISHOP_STRIKE_BLOW;
                work->timer = 0;
            } else if (work->interruptDamage > 0) {
                work->sequence  = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                work->step      = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_BLOW:
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_STRIKE_FIRST_FRAME) {
                work->strikeBody.coord  = &task->extra.tmd->coords[8];
                work->strikeBody.pos.vx = 0;
                work->strikeBody.pos.vy = 0;
                work->strikeBody.pos.vz = 0;
                work->strikeBody.radius = 0x12C;
                work->strikeBody.key    = damagePackAttackKey(_gGolemKnightBishopAttacks, GOLEM_KNIGHT_BISHOP_ATTACK_STRIKE);
                work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                strikeSound             = _gGolemKnightBishopStrikeSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(strikeSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            } else if (work->animFrame == GOLEM_KNIGHT_BISHOP_STRIKE_SECOND_FRAME) {
                work->strikeBody.coord = &task->extra.tmd->coords[12];
                strikeSound            = _gGolemKnightBishopStrikeSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(strikeSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_STRIKE_END_FRAME) {
                work->step                   = GOLEM_KNIGHT_BISHOP_STRIKE_VANISH;
                work->timer                  = 0x1E;
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
                work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
                work->strikeBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->vanishSound            = _gGolemKnightBishopVanishSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            break;
        case GOLEM_KNIGHT_BISHOP_STRIKE_VANISH:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step         = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->reactionLock = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GolemKnightBishopOffsetScratch);
}

/// Interrupts box waiting or advance with a flickering recovery and stops movement.
///
/// `work` is the live GOLEM work block whose accumulated damage reached the
/// carrier's interruption threshold. Disables the aim capsule's grid and pair
/// tests while retaining its shape and other flags. Keeps the current animation
/// and reaction lock; recovery sets its clip on the next sequence update.
static inline void _golemKnightBishopInterruptBoxApproach(GolemKnightBishopWork* work)
{
    work->sequence           = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
    work->step               = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
    work->fadeState          = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
    work->forwardSpeed       = 0;
    work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
}

/// Appears at the selected box region's post, aims, advances and charges.
///
/// `boxRegion` must select a live box entry in the task's room-region table.
/// The player leaving its heading corridor or sufficient interrupt damage ends
/// the approach. The charge's speed table must cover every animation frame
/// before the sequence exits; its raw u16 speeds narrow into signed s16 movement.
static void _golemKnightBishopBoxApproachSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_BOX_HEADING_TOLERANCE = 384,
        GOLEM_KNIGHT_BISHOP_CHARGE_CUE_FRAME      = 18,
        GOLEM_KNIGHT_BISHOP_CHARGE_ARM_FRAME      = 20,
        GOLEM_KNIGHT_BISHOP_CHARGE_DISARM_FRAME   = 32,
        GOLEM_KNIGHT_BISHOP_CHARGE_VANISH_FRAME   = 90,
    };
    enum {
        GOLEM_KNIGHT_BISHOP_BOX_APPEAR  = 0,
        GOLEM_KNIGHT_BISHOP_BOX_WAIT    = 1,
        GOLEM_KNIGHT_BISHOP_BOX_ADVANCE = 2,
        GOLEM_KNIGHT_BISHOP_BOX_CHARGE  = 3,
        GOLEM_KNIGHT_BISHOP_BOX_VANISH  = 4,
    };
    GolemKnightBishopOffsetScratch* scratch;
    s32                             step;
    GolemKnightBishopWork*          work;
    GfxCoord*                       root;
    s32                             sound;
    s32                             speedSpanIndex;
    s16                             headingDifference;
    s16                             headingError;
    s32                             advanceSpeed;
    s16                             framesLeft;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GolemKnightBishopOffsetScratch);
    work    = task->work;
    step    = work->step;
    root    = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_BOX_APPEAR:
            root->coord.t[0]    = work->regions[work->boxRegion].x;
            root->coord.t[1]    = gPlayerStatus.coordMtx->t[1];
            root->coord.t[2]    = work->regions[work->boxRegion].z;
            scratch->operand.vx = 0;
            scratch->operand.vy = work->regions[work->boxRegion].param.heading;
            scratch->operand.vz = 0;
            RotMatrix(&scratch->operand, &root->coord);
            scratch->offset.vx = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
            scratch->offset.vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
            if ((s16)SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vz * scratch->offset.vz) < 0xDAC) {
                work->anim  = GOLEM_KNIGHT_BISHOP_ANIM_STAND;
                work->step  = GOLEM_KNIGHT_BISHOP_BOX_WAIT;
                work->timer = 0x1E;
            } else {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_ADVANCE;
                work->step = GOLEM_KNIGHT_BISHOP_BOX_ADVANCE;
            }
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_APPEAR;
            work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
            work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
            work->appearSound            = _gGolemKnightBishopAppearSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            sndEvtRequestScriptStart(work->appearSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            sceneEngageBattle(1);
            if (work->hitCooldown == 0) {
                work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
            }
            work->auxTimer        = GOLEM_KNIGHT_BISHOP_AIM_FRAMES;
            work->interruptDamage = 0;
            work->reactionLock    = GOLEM_KNIGHT_BISHOP_REACTION_ATTACKING;
            break;
        case GOLEM_KNIGHT_BISHOP_BOX_WAIT:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_ADVANCE;
                work->step = GOLEM_KNIGHT_BISHOP_BOX_ADVANCE;
            }
            if (work->auxTimer > 0) {
                work->auxTimer--;
                _golemKnightBishopAimFromPart(task);
            }
            if (work->interruptDamage >= GOLEM_KNIGHT_BISHOP_BOX_INTERRUPT_DAMAGE) {
                _golemKnightBishopInterruptBoxApproach(work);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_BOX_ADVANCE:
            advanceSpeed = 0;
            if (work->animFrame >= 8) {
                advanceSpeed = 0x78;
            }
            work->forwardSpeed = advanceSpeed;
            if (work->auxTimer > 0) {
                work->auxTimer--;
                _golemKnightBishopAimFromPart(task);
            }
            scratch->offset.vx = gPlayerStatus.coordMtx->t[0] - root->coord.t[0];
            scratch->offset.vz = gPlayerStatus.coordMtx->t[2] - root->coord.t[2];
            if ((s16)SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vz * scratch->offset.vz) < 0xA8C) {
                work->anim               = GOLEM_KNIGHT_BISHOP_ANIM_CHARGE;
                work->step               = GOLEM_KNIGHT_BISHOP_BOX_CHARGE;
                work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            } else {
                headingDifference = (ratan2((s16)scratch->offset.vx, (s16)scratch->offset.vz) & GOLEM_KNIGHT_BISHOP_YAW_MASK) - work->regions[work->boxRegion].param.heading;
                headingError      = (abs(headingDifference) >= GOLEM_KNIGHT_BISHOP_YAW_HALF_TURN) ? ((headingDifference > 0) ? GOLEM_KNIGHT_BISHOP_YAW_FULL_TURN - headingDifference : headingDifference + GOLEM_KNIGHT_BISHOP_YAW_FULL_TURN) : abs(headingDifference);
                if (headingError > GOLEM_KNIGHT_BISHOP_BOX_HEADING_TOLERANCE) {
                    work->anim                   = GOLEM_KNIGHT_BISHOP_ANIM_STAND;
                    work->step                   = GOLEM_KNIGHT_BISHOP_BOX_VANISH;
                    work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                    work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
                    work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
                    work->reactionLock           = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
                    work->timer                  = work->translucencyFadeFrames + 0xA;
                    work->aimBeamBody.flags     &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    work->vanishSound            = _gGolemKnightBishopVanishSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
                }
            }
            if (work->interruptDamage >= GOLEM_KNIGHT_BISHOP_BOX_INTERRUPT_DAMAGE) {
                _golemKnightBishopInterruptBoxApproach(work);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_BOX_CHARGE:
            for (speedSpanIndex = 0; work->animFrame > _gGolemKnightBishopChargeSpeedSpans[speedSpanIndex].lastFrame; speedSpanIndex++) {
            }
            work->forwardSpeed = _gGolemKnightBishopChargeSpeedSpans[speedSpanIndex].forwardSpeed;
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_CHARGE_CUE_FRAME) {
                sound = _gGolemKnightBishopStrikeSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_CHARGE_ARM_FRAME) {
                work->strikeBody.coord  = task->extra.tmd->coords;
                work->strikeBody.pos.vy = -0x4B0;
                work->strikeBody.pos.vz = 0x1F4;
                work->strikeBody.pos.vx = 0;
                work->strikeBody.radius = 0x3E8;
                work->strikeBody.key    = damagePackAttackKey(_gGolemKnightBishopAttacks, GOLEM_KNIGHT_BISHOP_ATTACK_CHARGE);
                work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_CHARGE_DISARM_FRAME) {
                work->reactionLock      = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
                work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_CHARGE_VANISH_FRAME) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = GOLEM_KNIGHT_BISHOP_STANDARD_TRANSLUCENCY_FRAMES;
                work->colorBlendFadeFrames   = GOLEM_KNIGHT_BISHOP_STANDARD_COLOR_BLEND_FRAMES;
                work->step                   = GOLEM_KNIGHT_BISHOP_BOX_VANISH;
                work->timer                  = work->translucencyFadeFrames + 0xA;
                work->vanishSound            = _gGolemKnightBishopVanishSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(work->vanishSound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            break;
        case GOLEM_KNIGHT_BISHOP_BOX_VANISH:
            work->forwardSpeed = 0;
            framesLeft         = work->timer - 1;
            work->timer        = framesLeft;
            if (framesLeft <= 0) {
                work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step     = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GolemKnightBishopOffsetScratch);
}

/// Stands exposed for a random recovery interval, then vanishes and idles.
///
/// The interval is the kind's recovery base plus 0..31 frame updates. The hurt
/// sphere is rearmed only when its hit cooldown has expired. The retained
/// eight-byte scratch reservation contains no accessed object; its purpose is
/// unproven.
static void _golemKnightBishopRecoverSeq(Task* task)
{
    enum { GOLEM_KNIGHT_BISHOP_RECOVERY_SCRATCH_BYTES = 8 };
    enum {
        GOLEM_KNIGHT_BISHOP_RECOVER_START = 0,
        GOLEM_KNIGHT_BISHOP_RECOVER_WAIT  = 1,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    audioPan;
    u32                    randomDraw;
    s16                    framesLeft;

    SCRATCH_STACK_RESERVE_BYTES(GOLEM_KNIGHT_BISHOP_RECOVERY_SCRATCH_BYTES);
    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_RECOVER_START:
            work->anim      = GOLEM_KNIGHT_BISHOP_ANIM_RECOVER;
            work->step      = GOLEM_KNIGHT_BISHOP_RECOVER_WAIT;
            randomDraw      = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState = randomDraw;
            work->timer     = ((randomDraw >> 16) & 0x1F) + GOLEM_KNIGHT_BISHOP_RECOVERY_BASE_FRAMES;
            if (work->hitCooldown == 0) {
                work->hurtBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->hurtBody.key    = work->actorId | WORLD_COLLISION_CONTACT_ENEMY_BODY;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_RECOVER_WAIT:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0xA;
                work->sequence               = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                work->step                   = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
                work->colorBlendFadeFrames   = 5;
                work->flickerTimer           = 0;
                work->vanishSound            = _gGolemKnightBishopVanishSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                audioPan                     = (s8)worldCoordGetOriginAudioPan(root);
                sndEvtRequestScriptStart(work->vanishSound, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(GOLEM_KNIGHT_BISHOP_RECOVERY_SCRATCH_BYTES);
}

#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Starts a random flicker half-period and sets both appearance fade divisors.
///
/// `work` retains the new countdown and divisors for the appearance update.
/// Two successive LCG draws choose a 2..17-frame countdown and add 0..15 to
/// produce the shared 2..32-frame fade divisor. No visibility or tint changes here.
static inline void _golemKnightBishopResetFlickerPeriod(GolemKnightBishopWork* work)
{
    enum {
        GOLEM_KNIGHT_BISHOP_FLICKER_RANDOM_MASK = 15,
        GOLEM_KNIGHT_BISHOP_FLICKER_MIN_FRAMES  = 2,
    };
    s16 fadeFrames;
    work->flickerTimer           = (((RANDOM_LCG_ADVANCE()) >> 16) & GOLEM_KNIGHT_BISHOP_FLICKER_RANDOM_MASK) + GOLEM_KNIGHT_BISHOP_FLICKER_MIN_FRAMES;
    fadeFrames                   = work->flickerTimer + (((RANDOM_LCG_ADVANCE()) >> 16) & GOLEM_KNIGHT_BISHOP_FLICKER_RANDOM_MASK);
    work->translucencyFadeFrames = fadeFrames;
    work->colorBlendFadeFrames   = fadeFrames;
}

/// Updates visibility, colour, shadow, feint shrink and hit flicker.
///
/// `task` owns live GOLEM work, model and Enemy spawn data. Fade durations must
/// be positive frame divisors: translucency spans 0..255, colour blend uses Q12
/// and the shadow reaches shade 128. A feint uses 11/16 colour, saves its root
/// matrix and shrinks before hiding. Flicker consumes two LCG draws per half
/// period and emits sparks on odd animation frames. Signed s16 intermediate
/// ramps narrow before their clamps; integer division can extend nominal fades.
static void _golemKnightBishopUpdateAppearance(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_FEINT_COLOR_BLEND          = TMD_OBJECT_COLOR_BLEND_ONE * 11 / 16,
        GOLEM_KNIGHT_BISHOP_FLICKER_TRANSLUCENCY_LIMIT = 128,
        GOLEM_KNIGHT_BISHOP_SHRINK_SQUASH              = 0,
        GOLEM_KNIGHT_BISHOP_SHRINK_NARROW              = 1,
        GOLEM_KNIGHT_BISHOP_SHRINK_HELD                = 2,
        GOLEM_KNIGHT_BISHOP_TINT_NONE                  = 0,
        GOLEM_KNIGHT_BISHOP_TINT_DIM_BLUE              = 1,
    };
    SVECTOR*               sparkOffset;
    GolemKnightBishopWork* work;
    TmdObject*             model;
    GfxCoord*              root;
    s32                    vanishCue;
    s32                    audioPan;
    s32                    nextWidthScale;
    s32                    nextHeightScale;
    s32                    squashedHeight;
    s32                    heightScale;
    s16                    nextTranslucency;
    s16                    nextShadowShade;
    s16                    flickerFramesLeft;

    sparkOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work        = task->work;
    model       = task->extra.tmd;
    root        = model->coords;
    switch (work->fadeState) {
        case GOLEM_KNIGHT_BISHOP_FADE_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->shadowShade      = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            work->hurtBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if (work->appearSound != 0) {
                sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->appearSound = 0;
            }
            if (work->vanishSound != 0) {
                sndEvtRequestScriptStop(work->vanishSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->vanishSound = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_APPEAR:
            model->shading.colorBlend += TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
            if (model->shading.colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE) {
                model->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
                nextTranslucency          = work->translucency - GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
                work->translucency        = nextTranslucency;
                if (nextTranslucency <= 0) {
                    work->translucency = 0;
                    work->fadeState    = GOLEM_KNIGHT_BISHOP_FADE_SHOWN;
                    if (work->appearSound != 0) {
                        sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->appearSound = 0;
                    }
                }
            }
            nextShadowShade   = work->shadowShade + GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE / work->translucencyFadeFrames;
            work->shadowShade = nextShadowShade;
            if (nextShadowShade >= GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE) {
                work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_SHOWN:
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE;
            if (work->appearSound != 0) {
                sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->appearSound = 0;
            }
            if (work->vanishSound != 0) {
                sndEvtRequestScriptStop(work->vanishSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->vanishSound = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_VANISH:
            nextTranslucency   = work->translucency + GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
            work->translucency = nextTranslucency;
            if (nextTranslucency >= GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE) {
                work->translucency         = GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE;
                model->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
                if (model->shading.colorBlend <= 0) {
                    model->shading.colorBlend = 0;
                    work->fadeState           = GOLEM_KNIGHT_BISHOP_FADE_HIDDEN;
                    if (work->vanishSound != 0) {
                        sndEvtRequestScriptStop(work->vanishSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->vanishSound = 0;
                    }
                    vanishCue = _gGolemKnightBishopVanishEndSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    audioPan  = (s8)worldCoordGetOriginAudioPan(root);
                    sndEvtRequestScriptStart(vanishCue, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
                }
            }
            nextShadowShade   = work->shadowShade - GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE / work->translucencyFadeFrames;
            work->shadowShade = nextShadowShade;
            if (nextShadowShade < 0) {
                work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FEINT_APPEAR:
            model->shading.colorBlend += (GOLEM_KNIGHT_BISHOP_FEINT_COLOR_BLEND) / work->colorBlendFadeFrames;
            if (model->shading.colorBlend >= GOLEM_KNIGHT_BISHOP_FEINT_COLOR_BLEND) {
                model->shading.colorBlend = GOLEM_KNIGHT_BISHOP_FEINT_COLOR_BLEND;
                nextTranslucency          = work->translucency - GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
                work->translucency        = nextTranslucency;
                if (nextTranslucency <= 0) {
                    work->fadeState       = GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHOWN;
                    work->translucency    = 0;
                    work->scale.vx        = 0x1000;
                    work->scale.vy        = 0x1000;
                    work->scale.vz        = 0x1000;
                    work->unscaledRootMtx = task->extra.tmd->coords[0].coord;
                    work->shrinkStep      = GOLEM_KNIGHT_BISHOP_SHRINK_SQUASH;
                }
            }
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHOWN:
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHRINK:
            work->hurtBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            switch (work->shrinkStep) {
                case GOLEM_KNIGHT_BISHOP_SHRINK_SQUASH:
                    heightScale = work->scale.vy;
                    if (work->feintBroken != 0) {
                        squashedHeight = heightScale - (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 4);
                    } else {
                        squashedHeight = heightScale - (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 8);
                    }
                    work->scale.vy = squashedHeight;
                    if (squashedHeight <= (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 2)) {
                        work->shrinkStep = GOLEM_KNIGHT_BISHOP_SHRINK_NARROW;
                    }
                    break;
                case GOLEM_KNIGHT_BISHOP_SHRINK_NARROW:
                    if (work->feintBroken != 0) {
                        nextWidthScale  = work->scale.vx - (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 8);
                        nextHeightScale = work->scale.vy + (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 4);
                        work->scale.vx  = nextWidthScale;
                        work->scale.vy  = nextHeightScale;
                    } else {
                        nextWidthScale  = work->scale.vx - GOLEM_KNIGHT_BISHOP_SCALE_ONE / 16;
                        nextHeightScale = work->scale.vy + (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 8);
                        work->scale.vx  = nextWidthScale;
                        work->scale.vy  = nextHeightScale;
                    }
                    if (work->scale.vx <= (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 2)) {
                        work->shrinkStep = GOLEM_KNIGHT_BISHOP_SHRINK_HELD;
                    }
                    break;
            }
            _golemKnightBishopApplyScale(task);
            nextTranslucency   = work->translucency + GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
            work->translucency = nextTranslucency;
            if (nextTranslucency >= GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE) {
                work->translucency         = GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE;
                model->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
                if (model->shading.colorBlend <= 0) {
                    model->shading.colorBlend = 0;
                    work->fadeState           = GOLEM_KNIGHT_BISHOP_FADE_HIDDEN;
                    gfxSetRotIdentity(&task->extra.tmd->coords[0].coord);
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                }
            }
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START:
            work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM;
            _golemKnightBishopResetFlickerPeriod(work);
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM:
            nextTranslucency   = work->translucency + GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
            work->translucency = nextTranslucency;
            if (nextTranslucency >= GOLEM_KNIGHT_BISHOP_FLICKER_TRANSLUCENCY_LIMIT) {
                work->translucency         = GOLEM_KNIGHT_BISHOP_FLICKER_TRANSLUCENCY_LIMIT;
                model->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
                if (model->shading.colorBlend <= TMD_OBJECT_COLOR_BLEND_ONE / 2) {
                    model->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE / 2;
                }
            }
            nextShadowShade   = work->shadowShade - GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE / work->translucencyFadeFrames;
            work->shadowShade = nextShadowShade;
            if (nextShadowShade < 0) {
                work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            }
            flickerFramesLeft  = work->flickerTimer - 1;
            work->flickerTimer = flickerFramesLeft;
            if (flickerFramesLeft <= 0) {
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_BRIGHT;
                _golemKnightBishopResetFlickerPeriod(work);
            }
            if (work->tintRequest == GOLEM_KNIGHT_BISHOP_TINT_NONE) {
                work->tintRequest = GOLEM_KNIGHT_BISHOP_TINT_DIM_BLUE;
            }
            if (work->animFrame & 1) {
                sparkOffset->vx = 0;
                sparkOffset->vy = -(((RANDOM_LCG_ADVANCE()) >> 16) & 0xFF);
                sparkOffset->vz = ((RANDOM_LCG_ADVANCE()) >> 16) & 0xFF;
                effectSpawn(EFFECT_FLASH_BURST, &task->extra.tmd->coords[3], 0x100, sparkOffset);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FLICKER_BRIGHT:
            model->shading.colorBlend += TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
            if (model->shading.colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE) {
                model->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
                nextTranslucency          = work->translucency - GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
                work->translucency        = nextTranslucency;
                if (nextTranslucency <= 0) {
                    work->translucency = 0;
                }
            }
            nextShadowShade   = work->shadowShade + GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE / work->translucencyFadeFrames;
            work->shadowShade = nextShadowShade;
            if (nextShadowShade >= GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE) {
                work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE;
            }
            flickerFramesLeft  = work->flickerTimer - 1;
            work->flickerTimer = flickerFramesLeft;
            if (flickerFramesLeft <= 0) {
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM;
                _golemKnightBishopResetFlickerPeriod(work);
            }
            if (work->animFrame & 1) {
                sparkOffset->vx = 0;
                sparkOffset->vy = -(((RANDOM_LCG_ADVANCE()) >> 16) & 0xFF);
                sparkOffset->vz = ((RANDOM_LCG_ADVANCE()) >> 16) & 0xFF;
                effectSpawn(EFFECT_FLASH_BURST, &task->extra.tmd->coords[3], 0x100, sparkOffset);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Plays the light hit reaction, then idles or recovers.
///
/// The reaction finishes at animation frame 55. A non-flicker hit vanishes during
/// the clip; a flicker hit advances its flicker stage and enters exposed recovery.
static void _golemKnightBishopLightFlinchSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_START = 0,
        GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_WAIT  = 1,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    audioPan;

    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_START:
            work->anim         = GOLEM_KNIGHT_BISHOP_ANIM_LIGHT_FLINCH;
            work->step         = GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_WAIT;
            work->forwardSpeed = 0;
            if (work->flickerStage != GOLEM_KNIGHT_BISHOP_FLICKER_HIT) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0x1E;
                work->colorBlendFadeFrames   = 0xF;
                work->vanishSound            = _gGolemKnightBishopVanishSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                audioPan                     = (s8)worldCoordGetOriginAudioPan(root);
                sndEvtRequestScriptStart(work->vanishSound, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
                break;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_LIGHT_FLINCH_WAIT:
            if (work->animFrame >= 0x37) {
                if (work->flickerStage == step) {
                    work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                    work->flickerStage = GOLEM_KNIGHT_BISHOP_FLICKER_RECOVERED;
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                }
                work->step = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
    }
}

/// Plays the side-dependent heavy hit reaction, then idles or recovers.
///
/// A non-flicker hit vanishes during its reaction; a flicker hit remains exposed
/// and advances the flicker stage on completion. Front and rear clips finish
/// at animation frames 80 and 59 respectively.
static void _golemKnightBishopHeavyFlinchSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_START  = 0,
        GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_FRONT  = 1,
        GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_BEHIND = 2,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    audioPan;

    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_START:
            if (work->hitFromFront == 1) {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_FRONT;
                work->step = GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_FRONT;
            } else {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_HEAVY_FLINCH_BEHIND;
                work->step = GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_BEHIND;
            }
            work->forwardSpeed = 0;
            if (work->flickerStage != GOLEM_KNIGHT_BISHOP_FLICKER_HIT) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_VANISH;
                work->translucencyFadeFrames = 0x1E;
                work->colorBlendFadeFrames   = 0xF;
                work->vanishSound            = _gGolemKnightBishopVanishSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                audioPan                     = (s8)worldCoordGetOriginAudioPan(root);
                sndEvtRequestScriptStart(work->vanishSound, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
                break;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_FRONT:
            if (work->animFrame >= 0x50) {
                if (work->flickerStage == step) {
                    work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                    work->flickerStage = GOLEM_KNIGHT_BISHOP_FLICKER_RECOVERED;
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                }
                work->step = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_HEAVY_FLINCH_BEHIND:
            if (work->animFrame >= 0x3B) {
                if (work->flickerStage == GOLEM_KNIGHT_BISHOP_FLICKER_HIT) {
                    work->sequence     = GOLEM_KNIGHT_BISHOP_SEQUENCE_RECOVER;
                    work->flickerStage = step;
                } else {
                    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_IDLE;
                }
                work->step = GOLEM_KNIGHT_BISHOP_SEQUENCE_START;
            }
            break;
    }
}

#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Ends a knockdown fall with an unlocked lying pose and a random rest countdown.
///
/// `lyingAnimation` is the rear or front lying clip of the live GOLEM rig.
/// One LCG draw sets the stored countdown to 0..63 frames; the rest sequence
/// decrements before testing, so both 0 and 1 advance on its next update.
static inline void _golemKnightBishopFinishKnockdownFall(GolemKnightBishopWork* work, s16 lyingAnimation)
{
    enum {
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_STEP       = 3,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_MAX_FRAMES = 63,
    };
    u32 randomDraw;
    work->anim         = lyingAnimation;
    work->step         = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_STEP;
    work->reactionLock = GOLEM_KNIGHT_BISHOP_REACTION_UNLOCKED;
    randomDraw         = RANDOM_LCG_NEXT(gRandomLcgState);
    gRandomLcgState    = randomDraw;
    work->timer        = (randomDraw >> 16) & GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_MAX_FRAMES;
}

/// Falls at low HP, then alternates the retained lying and writhing poses.
///
/// Requires live GOLEM work and model coordinates. The hit side chooses the
/// fall and hurt-sphere offset; grid participation moves off the root sphere.
/// The fall lock prevents reactions until the lying pose starts. Rest draws
/// 0..63 frames, with decrement before testing: stored 0 and 1 both leave rest
/// on its next update. Writhing lasts ten updates before another rest draw.
static void _golemKnightBishopKnockdownSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_START       = 0,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_BEHIND = 1,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_FRONT  = 2,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST        = 3,
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_WRITHE      = 4,
    };
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    s32                    step;
    s32                    sound;
    s32                    downedAnimation;
    u32                    randomDraw;
    s16                    framesLeft;

    work = task->work;
    step = work->step;
    root = task->extra.tmd->coords;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_START:
            if (work->hitFromFront == 0) {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_FALL_BEHIND;
                work->step            = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_BEHIND;
                work->downedPose      = GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND;
                work->hurtBody.pos.vz = GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND_OFFSET_Z;
            } else {
                work->anim            = GOLEM_KNIGHT_BISHOP_ANIM_FALL_FRONT;
                work->step            = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_FRONT;
                work->downedPose      = GOLEM_KNIGHT_BISHOP_DOWNED_FRONT;
                work->hurtBody.pos.vz = GOLEM_KNIGHT_BISHOP_DOWNED_FRONT_OFFSET_Z;
            }
            work->hurtBody.radius   = GOLEM_KNIGHT_BISHOP_HURT_RADIUS;
            work->knockdownStage    = GOLEM_KNIGHT_BISHOP_FALL_STARTED;
            work->fadeState         = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START;
            work->reactionLock      = GOLEM_KNIGHT_BISHOP_REACTION_FALLING;
            work->forwardSpeed      = 0;
            work->hurtBody.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->groundBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_BEHIND:
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_FALL_BEHIND_IMPACT_FRAME) {
                sound = _gGolemKnightBishopAnimSounds[work->soundSet + 8] | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_FALL_BEHIND_FRAMES) {
                _golemKnightBishopFinishKnockdownFall(work, GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND);
            }
            if (work->knockdownStage == GOLEM_KNIGHT_BISHOP_FALL_STARTED) {
                work->knockdownStage = GOLEM_KNIGHT_BISHOP_FALL_SETTLED;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_FALL_FRONT:
            if (work->animFrame == GOLEM_KNIGHT_BISHOP_FALL_FRONT_IMPACT_FRAME) {
                sound = _gGolemKnightBishopAnimSounds[work->soundSet + 8] | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                sndEvtRequestScriptStart(sound, (s8)worldCoordGetOriginAudioPan(root), (s8)worldCoordGetOriginAudioDepth(root));
            }
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_FALL_FRONT_FRAMES) {
                _golemKnightBishopFinishKnockdownFall(work, GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT);
            }
            if (work->knockdownStage == GOLEM_KNIGHT_BISHOP_FALL_STARTED) {
                work->knockdownStage = GOLEM_KNIGHT_BISHOP_FALL_SETTLED;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                downedAnimation = GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_FRONT;
                if (work->downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                    downedAnimation = GOLEM_KNIGHT_BISHOP_ANIM_WRITHE_BEHIND;
                }
                work->timer = 0xA;
                work->anim  = downedAnimation;
                work->step  = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_WRITHE;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_KNOCKDOWN_WRITHE:
            framesLeft  = work->timer - 1;
            work->timer = framesLeft;
            if (framesLeft <= 0) {
                downedAnimation = GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT;
                if (work->downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                    downedAnimation = GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND;
                }
                work->anim      = downedAnimation;
                work->step      = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST;
                randomDraw      = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState = randomDraw;
                work->timer     = (randomDraw >> 16) & 0x3F;
            }
            break;
    }
}

#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Resumes the downed lying pose with a freshly rolled rest countdown.
///
/// Requires live GOLEM work and the lying clip matching its retained downed
/// side. Selects knockdown rest step 3 and consumes one LCG draw for a timer
/// in 0..63 updates; the rest step decrements before testing, so 0 and 1 both
/// leave the lying pose on its next update. Animation playback follows in
/// the caller's frame update; this helper does not reset playback slots.
static inline void _golemKnightBishopResumeDownedRest(GolemKnightBishopWork* work, s16 lyingAnimation)
{
    enum {
        GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_STEP = 3,
        GOLEM_KNIGHT_BISHOP_DOWNED_DWELL_MASK   = 63,
    };
    work->anim     = lyingAnimation;
    work->sequence = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL;
    work->step     = GOLEM_KNIGHT_BISHOP_KNOCKDOWN_REST_STEP;
    RANDOM_LCG_ADVANCE();
    work->timer = RANDOM_LCG_DRAW() & GOLEM_KNIGHT_BISHOP_DOWNED_DWELL_MASK;
}

/// Flinches from a hit while downed, then resumes the corresponding lying pose.
///
/// `task` owns a live GOLEM work block. The saved pose selects a 16- or 22-frame
/// reaction; completion returns to the knockdown rest step with a new random
/// dwell of 0..63 frames.
static void _golemKnightBishopDownedHitSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_START  = 0,
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_BEHIND = 1,
        GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT  = 2,
        GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND        = 0x10,
        GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT         = 0x14,
    };
    GolemKnightBishopWork* work;
    s16                    step;
    s32                    downedPose;

    work = task->work;
    step = work->step;
    switch (step) {
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_START:
            downedPose = work->downedPose;
            if (downedPose == GOLEM_KNIGHT_BISHOP_DOWNED_BEHIND) {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_BEHIND;
                work->step = downedPose;
            } else {
                work->anim = GOLEM_KNIGHT_BISHOP_ANIM_DOWNED_HIT_FRONT;
                work->step = GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_BEHIND:
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_DOWNED_HIT_BEHIND_FRAMES) {
                _golemKnightBishopResumeDownedRest(work, GOLEM_KNIGHT_BISHOP_ANIM_LIE_BEHIND);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_DOWNED_REACTION_FRONT:
            if (work->animFrame >= GOLEM_KNIGHT_BISHOP_DOWNED_HIT_FRONT_FRAMES) {
                _golemKnightBishopResumeDownedRest(work, GOLEM_KNIGHT_BISHOP_ANIM_LIE_FRONT);
            }
            break;
    }
}
