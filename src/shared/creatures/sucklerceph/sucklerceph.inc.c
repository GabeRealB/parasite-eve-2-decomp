#include "main/random.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Schedules the dormant Sucklerceph's next idle sound and advances its countdown.
///
/// Requires the task's live work, enemy and root coordinate and a loaded sound
/// bank. Each request draws the next delay from 80..179 frames and uses the
/// enemy's placement index as its sound instance tag; request failure is ignored.
static inline void _sucklercephTickIdleSound(const Task* task, const GfxCoord* rootCoord, SucklercephWork* work)
{
    enum {
        SUCKLERCEPH_IDLE_SOUND_MIN_FRAMES    = 80,
        SUCKLERCEPH_IDLE_SOUND_FRAME_CHOICES = 100,
        SUCKLERCEPH_IDLE_SOUND               = 0x402E0001,
        SUCKLERCEPH_VARIANT_IDLE_SOUND       = 0x40460009,
        SUCKLERCEPH_SOUND_INSTANCE_SHIFT     = 8
    };
    s32    soundId;
    u32    randomState;
    Enemy* enemy;
    /// Requests the chosen idle script at the enemy's current audio position.
    ///
    /// Block statement macro capturing task, rootCoord, enemy and soundId.
    /// Requires stable live pointers and SUCKLERCEPH_SOUND_INSTANCE_SHIFT.
    /// Evaluates baseSoundId once; narrows pan/depth to s8 and ignores the
    /// request result. Undefined after this function.
#define SUCKLERCEPH_REQUEST_IDLE_SOUND(baseSoundId)                                                                                  \
    {                                                                                                                                \
        enemy   = task->spawnArg2.pointer;                                                                                           \
        soundId = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_SOUND_INSTANCE_SHIFT) | (baseSoundId);                \
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord)); \
    }
    work->idleSoundFrames--;
    if (work->idleSoundFrames <= 0) {
        randomState           = RANDOM_LCG_NEXT(gRandomLcgState);
        gRandomLcgState       = randomState;
        work->idleSoundFrames = (randomState >> 16) % SUCKLERCEPH_IDLE_SOUND_FRAME_CHOICES + SUCKLERCEPH_IDLE_SOUND_MIN_FRAMES;
        if (work->variant != 0) {
            SUCKLERCEPH_REQUEST_IDLE_SOUND(SUCKLERCEPH_VARIANT_IDLE_SOUND);
        } else {
            SUCKLERCEPH_REQUEST_IDLE_SOUND(SUCKLERCEPH_IDLE_SOUND);
        }
    }
}

#undef SUCKLERCEPH_REQUEST_IDLE_SOUND

/// Rocks an idle Sucklerceph until player contact or damage requests waking.
///
/// Requires an initialized root, work and owning enemy. Waking selects crawl,
/// disables the sense sphere's pair tests and engages the scene battle; sense
/// contacts are consumed on every call. Under the idle animation, sound repeats
/// after 80..179 frames, the root moves forward during frames 1..41 and backward
/// during 51..91 at 20 units per frame, and the cycle resets at frame 99.
/// Holds eight untouched scratch bytes across the whole update; their role
/// is unproven. Collision correction and animation playback follow in the caller.
static void _sucklercephDormantTick(Task* task)
{
    enum {
        SUCKLERCEPH_DORMANT_SCRATCH_BYTES = 8,
        SUCKLERCEPH_ROCK_SPEED            = 20,
        SUCKLERCEPH_ROCK_WINDOW_FRAMES    = 41,
        SUCKLERCEPH_ROCK_BACKWARD_START   = 51,
        SUCKLERCEPH_ROCK_CYCLE_FRAMES     = 99
    };

    SucklercephWork* work;
    GfxCoord*        rootCoord;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    SCRATCH_STACK_RESERVE_BYTES(SUCKLERCEPH_DORMANT_SCRATCH_BYTES);
    if (worldCollisionCountContactsByKind(&work->senseContact, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        work->wakeRequested = 1;
    }
    if (work->wakeRequested != 0) {
        work->state           = SUCKLERCEPH_STATE_AWAKE;
        work->awakeStage      = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
        work->senseBody.flags = (u16)(work->senseBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
        sceneEngageBattle(1);
    }
    worldCollisionClearContacts(&work->senseContact);
    if (work->animId == SUCKLERCEPH_ANIM_IDLE) {
        _sucklercephTickIdleSound(task, rootCoord, work);
        work->field_2C6    = 1;
        work->forwardSpeed = 0;
        if ((u32)(work->animFrames - 1) < SUCKLERCEPH_ROCK_WINDOW_FRAMES) {
            work->forwardSpeed = SUCKLERCEPH_ROCK_SPEED;
        }
        if ((u32)(work->animFrames - SUCKLERCEPH_ROCK_BACKWARD_START) < SUCKLERCEPH_ROCK_WINDOW_FRAMES) {
            work->forwardSpeed = -SUCKLERCEPH_ROCK_SPEED;
        }
        if ((s16)work->animFrames >= SUCKLERCEPH_ROCK_CYCLE_FRAMES) {
            work->animFrames = 0;
        }
        _sucklercephStep(task);
    }
    SCRATCH_STACK_RELEASE_BYTES(SUCKLERCEPH_DORMANT_SCRATCH_BYTES);
}

#include "main/random.h"
#include "main/sound.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Sound banks of the default specimen and its nonzero spawn-argument variant.
enum { SUCKLERCEPH_SOUND_BANK_DEFAULT = 0x2E,
       SUCKLERCEPH_SOUND_BANK_VARIANT = 0x46 };

/// Advances crawling or the five-tick swelling that starts the specimen's death.
///
/// Requires the live task, work, Enemy and composed root. Crawling turns toward
/// the player, steps 20 parent-frame units and wraps its animation counter at 29.
/// Idle sounds redraw an 80..179-frame delay from the shared LCG. Swelling adds
/// 200 Q12 scale units per call, then chooses burst/slump death and clears HP.
/// Contact correction and animation playback follow in the update handler.
static void _sucklercephAwakeTick(Task* task)
{
    SucklercephWork* work;
    Enemy*           enemy;
    GfxCoord*        rootCoord;
    s16              awakeStage;
    enum { SUCKLERCEPH_CRAWL_SPEED        = 20,
           SUCKLERCEPH_CRAWL_CYCLE_FRAMES = 29 };

    rootCoord  = task->extra.tmd->coords;
    work       = task->work;
    enemy      = task->spawnArg2.pointer;
    awakeStage = work->awakeStage;
    switch (awakeStage) {
        case SUCKLERCEPH_AWAKE_STAGE_CRAWL:
            _sucklercephTickIdleSound(task, rootCoord, work);
            work->forwardSpeed = SUCKLERCEPH_CRAWL_SPEED;
            work->animId       = SUCKLERCEPH_ANIM_CRAWL;
            _sucklercephTurnToPlayer(task);
            _sucklercephStep(task);
            if ((s16)work->animFrames >= SUCKLERCEPH_CRAWL_CYCLE_FRAMES) {
                work->animFrames = 0;
            }
            break;
        case SUCKLERCEPH_AWAKE_STAGE_SWELL:
            work->swellFrames++;
            work->swellScale += SUCKLERCEPH_SWELL_SCALE_STEP_Q12;
            if (work->swellFrames >= SUCKLERCEPH_SWELL_DURATION_FRAMES) {
                _sucklercephKill(task, 0);
                task->killCountdown = SUCKLERCEPH_DEATH_COUNTDOWN_FRAMES;
                work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                task->state         = awakeStage;
                enemy->hp           = 0;
            }
            break;
    }
}

#include "gameplay/room_effects.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Resolves grid/body separation, player proximity and attacks, then consumes contacts.
///
/// Requires live task/work/Enemy storage, composed root and grid-view matrices,
/// and one free `SucklercephContactsScratch` block (76 bytes), plus callee scratch.
/// Grid correction adds Q16.16 integer halves or restores the previous step's
/// position. Player contact or XZ range below 800 starts swelling while crawling.
/// All four contacts are examined: critical attacks force a burst, ordinary
/// attacks apply HP/status/effects/cooldown, and enemy bodies separate the root
/// by their overlap in the grid's parent frame. Only negative Y body corrections apply.
/// Attack-body contact disables its pair tests once awake. No pointer is retained.
static void _sucklercephContacts(Task* task)
{
    enum {
        SUCKLERCEPH_SWELL_PLAYER_RANGE           = 800,
        SUCKLERCEPH_REACTION_LOW_MASK            = 0xFFFF,
        SUCKLERCEPH_REACTION_BUILDUP_ALTERNATE   = 9,
        SUCKLERCEPH_CONTACT_NORMAL_FRACTION_BITS = 12,
        SUCKLERCEPH_CRITICAL_DEATH_HP            = -1
    };
    Enemy*                      enemy;
    WorldCollisionContact*      attackContact;
    s32                         attackReaction;
    s32                         verticalPushQ12;
    s32                         gridResponse;
    s32                         playerDx;
    s32                         playerDz;
    s32                         bodyDx;
    s32                         bodyDz;
    s16                         attackCooldownFrames;
    s32                         rangeOrOverlap;
    u32                         attackDamage;
    SucklercephWork*            work;
    GfxCoord*                   rootCoord;
    SucklercephContactsScratch* scratch;
    s32                         contactIndex;

    work         = task->work;
    scratch      = SCRATCH_STACK_RESERVE_BLOCK(SucklercephContactsScratch);
    rootCoord    = task->extra.tmd->coords;
    enemy        = task->spawnArg2.pointer;
    gridResponse = worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), &scratch->gridKeyMask);
    // Correct the last root step before measuring proximity and hit range.
    switch (gridResponse) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            rootCoord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevRootPos.vx;
            rootCoord->coord.t[1] = work->prevRootPos.vy;
            rootCoord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    if (work->hitCooldown != 0) {
        work->hitCooldown--;
        if (work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    playerDx                 = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    scratch->delta.vector.vx = playerDx;
    scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    playerDz                 = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    scratch->delta.vector.vz = playerDz;
    rangeOrOverlap           = SquareRoot0((playerDx * playerDx) + (playerDz * playerDz));
    if (rangeOrOverlap < SUCKLERCEPH_SWELL_PLAYER_RANGE && work->awakeStage == SUCKLERCEPH_AWAKE_STAGE_CRAWL) {
        work->animFrozen = 1;
        work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_SWELL;
    }
    for (contactIndex = 0; contactIndex < ARRAY_SIZE(work->contacts); contactIndex++) {
        switch (work->contacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) {
            case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                if (work->awakeStage == SUCKLERCEPH_AWAKE_STAGE_CRAWL) {
                    work->animFrozen = 1;
                    work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_SWELL;
                }
                break;
            case WORLD_COLLISION_CONTACT_ATTACK:
                if (work->hitCooldown == 0) {
                    attackDamage = damageComputePlayerAttack(work->contacts[contactIndex].key.value, rangeOrOverlap, 0, 0);
                    if (damageRollCriticalHit(task->spawnArg2.pointer, work->contacts[contactIndex].key.value, 0) != 0) {
                        _sucklercephKill(task, 1);
                        task->killCountdown = SUCKLERCEPH_DEATH_COUNTDOWN_FRAMES;
                        task->state         = SUCKLERCEPH_TASK_DEATH;
                        work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                        enemy->hp           = SUCKLERCEPH_CRITICAL_DEATH_HP;
                    } else {
                        damageAccumulateLifeDrainHp(enemy, work->contacts[contactIndex].key.value, attackDamage, 0);
                        _sucklercephTakeDamage(task, attackDamage);
                        attackReaction = damageGetPlayerAttackReaction(work->contacts[contactIndex].key.value) & SUCKLERCEPH_REACTION_LOW_MASK;
                        switch (attackReaction) {
                            case DAMAGE_PLAYER_REACTION_STAGGER:
                                work->animFrozen = 1;
                                work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_SWELL;
                                break;
                            case DAMAGE_PLAYER_REACTION_POISON:
                                damageTryStartEnemyDamageOverTime(enemy, work->contacts[contactIndex].key.value, 0);
                                break;
                            case DAMAGE_PLAYER_REACTION_BUILDUP:
                            case SUCKLERCEPH_REACTION_BUILDUP_ALTERNATE:
                                damageStartEnemyBuildup(enemy, work->contacts[contactIndex].key.value, 0);
                                break;
                        }
                        if (enemy->hp > 0) {
                            effectSpawnHit(damageGetPlayerAttackEffectId(work->contacts[contactIndex].key.value), task->extra.tmd->coords + 1, NULL, &work->hitEffectArg);
                        }
                        attackCooldownFrames = damageGetPlayerAttackHitCooldown(work->contacts[contactIndex].key.value);
                        if (attackCooldownFrames > 0) {
                            work->hitCooldown = attackCooldownFrames;
                        }
                    }
                }
                break;
            case WORLD_COLLISION_CONTACT_ENEMY_BODY:
                // Contact points are view-space body centres, not wall normals.
                // Keep the overwritten range for any later attack in this table.
                bodyDx                   = rootCoord->workm.t[0] - work->contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = 0;
                scratch->delta.vector.vx = bodyDx;
                bodyDz                   = rootCoord->workm.t[2] - work->contacts[contactIndex].point.vz;
                scratch->delta.vector.vz = bodyDz;
                rangeOrOverlap           = SquareRoot0((bodyDx * bodyDx) + (bodyDz * bodyDz));
                rangeOrOverlap           = work->contacts[contactIndex].distance - rangeOrOverlap;
                rangeOrOverlap           = (rangeOrOverlap <= 0) ? 0 : rangeOrOverlap;
                scratch->delta.vector.vx = rootCoord->workm.t[0] - work->contacts[contactIndex].point.vx;
                scratch->delta.vector.vy = rootCoord->workm.t[1] - work->contacts[contactIndex].point.vy;
                scratch->delta.vector.vz = rootCoord->workm.t[2] - work->contacts[contactIndex].point.vz;
                VectorNormal(&scratch->delta.vector, &scratch->normal);
                ApplyTransposeMatrixLV(&gWorldCollisionActiveGrid->viewCoord->workm, &scratch->normal, &scratch->delta.vector);
                if (work->animId == SUCKLERCEPH_ANIM_IDLE || work->animId == SUCKLERCEPH_ANIM_CRAWL) {
                    rootCoord->coord.t[0] += (rangeOrOverlap * scratch->delta.vector.vx) >> SUCKLERCEPH_CONTACT_NORMAL_FRACTION_BITS;
                    verticalPushQ12        = rangeOrOverlap * scratch->delta.vector.vy;
                    if (verticalPushQ12 < 0) {
                        rootCoord->coord.t[1] += verticalPushQ12 >> SUCKLERCEPH_CONTACT_NORMAL_FRACTION_BITS;
                    }
                    rootCoord->coord.t[2] += (rangeOrOverlap * scratch->delta.vector.vz) >> SUCKLERCEPH_CONTACT_NORMAL_FRACTION_BITS;
                }
                break;
        }
    }
    worldCollisionClearContacts(work->contacts);
    attackContact = &work->attackContact;
    if ((work->awakeStage != SUCKLERCEPH_AWAKE_STAGE_NONE) && (worldCollisionFindContactIndex(attackContact, WORLD_COLLISION_FIND_ANY_KEY) != 0)) {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        worldCollisionClearContacts(attackContact);
    }
    SCRATCH_STACK_RELEASE_BLOCK(SucklercephContactsScratch);
}

/// Requests a positioned character script with its placement instance tag.
///
/// Compound statement for a braced branch. Supply a live Enemy pointer or
/// generic task slot in enemyArg, a composed root and a modifiable s32 idLocal.
/// enemyArg/baseSoundId evaluate once; rootCoord twice and idLocal twice, so
/// use stable expressions without side effects. The read-only Enemy view
/// extracts the placement nibble into sound ID bits 8..15. Pan/depth narrow
/// to s8 and request failure is ignored. Include this before the death-state
/// fragment, which shares the helper and undefines it after its definition.
#define SUCKLERCEPH_REQUEST_POSITIONED_SOUND(enemyArg, rootCoord, baseSoundId, idLocal)                                                                 \
    {                                                                                                                                                   \
        enum { SUCKLERCEPH_POSITIONED_SOUND_INSTANCE_SHIFT = 8 };                                                                                       \
        (idLocal) = ((((const Enemy*)(enemyArg))->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_POSITIONED_SOUND_INSTANCE_SHIFT) | (baseSoundId); \
        sndEvtRequestScriptStart((idLocal), (s8)worldCoordGetOriginAudioPan((rootCoord)), (s8)worldCoordGetOriginAudioDepth((rootCoord)));              \
    }

/// Subtracts HP and adds its readout, starting death only when HP becomes negative.
///
/// Requires a live task, Enemy, model root and work; damage is an HP amount.
/// Zero remaining HP still plays the hurt cue and can request waking from idle.
/// A lethal call chooses burst/slump and arms the five-tick death countdown.
/// Surviving calls also store 15 in an unread work halfword whose role is unproven.
static void _sucklercephTakeDamage(Task* task, s32 damage)
{
    SucklercephWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    s32              animId;
    s32              soundId;
    enum { SUCKLERCEPH_SURVIVED_HIT_VALUE = 15 };

    enemy      = task->spawnArg2.pointer;
    model      = task->extra.tmd;
    rootCoord  = model->coords;
    work       = task->work;
    enemy->hp -= damage;
    worldTargetAddReadoutAmount(&enemy->node, damage, 0);
    if (enemy->hp < 0) {
        _sucklercephKill(task, 0);
        task->state         = SUCKLERCEPH_TASK_DEATH;
        task->killCountdown = SUCKLERCEPH_DEATH_COUNTDOWN_FRAMES;
        work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
        return;
    }
    if (work->variant != 0) {
        SUCKLERCEPH_REQUEST_POSITIONED_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_SOUND_BANK_VARIANT, 10), soundId);
    } else {
        SUCKLERCEPH_REQUEST_POSITIONED_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_SOUND_BANK_DEFAULT, 2), soundId);
    }
    animId          = work->animId;
    work->field_2CC = SUCKLERCEPH_SURVIVED_HIT_VALUE;
    if (animId == SUCKLERCEPH_ANIM_IDLE) {
        work->wakeRequested = animId;
    }
}

/// Turns the crawling Sucklerceph toward the player by up to 32 angle units.
///
/// Requires live root/work storage and the player's root in the same parent
/// frame. Bearing uses the low signed halfwords of the X/Z offset, with 4096
/// angle units per turn. Deltas of at most 32 snap to the bearing; larger
/// deltas step the stored signed-halfword heading, retaining its wrap behavior.
/// Replaces pitch, roll and scale with the resulting yaw while keeping translation.
/// Releases one `ActorFaceScratch` block; the caller invalidates composition.
static void _sucklercephTurnToPlayer(Task* task)
{
    enum { SUCKLERCEPH_TURN_STEP = 32 };

    SucklercephWork*  work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* turnScratch;
    s16               normalizedHeading;
    s16               nextHeading;
    s32               playerHeading;
    s16               headingDelta;
    s32               absoluteDelta;
    s16               turnDelta;
    s16               wrappedDelta;
    s32               heading;

    rootCoord             = task->extra.tmd->coords;
    work                  = task->work;
    turnScratch           = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    turnScratch->delta.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    turnScratch->delta.vy = 0;
    turnScratch->delta.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    playerHeading         = ratan2((s16)turnScratch->delta.vx, (s16)turnScratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
    normalizedHeading     = work->heading & ACTOR_TRANSFORM_ANGLE_MASK;
    headingDelta          = playerHeading - normalizedHeading;
    absoluteDelta         = headingDelta >= 0 ? headingDelta : -headingDelta;
    turnDelta             = headingDelta;
    if (absoluteDelta < SUCKLERCEPH_TURN_STEP + 1) {
        work->heading = playerHeading;
    } else {
        if (absoluteDelta >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
            // Retain the asymmetric negative-delta wrap: turn selection uses its sign.
            wrappedDelta = headingDelta - ACTOR_TRANSFORM_ANGLE_TURN;
            if (headingDelta <= 0) {
                wrappedDelta = ACTOR_TRANSFORM_ANGLE_TURN - headingDelta;
            }
            turnDelta = wrappedDelta;
        }
        heading = work->heading;
        if (turnDelta <= 0) {
            nextHeading = heading - SUCKLERCEPH_TURN_STEP;
        } else {
            nextHeading = heading + SUCKLERCEPH_TURN_STEP;
        }
        work->heading = nextHeading;
    }
    turnScratch->rot.vx = 0;
    turnScratch->rot.vy = work->heading;
    turnScratch->rot.vz = 0;
    RotMatrix(&turnScratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Finishes the death countdown, detaches combat records and expires the corpse.
///
/// Borrows the owning Enemy and live model task. PAUSED does nothing; HIDDEN
/// sets draw suppression and unlockability without progressing death. Running
/// countdown disables the blast sphere and reduces swelling; expiry plays its
/// cue, credits rewards, saves the root and unlinks the target and four bodies.
/// Puffing/slump deaths flatten before lingering; other deaths linger after one
/// flatten tick. The shared counter's expiry threshold is 61; a flatten phase
/// reaching that threshold is destroyed on the next LINGER call. That call
/// destroys task/model/work through `enemyDestroy` and returns.
/// Other running phases refresh part 1 before sampling its cached matrix for
/// lighting, except puffing/slump deaths. No separate shadow draw occurs here.
static void _sucklercephDeathState(Enemy* enemy, Task* task)
{
    TmdObject*       model;
    SucklercephWork* work;
    GfxCoord*        rootCoord;
    s32              soundId;
    enum {
        SUCKLERCEPH_DEATH_SCALE_DECAY_Q12 = 300,
        SUCKLERCEPH_DEATH_HIDE_COUNTDOWN  = 3,
        SUCKLERCEPH_DEATH_LIFETIME_FRAMES = 61,
        SUCKLERCEPH_DEATH_FADE_FRAME      = 10,
        SUCKLERCEPH_DEATH_REWARD_ARG      = 0x2E // Unused compatibility argument; rewards come from enemy parameters.
    };

    rootCoord = task->extra.tmd->coords;
    work      = task->work;
    model     = task->extra.tmd;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            switch (work->deathPhase) {
                case SUCKLERCEPH_DEATH_PHASE_COUNTDOWN:
                    // Stop further blast contacts while the visible death reaction settles.
                    work->blastBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    work->swellScale      -= SUCKLERCEPH_DEATH_SCALE_DECAY_Q12;
                    task->killCountdown--;
                    if (work->state != SUCKLERCEPH_STATE_PUFFING_DEATH && work->state != SUCKLERCEPH_STATE_SLUMP_DEATH && task->killCountdown == SUCKLERCEPH_DEATH_HIDE_COUNTDOWN) {
                        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    }
                    if (work->state == SUCKLERCEPH_STATE_SLUMP_DEATH) {
                        work->animId = SUCKLERCEPH_ANIM_IDLE;
                        _sucklercephTickAnim(task);
                    }
                    if (task->killCountdown <= 0) {
                        if (work->variant != 0) {
                            SUCKLERCEPH_REQUEST_POSITIONED_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_SOUND_BANK_VARIANT, 13), soundId);
                        } else {
                            SUCKLERCEPH_REQUEST_POSITIONED_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_SOUND_BANK_DEFAULT, 5), soundId);
                        }
                        task->killCountdown = 0;
                        sceneReleaseBattleRefWithRewards(task, SUCKLERCEPH_DEATH_REWARD_ARG);
                        if (work->hasBurst != 0) {
                            effectSpawn(EFFECT_RED_GROUND_GLOW, task->extra.tmd->coords, 0, NULL);
                        }
                        work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_FLATTEN;
                        work->deathFrames   = 0;
                        work->flattenScaleY = ONE;
                        work->savedRootMtx  = rootCoord->coord;
                        // The corpse retains its model; combat registration ends now.
                        enemy->recs = NULL;
                        worldTargetUnlinkNode(&enemy->node);
                        worldCollisionUnlinkBody(&work->senseBody);
                        worldCollisionUnlinkBody(&work->body);
                        worldCollisionUnlinkBody(&work->attackBody);
                        worldCollisionUnlinkBody(&work->blastBody);
                    }
                    break;
                case SUCKLERCEPH_DEATH_PHASE_FLATTEN:
                    if (work->state != SUCKLERCEPH_STATE_PUFFING_DEATH && work->state != SUCKLERCEPH_STATE_SLUMP_DEATH) {
                        work->deathPhase = SUCKLERCEPH_DEATH_PHASE_LINGER;
                    }
                    work->deathFrames++;
                    if (work->deathFrames >= SUCKLERCEPH_DEATH_LIFETIME_FRAMES) {
                        work->deathPhase = SUCKLERCEPH_DEATH_PHASE_LINGER;
                    }
                    _sucklercephFlatten(task);
                    if (work->deathFrames == SUCKLERCEPH_DEATH_FADE_FRAME) {
                        task->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    }
                    break;
                case SUCKLERCEPH_DEATH_PHASE_LINGER:
                    work->deathFrames++;
                    if (work->deathFrames >= SUCKLERCEPH_DEATH_LIFETIME_FRAMES) {
                        enemyDestroy(enemy, task);
                    }
                    return;
            }
            if (work->state != SUCKLERCEPH_STATE_PUFFING_DEATH && work->state != SUCKLERCEPH_STATE_SLUMP_DEATH) {
                _sucklercephTickAnim(task);
                _sucklercephScalePart(task, &task->extra.tmd->coords[1]);
                task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
                task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(&task->extra.tmd->coords[1]);
                _actorRenderUpdateCoordColor(enemy, &task->extra.tmd->coords[1]);
            }
            break;
    }
}

#undef SUCKLERCEPH_REQUEST_POSITIONED_SOUND

#include "main/random.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

#ifndef SUCKLERCEPH_BURST_PARTICLE_OFFSET
/// Binds the burst particle's model-root-local displacement in game units.
///
/// Supply a live word-aligned SVECTOR lvalue before including this fragment;
/// its address is passed once to effectSpawn, which copies it during placement.
/// Both carriers default to their private (0,-10,0) vector. It need only survive
/// the spawn call. This binding is undefined after the fragment.
#define SUCKLERCEPH_BURST_PARTICLE_OFFSET _gSucklercephBurstParticleOffset
#endif
#ifndef SUCKLERCEPH_BURST_PAD_SCRIPT
/// Binds the burst's two-lane PadScriptCmd command array.
///
/// Supply the array or a pointer to its first command before this inclusion.
/// Both lanes must eventually STOP and every reachable PLAY operand must index
/// SUCKLERCEPH_BURST_VIBRATION_SEGMENTS. Both carriers default to a private
/// three-command array. The task borrows it, unmodified, until teardown.
/// Evaluated once at spawn and undefined after the fragment.
#define SUCKLERCEPH_BURST_PAD_SCRIPT _gSucklercephBurstPadScript
#endif
#ifndef SUCKLERCEPH_BURST_VIBRATION_SEGMENTS
/// Binds the PadScriptVibrationSegment table used by the burst commands.
///
/// Supply a readable array or first-entry pointer covering every PLAY index in
/// SUCKLERCEPH_BURST_PAD_SCRIPT. Each record supplies byte intensities and a
/// duration in script ticks; both carriers default to three records for hold,
/// full intensity and fade. The task borrows this table unmodified until
/// teardown. Evaluated once at spawn and undefined after the fragment.
#define SUCKLERCEPH_BURST_VIBRATION_SEGMENTS _gSucklercephBurstVibrationSegments
#endif

/// Clears the Sucklerceph's HP and chooses its burst or slump death.
///
/// Requires live Sucklerceph work, enemy and model. A nonzero forceBurst selects
/// the burst; otherwise one random bit selects it. Both paths consume one draw.
/// The burst arms its two damage spheres, spawns its visual and vibration
/// effects and latches hasBurst; the slump changes the behavior state. The
/// caller starts the task's death state and countdown; storage stays live.
static void _sucklercephKill(Task* task, u8 forceBurst)
{
    enum { SUCKLERCEPH_DEFAULT_SOUND_BANK         = 0x2E,
           SUCKLERCEPH_DEATH_SOUND_INSTANCE_SHIFT = 8,
           SUCKLERCEPH_BURST_ROLL_BIT             = 2,
           SUCKLERCEPH_BURST_FLASH_STYLE          = 1,
           SUCKLERCEPH_BURST_PARTICLE_SIZE        = 768 };
    /// Queues a placement-scoped death cue from a live enemy argument and root.
    ///
    /// enemyArg and baseId are evaluated once, rootCoord twice and idLocal
    /// twice. Expands a compound statement; use within a braced branch.
    /// Supply stable pointers and a modifiable s32 local without side
    /// effects. Pan/depth retain their signed-byte conversion. The read-only
    /// Enemy view reloads the generic spawn slot at the sound site.
    /// The placement nibble occupies the sound ID's instance byte. Uses the
    /// containing function's SUCKLERCEPH_DEATH_SOUND_INSTANCE_SHIFT constant.
#define SUCKLERCEPH_REQUEST_DEATH_SOUND(enemyArg, rootCoord, baseId, idLocal)                                                                 \
    {                                                                                                                                         \
        (idLocal) = ((((const Enemy*)(enemyArg))->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_DEATH_SOUND_INSTANCE_SHIFT) | (baseId); \
        sndEvtRequestScriptStart((idLocal), (s8)worldCoordGetOriginAudioPan((rootCoord)), (s8)worldCoordGetOriginAudioDepth((rootCoord)));    \
    }
    SucklercephWork* work;
    Enemy*           enemy;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    s32              soundId;

    model     = task->extra.tmd;
    enemy     = task->spawnArg2.pointer;
    work      = task->work;
    rootCoord = model->coords;
    enemy->hp = 0;
    // Consume the death draw even when the caller forces a burst.
    RANDOM_LCG_ADVANCE();
    if ((RANDOM_LCG_DRAW() & SUCKLERCEPH_BURST_ROLL_BIT) || forceBurst) {
        if (work->variant != 0) {
            SUCKLERCEPH_REQUEST_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 0x0B), soundId);
        } else {
            SUCKLERCEPH_REQUEST_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_DEFAULT_SOUND_BANK, 3), soundId);
        }
        work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->blastBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        effectSpawn(EFFECT_CRITICAL_HIT, task->extra.tmd->coords, SUCKLERCEPH_BURST_FLASH_STYLE, NULL);
        effectSpawn(EFFECT_030, task->extra.tmd->coords, SUCKLERCEPH_BURST_PARTICLE_SIZE, &SUCKLERCEPH_BURST_PARTICLE_OFFSET);
        padScriptSpawn(SUCKLERCEPH_BURST_PAD_SCRIPT, SUCKLERCEPH_BURST_VIBRATION_SEGMENTS);
        work->hasBurst = 1;
    } else {
        if (work->variant != 0) {
            SUCKLERCEPH_REQUEST_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SOUND_BANK_SUCKLERCEPH, 0x0C), soundId);
        } else {
            SUCKLERCEPH_REQUEST_DEATH_SOUND(task->spawnArg2.pointer, rootCoord, SOUND_CHARACTER(SUCKLERCEPH_DEFAULT_SOUND_BANK, 4), soundId);
        }
        work->state = SUCKLERCEPH_STATE_SLUMP_DEATH;
    }
#undef SUCKLERCEPH_REQUEST_DEATH_SOUND
}

#undef SUCKLERCEPH_BURST_PARTICLE_OFFSET
#undef SUCKLERCEPH_BURST_PAD_SCRIPT
#undef SUCKLERCEPH_BURST_VIBRATION_SEGMENTS

/// Initializes a hidden Sucklerceph that waits for a room command to begin dropping.
///
/// Requires the owning enemy and a live three-part TMD model. A signed high
/// spawn halfword of 1 or work-allocation failure destroys the enemy and task.
/// The low halfword selects the sound variant; the retained high halfword's
/// further role is unproven. On success the task owns zeroed work, acquires a
/// battle hold and enters drop task state 3 with idle tracks 1 and 2.
/// The model borrows the work's matrices; the enemy borrows part 1 and contacts.
/// All four spheres are linked with pair tests disabled, and the ordinary body
/// also has floor/grid queries disabled. The room message table later reveals
/// and arms the drop. Common task teardown releases the work.
static void _sucklercephDropSpawnState(Enemy* enemy, Task* task)
{
    enum {
        SUCKLERCEPH_DROP_SPAWN_CANCELLED   = 1,
        SUCKLERCEPH_DROP_SENSE_RADIUS      = 3000,
        SUCKLERCEPH_DROP_BODY_RADIUS       = 200,
        SUCKLERCEPH_DROP_BURST_RADIUS      = 1000,
        SUCKLERCEPH_DROP_BODY_KEY          = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x2E,
        SUCKLERCEPH_DROP_BLAST_KEY         = 0x22323,
        SUCKLERCEPH_DROP_HIT_EFFECT_ARG_LO = 256,
        SUCKLERCEPH_DROP_HIT_EFFECT_ARG_HI = 1,
        SUCKLERCEPH_DROP_TASK_WAIT         = 3
    };
    SucklercephWork* work;
    GfxCoord*        rootCoord;
    GfxCoord*        bodyCoord;
    TmdObject*       model;
    s32              slotIndex;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    bodyCoord = &rootCoord[1];
    if ((s16)(task->spawnArg1.value >> 16) == SUCKLERCEPH_DROP_SPAWN_CANCELLED) {
        enemyDestroy(enemy, task);
        return;
    }
    work = memCalloc(sizeof(SucklercephWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Attach work before publishing the pointers the model and enemy borrow.
    task->work              = work;
    work->field_2DC         = task->spawnArg1.value >> 16;
    work->variant           = task->spawnArg1.value;
    model->flags            = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord[1].coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = bodyCoord;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &_gSucklercephParams;
    enemy->recs                   = work->contacts;
    enemy->hp                     = _gSucklercephParams.hpMax;
    animationInitContext(&work->anim, _gSucklercephAnimSets, model, work->poses, work->slots);
    slotIndex = 1;
    do {
        animationResetSlot(&work->anim, slotIndex, SUCKLERCEPH_ANIM_IDLE);
        slotIndex += 1;
    } while (slotIndex < ARRAY_SIZE(work->slots));
    sceneAcquireBattleRef(0);
    work->animId                  = SUCKLERCEPH_ANIM_IDLE;
    work->appliedAnim             = SUCKLERCEPH_ANIM_IDLE;
    work->swellScale              = ONE;
    work->hasBurst                = 0;
    work->hitCooldown             = 0;
    work->swellFrames             = 0;
    work->animFrozen              = 0;
    work->field_2CC               = 0;
    task->killCountdown           = 0;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[1];
    work->hitEffectArg.spawnArgLo = SUCKLERCEPH_DROP_HIT_EFFECT_ARG_LO;
    work->hitEffectArg.spawnArgHi = SUCKLERCEPH_DROP_HIT_EFFECT_ARG_HI;
    // Link sensing, body and burst spheres without enabling their passes.
    /// Initializes one unlinked drop sphere and its borrowed coordinate/contact pointers.
    ///
    /// `work` is a live SucklercephWork* evaluated eight times; `member` is its
    /// WorldCollisionBody field name. Other arguments are evaluated once and
    /// must be stable and independent of the destination. `root` and
    /// `contactTable` remain borrowed until unlink/teardown; the caller links
    /// the body and initializes that table afterwards. Centre Y narrows to s16,
    /// radius to u16, both in root-local coordinate units; key is a packed s32.
    /// Leaves pos.pad untouched and replaces flags with sphere kind only,
    /// disabling every collision pass. Captures no locals. One statement.
#define SUCKLERCEPH_INIT_DROP_SPHERE(work, member, root, contactTable, centreY, sphereRadius, contactKey) \
    (void)({                                                                                              \
        (work)->member.coord            = (root);                                                         \
        (work)->member.context.contacts = (contactTable);                                                 \
        (work)->member.pos.vx           = 0;                                                              \
        (work)->member.pos.vy           = (centreY);                                                      \
        (work)->member.pos.vz           = 0;                                                              \
        (work)->member.key              = (contactKey);                                                   \
        (work)->member.radius           = (sphereRadius);                                                 \
        (work)->member.flags            = WORLD_COLLISION_BODY_SPHERE;                                    \
    })
    SUCKLERCEPH_INIT_DROP_SPHERE(work, senseBody, rootCoord, &work->senseContact, 0, SUCKLERCEPH_DROP_SENSE_RADIUS, 0);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(&work->senseContact, 1, 0);
    SUCKLERCEPH_INIT_DROP_SPHERE(work, body, rootCoord, work->contacts, -SUCKLERCEPH_DROP_BODY_RADIUS, SUCKLERCEPH_DROP_BODY_RADIUS, SUCKLERCEPH_DROP_BODY_KEY);
    work->senseBody.flags = (u16)(work->senseBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->attackBody.coord            = rootCoord;
    work->attackBody.context.contacts = &work->attackContact;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->body.flags                  = (u16)(work->body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
    work->attackBody.key              = damagePackAttackKey(&_gSucklercephBurstAttack, 0);
    work->attackBody.radius           = SUCKLERCEPH_DROP_BURST_RADIUS;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(&work->attackContact, 1, 0);
    SUCKLERCEPH_INIT_DROP_SPHERE(work, blastBody, rootCoord, &work->blastContact, 0, SUCKLERCEPH_DROP_BURST_RADIUS, SUCKLERCEPH_DROP_BLAST_KEY);
    work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, &work->blastBody);
    worldCollisionInitContacts(&work->blastContact, 1, 0);
#undef SUCKLERCEPH_INIT_DROP_SPHERE

    work->dropArmed       = 0;
    work->blastBody.flags = (u16)(work->blastBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    task->msgTable        = _gSucklercephDropMsgTable;
    task->state           = SUCKLERCEPH_DROP_TASK_WAIT;
}
