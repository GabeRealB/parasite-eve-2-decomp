/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Spawns the two death particles and their ground glow at the model root.
///
/// The particle argument seeds the size of secondary effects. Requires a live
/// root for placement and the carrier's loaded effect resources. Each spawn
/// may fail independently; offsets remain in the carrier's static storage.
static __inline__ void _skullStalkerSpawnDeathEffects(const Task* task)
{
    enum { SKULL_STALKER_PARTICLE_SECONDARY_SIZE = 0x200 };
    effectSpawn(EFFECT_030, task->extra.tmd->coords, SKULL_STALKER_PARTICLE_SECONDARY_SIZE, &gSkullStalkerSparkOffset);
    effectSpawn(EFFECT_030, task->extra.tmd->coords, SKULL_STALKER_PARTICLE_SECONDARY_SIZE, &gSkullStalkerSparkOffset);
    effectSpawn(EFFECT_RED_GROUND_GLOW, task->extra.tmd->coords, 0, &gSkullStalkerHitFxOffset);
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

    /// Queues one placement-tagged spatial sound.
    ///
    /// Captures the live task, composed rootCoord and soundId (overwritten).
    /// scriptId is evaluated once; rootCoord is sampled by both audio queries.
#define SKULL_STALKER_PLAY_CONTACT_SOUND(scriptId)                                                                                   \
    do {                                                                                                                             \
        Enemy* soundEnemy = task->spawnArg2.pointer;                                                                                 \
        soundId           = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | (scriptId);                                   \
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
