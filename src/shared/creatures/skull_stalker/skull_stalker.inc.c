#include "main/random.h"

/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Creates the animation, lighting and collision state of a hidden Skull Stalker.
///
/// Entry requires a live enemy and a three-part TMD task at the spawn state.
/// Allocation failure destroys both; success gives the task ownership of a
/// zeroed work block, links its target and three collision bodies, acquires a
/// battle reference and advances to active state with a hidden idle animation.
/// Placement mode selects the sound variant; mode 1 on a TMD body also advances
/// the texture page and CLUT row and rebuilds both existing buffer halves.
/// The carrier's animation table must provide entries 0..2 (0 unused).
static void _skullStalkerSpawnState(Enemy* enemy, Task* task)
{
    SkullStalkerWork* work;
    enum { SKULL_STALKER_BODY_CONTACT_KEY = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x2F };
    TmdObject*             model;
    GfxCoord*              rootCoord;
    GfxCoord*              targetCoord;
    u32                    nextRandomState;
    WorldCollisionContact* frontSenseContacts;
    WorldCollisionContact* senseContacts;
    WorldCollisionContact* bodyContacts;
    s32                    slotIndex;

    model       = task->extra.tmd;
    rootCoord   = model->coords;
    targetCoord = &rootCoord[1];
    work        = memCalloc(sizeof(SkullStalkerWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord[1].coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = targetCoord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &_gSkullStalkerParams;
    enemy->recs                   = work->bodyContacts;
    enemy->hp                     = _gSkullStalkerParams.hpMax;
    // Bind animation and start fully hidden before collision can sense the player.
    animationInitContext(&work->anim, _gSkullStalkerAnimSets, model, work->poses, work->slots);
    slotIndex = 1;
    do {
        animationResetSlot(&work->anim, slotIndex, SKULL_STALKER_ANIM_IDLE);
        slotIndex += 1;
    } while (slotIndex < ARRAY_SIZE(work->slots));
    (sceneAcquireBattleRef)(0);
    work->animId                  = SKULL_STALKER_ANIM_IDLE;
    work->appliedAnim             = SKULL_STALKER_ANIM_IDLE;
    work->hiding                  = 1;
    work->fadeFrames              = SKULL_STALKER_FADE_FRAMES;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    nextRandomState               = RANDOM_LCG_NEXT(gRandomLcgState);
    work->fadeWaitFrames          = ((nextRandomState >> 16) & SKULL_STALKER_HIDDEN_WAIT_JITTER_MASK) + SKULL_STALKER_HIDDEN_WAIT_BASE;
    gRandomLcgState               = nextRandomState;
    worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    // The forward capsule and root sphere sense the player; the small body receives hits.
    work->frontSenseCapsule.ends[0].vz   = 5000;
    work->frontSenseCapsule.end0Radius   = 4000;
    work->frontSenseCapsule.end1Radius   = 2000;
    frontSenseContacts                   = work->frontSenseContacts;
    work->frontSenseCapsule.contacts     = frontSenseContacts;
    work->frontSenseBody.context.capsule = &work->frontSenseCapsule;
    work->frontSenseBody.coord           = rootCoord;
    work->frontSenseBody.pos.vx          = 0;
    work->frontSenseBody.pos.vy          = 0;
    work->frontSenseBody.pos.vz          = 0;
    work->frontSenseBody.key             = 0;
    work->frontSenseBody.radius          = 0;
    work->frontSenseBody.flags           = WORLD_COLLISION_BODY_CAPSULE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->frontSenseBody);
    worldCollisionInitContacts(frontSenseContacts, ARRAY_SIZE(work->frontSenseContacts), 0);
    work->senseBody.coord            = rootCoord;
    senseContacts                    = work->senseContacts;
    work->senseBody.context.contacts = senseContacts;
    work->senseBody.pos.vx           = 0;
    work->senseBody.pos.vy           = 0;
    work->senseBody.pos.vz           = 0;
    work->senseBody.key              = 0;
    work->senseBody.radius           = 2000;
    work->senseBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->frontSenseBody.flags       = work->frontSenseBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(senseContacts, ARRAY_SIZE(work->senseContacts), 0);
    bodyContacts                = work->bodyContacts;
    work->body.coord            = rootCoord;
    work->body.context.contacts = bodyContacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -200;
    work->body.pos.vz           = 0;
    work->body.key              = SKULL_STALKER_BODY_CONTACT_KEY;
    work->body.radius           = 200;
    work->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags       = work->senseBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(bodyContacts, ARRAY_SIZE(work->bodyContacts), 0);
    work->body.flags = work->body.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->variant    = enemy->place->mode;
    if (work->variant == 1 && task->bodyKind == work->variant) {
        model->texturePageOffset++;
        model->clutRowOffset++;
        if (model->buffer != NULL) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    task->exitCallback = _skullStalkerExit;
    task->state++;
}

#include "main/random.h"

/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Alternates hidden and visible waits until sensing selects the alert animation.
///
/// Player-body contacts raise the scene alert, reveal a fully hidden enemy and
/// request a cry and battle engagement. Alert animation repeats its cry every
/// 40 animation ticks. Nonzero placement variants select the alternate sound
/// bank; placement index identifies the sound instance. Requires a composed
/// root for spatial audio and initialized, LAST-terminated sensing tables.
static void _skullStalkerIdleTick(Task* task)
{
    enum {
        SKULL_STALKER_SOUND_APPEAR         = 0x402E0006,
        SKULL_STALKER_SOUND_APPEAR_VARIANT = 0x40480007,
        SKULL_STALKER_SOUND_ALERT          = 0x402E0007,
        SKULL_STALKER_SOUND_ALERT_VARIANT  = 0x40480008,
        SKULL_STALKER_CRY_INTERVAL_TICKS   = 40,
        SKULL_STALKER_IDLE_SCRATCH_BYTES   = 8
    };
    SkullStalkerWork* work;
    GfxCoord*         rootCoord;
    s32               soundId;
    s16               animId;
    s32               soundScript;
    Enemy*            enemy;

    /// Queues one placement-tagged spatial sound.
    ///
    /// Captures task, rootCoord, enemy, soundScript and soundId; the last three
    /// locals are overwritten. scriptId is evaluated once. Use only within a
    /// braced compound statement, with a live task and composed root coordinate.
#define SKULL_STALKER_PLAY_IDLE_SOUND(scriptId)                                      \
    enemy       = task->spawnArg2.pointer;                                           \
    soundScript = (scriptId);                                                        \
    soundId     = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | soundScript; \
    sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord))

    work = task->work;
    // This tick reserves an extra scratch frame whose contents are never accessed.
    SCRATCH_STACK_RESERVE_BYTES(SKULL_STALKER_IDLE_SCRATCH_BYTES);
    rootCoord = task->extra.tmd->coords;
    if (worldCollisionCountContactsByKind(work->senseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0 || worldCollisionCountContactsByKind(work->frontSenseContacts, WORLD_COLLISION_CONTACT_PLAYER_BODY) != 0) {
        gSceneCombatState.signals.bytes.enemyAlert = 1;
        work->alertRequested                       = 1;
        work->animId                               = SKULL_STALKER_ANIM_ALERT;
        if (work->hiding != 0 && work->fadeFrames == SKULL_STALKER_FADE_FRAMES) {
            if (work->variant != 0) {
                SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_APPEAR_VARIANT);
            } else {
                SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_APPEAR);
            }
            work->animFrames = 0;
            work->hiding     = 0;
            RANDOM_LCG_ADVANCE();
            work->fadeWaitFrames = (RANDOM_LCG_DRAW() & SKULL_STALKER_VISIBLE_WAIT_JITTER_MASK) + SKULL_STALKER_VISIBLE_WAIT_BASE;
        }
    }
    if (work->alertRequested != 0) {
        if (work->variant != 0) {
            SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_ALERT_VARIANT);
        } else {
            SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_ALERT);
        }
        work->senseBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->frontSenseBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        sceneEngageBattle(1);
    }
    // Only the sphere table is cleared. A front-capsule contact stays occupied
    // after pair testing is disabled and can request the alert again next tick.
    worldCollisionClearContacts(work->senseContacts);
    animId = work->animId;
    if (animId == SKULL_STALKER_ANIM_IDLE) {
        work->field_29A = 1;
        work->field_292 = 0;
        if (work->animFrames > work->fadeWaitFrames) {
            if (work->hiding != 0 && work->fadeFrames == SKULL_STALKER_FADE_FRAMES) {
                if (work->variant != 0) {
                    SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_APPEAR_VARIANT);
                } else {
                    SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_APPEAR);
                }
                work->animFrames = 0;
                work->hiding     = 0;
                RANDOM_LCG_ADVANCE();
                work->fadeWaitFrames = (RANDOM_LCG_DRAW() & SKULL_STALKER_VISIBLE_WAIT_JITTER_MASK) + SKULL_STALKER_VISIBLE_WAIT_BASE;
            } else if (work->fadeFrames == 0 && work->hiding == 0) {
                work->animFrames = 0;
                work->hiding     = 1;
                RANDOM_LCG_ADVANCE();
                work->fadeWaitFrames = (RANDOM_LCG_DRAW() & SKULL_STALKER_HIDDEN_WAIT_JITTER_MASK) + SKULL_STALKER_HIDDEN_WAIT_BASE;
            }
        }
    } else if (animId == SKULL_STALKER_ANIM_ALERT) {
        work->alertRequested = 0;
        if (work->animFrames >= SKULL_STALKER_CRY_INTERVAL_TICKS) {
            if (work->variant != 0) {
                SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_ALERT_VARIANT);
            } else {
                SKULL_STALKER_PLAY_IDLE_SOUND(SKULL_STALKER_SOUND_ALERT);
            }
            work->animFrames = 0;
        }
    }
    SCRATCH_STACK_RELEASE_BYTES(SKULL_STALKER_IDLE_SCRATCH_BYTES);
#undef SKULL_STALKER_PLAY_IDLE_SOUND
}

/// Spawns the two death particles and their ground glow at the model root.
///
/// The particle argument seeds the size of secondary effects. Requires a live
/// root for placement and the carrier's loaded effect resources. Each spawn
/// may fail independently; offsets remain in the carrier's static storage.
static inline void _skullStalkerSpawnDeathEffects(const Task* task)
{
    enum { SKULL_STALKER_PARTICLE_SECONDARY_SIZE = 0x200 };
    effectSpawn(EFFECT_030, task->extra.tmd->coords, SKULL_STALKER_PARTICLE_SECONDARY_SIZE, &_gSkullStalkerDeathParticleOffset);
    effectSpawn(EFFECT_030, task->extra.tmd->coords, SKULL_STALKER_PARTICLE_SECONDARY_SIZE, &_gSkullStalkerDeathParticleOffset);
    effectSpawn(EFFECT_RED_GROUND_GLOW, task->extra.tmd->coords, 0, &_gSkullStalkerDeathGlowOffset);
}

/// Resolves root pushback and consumes the four body contacts.
///
/// Player-body contact or any nonzero attack damage hides and kills the enemy
/// immediately; the death state's countdown delays collision detachment by five
/// ticks. Zero-damage attacks can start buildup or request hiding instead.
/// Requires live model/work/enemy storage and an initialized scratch stack.
/// All contacts are processed, including later entries after a kill.
static void _skullStalkerProcessContacts(Task* task)
{
    enum {
        SKULL_STALKER_SOUND_HIT                 = 0x402E0008,
        SKULL_STALKER_SOUND_HIT_VARIANT         = 0x40480009,
        SKULL_STALKER_KILL_COUNTDOWN_TICKS      = 5,
        SKULL_STALKER_TOUCH_SCALE_Y_Q12         = 0x500,
        SKULL_STALKER_ATTACK_HIDE               = 8,
        SKULL_STALKER_ATTACK_BUILDUP            = 9,
        SKULL_STALKER_TOUCH_VIBRATION_TICKS     = 10,
        SKULL_STALKER_TOUCH_VIBRATION_INTENSITY = 96
    };
    SkullStalkerWork*         work;
    ActorContactDeltaScratch* scratch;
    ActorContactDeltaScratch* scratchHead;
    TmdObject*                model;
    GfxCoord*                 rootCoord;
    Enemy*                    enemy;
    s32                       contactIndex;
    s32                       variantHitScript;
    s32                       defaultHitScript;
    u32                       damage;
    s32                       soundId;

    /// Queues a contact sound tagged with the enemy's placement instance.
    ///
    /// `scriptId` is an s32 packed bank/entry ID evaluated once, with zero
    /// instance bits 8..15. Captures `task`, `rootCoord` and writable `soundId`;
    /// the task must borrow a live Enemy and the root must be composed in the
    /// current view. The placement index supplies the instance byte. Audio pan
    /// and attenuation narrow to signed bytes; their query order is unspecified
    /// by C. Admission failure is discarded; the bank must remain loaded through
    /// playback. The block-local `soundEnemy` is borrowed. One statement.
#define SKULL_STALKER_PLAY_CONTACT_SOUND(scriptId)                                                                                   \
    do {                                                                                                                             \
        enum { SKULL_STALKER_SOUND_INSTANCE_SHIFT = 8 };                                                                             \
        Enemy* soundEnemy = task->spawnArg2.pointer;                                                                                 \
        soundId           = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SKULL_STALKER_SOUND_INSTANCE_SHIFT) | (scriptId);  \
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord)); \
    } while (0)

    work                                           = task->work;
    scratchHead                                    = SCRATCH_STACK_CURSOR(ActorContactDeltaScratch);
    SCRATCH_STACK_CURSOR(ActorContactDeltaScratch) = scratchHead - 1;
    scratch                                        = scratchHead - 1;
    model                                          = task->extra.tmd;
    rootCoord                                      = model->coords;
    enemy                                          = task->spawnArg2.pointer;

    // Grid corrections use Q16.16; only their integer halves move the root.
    switch (worldCollisionResolvePushback(work->bodyContacts, &scratchHead[-1].delta, ARRAY_SIZE(work->bodyContacts), NULL)) {
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
    contactIndex     = 0;
    variantHitScript = SKULL_STALKER_SOUND_HIT_VARIANT;
    defaultHitScript = SKULL_STALKER_SOUND_HIT;
    do {
        switch (work->bodyContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) {
            case WORLD_COLLISION_CONTACT_PLAYER_BODY:
                if (work->variant != 0) {
                    SKULL_STALKER_PLAY_CONTACT_SOUND(variantHitScript);
                } else {
                    SKULL_STALKER_PLAY_CONTACT_SOUND(defaultHitScript);
                }
                _skullStalkerSpawnDeathEffects(task);
                padScriptSpawnVariableMotorRamp(SKULL_STALKER_TOUCH_VIBRATION_TICKS, SKULL_STALKER_TOUCH_VIBRATION_INTENSITY, SKULL_STALKER_TOUCH_VIBRATION_INTENSITY);
                model->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                work->flattenScaleY = SKULL_STALKER_TOUCH_SCALE_Y_Q12;
                work->animId        = SKULL_STALKER_ANIM_IDLE;
                enemy->hp           = 0;
                work->hiding        = 1;
                task->killCountdown = SKULL_STALKER_KILL_COUNTDOWN_TICKS;
                task->state         = SKULL_STALKER_TASK_DEATH;
                break;
            case WORLD_COLLISION_CONTACT_ATTACK:
                scratch->delta.vector.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
                scratch->delta.vector.vy = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
                scratch->delta.vector.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
                damage                   = damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value,
                                                                     SquareRoot0(scratch->delta.vector.vx * scratch->delta.vector.vx +
                                                                                 scratch->delta.vector.vy * scratch->delta.vector.vy +
                                                                                 scratch->delta.vector.vz * scratch->delta.vector.vz),
                                                                     0, 0);
                if (damageRollCriticalHit(task->spawnArg2.pointer, work->bodyContacts[contactIndex].key.value, 0) != 0) {
                    damage *= 4;
                }
                damageAccumulateLifeDrainHp(enemy, work->bodyContacts[contactIndex].key.value, damage, 0);
                worldTargetAddReadoutAmount(&enemy->node, damage, 0);
                if (damage != 0) {
                    if (work->variant != 0) {
                        SKULL_STALKER_PLAY_CONTACT_SOUND(variantHitScript);
                    } else {
                        SKULL_STALKER_PLAY_CONTACT_SOUND(defaultHitScript);
                    }
                    _skullStalkerSpawnDeathEffects(task);
                    model->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->flattenScaleY = ONE;
                    work->hiding        = 1;
                    work->animId        = SKULL_STALKER_ANIM_IDLE;
                    enemy->hp           = 0;
                    task->killCountdown = SKULL_STALKER_KILL_COUNTDOWN_TICKS;
                    task->state         = SKULL_STALKER_TASK_DEATH;
                    break;
                }
                switch ((u16)damageGetPlayerAttackReaction(work->bodyContacts[contactIndex].key.value)) {
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                    case SKULL_STALKER_ATTACK_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->bodyContacts[contactIndex].key.value, 0);
                        break;
                    case SKULL_STALKER_ATTACK_HIDE:
                        work->hiding = 1;
                        break;
                }
                break;
        }
        contactIndex++;
    } while (contactIndex < ARRAY_SIZE(work->bodyContacts));
    worldCollisionClearContacts(work->bodyContacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaScratch);
#undef SKULL_STALKER_PLAY_CONTACT_SOUND
}

/// Rebinds animation requests or advances slots 1 and 2 of the model.
///
/// Requires initialized work and an animation id of idle or alert. A changed
/// request restarts its counter and seeks both slots to frame 0 with an
/// eight-tick blend; otherwise the counter and both slots advance. Slot 0 is
/// left alone. Animation ticks, rather than render frames, time fades and cries.
static inline void _skullStalkerTickAnimation(Task* task)
{
    enum { SKULL_STALKER_ANIMATION_BLEND_TICKS = 8 };
    SkullStalkerWork* work = task->work;
    s32               slotIndex;

    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrames  = 0;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->anim, slotIndex, work->animId, 0, SKULL_STALKER_ANIMATION_BLEND_TICKS);
        }
    } else {
        work->animFrames++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
            animationTickSlot(&work->anim, slotIndex);
        }
    }
}

/// Counts down a kill, detaches the enemy, then destroys it after the linger period.
///
/// Paused actor control stops all death progress; hidden control hides the model
/// and disables lock-on. Otherwise the root is saved and flattened each tick.
/// Countdown ticks still advance animation. After detachment the linger ticks
/// only flatten; the terminal tick frees the enemy and requests task teardown.
static void _skullStalkerDeathState(Enemy* enemy, Task* task)
{
    enum { SKULL_STALKER_DEATH_LINGER_TICKS = 61 };
    SkullStalkerWork* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;

    work      = task->work;
    model     = task->extra.tmd;
    rootCoord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                 |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    if (work->deathPhase != SKULL_STALKER_DEATH_PHASE_COUNTDOWN) {
        work->savedRootMtx = rootCoord->coord;
        _skullStalkerFlatten(task);
        work->phaseFrames++;
        if (work->phaseFrames >= SKULL_STALKER_DEATH_LINGER_TICKS) {
            enemyDestroy(enemy, task);
        }
        return;
    }
    work->savedRootMtx = rootCoord->coord;
    _skullStalkerFlatten(task);
    // Detach embedded records before the linger can release their owning work.
    task->killCountdown--;
    if (task->killCountdown <= 0) {
        sceneReleaseBattleRefWithRewards(task, 0x2F);
        work->deathPhase  = SKULL_STALKER_DEATH_PHASE_LINGER;
        work->phaseFrames = 0;
        enemy->recs       = 0;
        worldTargetUnlinkNode(&enemy->node);
        worldCollisionUnlinkBody(&work->senseBody);
        worldCollisionUnlinkBody(&work->frontSenseBody);
        worldCollisionUnlinkBody(&work->body);
    }
    _skullStalkerTickAnimation(task);
}
