/* Part of the Moth library; see moth.h. */

/// Allocates and initializes a moth, then enters its living state.
///
/// The task must own a four-part TMD model and carry this enemy in spawnArg2.
/// Work survives until task teardown; allocation failure destroys both task
/// and enemy. Builds both texture-buffer halves after moving texture/CLUT
/// offsets by one, binds lighting and animation slots 1..3, records the home
/// pose and acquires a battle reference. Links radius-250 hit/grid spheres and
/// a radius-400 attack sphere; only the hit and grid tests start enabled.
static void _mothSpawn(Enemy* enemy, Task* task)
{
    enum {
        MOTH_BODY_ID             = 8,
        MOTH_BODY_RADIUS         = 250,
        MOTH_ATTACK_RADIUS       = 400,
        MOTH_HIT_EFFECT_ARGUMENT = 0x100,
        MOTH_HIT_EFFECT_COUNT    = 1
    };

    MothWork*  work;
    GfxCoord*  rootCoord;
    TmdObject* model;
    s32        slotIndex;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(MothWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work                = work;
    model->flags              = 0;
    rootCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    model->texturePageOffset += 1;
    model->clutRowOffset     += 1;
    tmdBuildBufferHalf(model);
    tmdBuildBufferHalf(model);
    // The task owns the work; the model and spheres borrow storage inside it.
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
    enemy->field_4  = &rootCoord->coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = rootCoord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &_gMothParams;
    enemy->recs                   = work->hitContacts;
    enemy->hp                     = _gMothParams.hpMax;
    work->hitEffectArg.spawnArgLo = MOTH_HIT_EFFECT_ARGUMENT;
    work->hitEffectArg.spawnArgHi = MOTH_HIT_EFFECT_COUNT;
    work->hitEffectArg.coord      = rootCoord;
    animationInitContext(&work->rig.anim, _gMothAnimSets, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, MOTH_ANIM_REST_POSE);
    }
    sceneAcquireBattleRef(0);
    work->flapSign                 = 1;
    work->homePos.vx               = rootCoord->coord.t[0];
    work->homePos.vy               = rootCoord->coord.t[1];
    work->homePos.vz               = rootCoord->coord.t[2];
    work->yaw                      = ((Enemy*)task->spawnArg2.pointer)->place->yaw;
    work->hitBody.coord            = rootCoord;
    work->hitBody.context.contacts = work->hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | MOTH_BODY_ID);
    work->hitBody.radius           = MOTH_BODY_RADIUS;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->gridBody.coord            = rootCoord;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | MOTH_BODY_ID);
    work->gridBody.radius           = MOTH_BODY_RADIUS;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->hitBody.flags             = (u16)(work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->attackBody.coord            = rootCoord;
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->gridBody.flags              = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->attackBody.key              = damagePackAttackKey(&_gMothAttack, 0);
    work->attackBody.radius           = MOTH_ATTACK_RADIUS;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    task->state            = MOTH_TASK_UPDATE;
}

/// Advances the living moth's contacts, wing motion, flight and lighting.
///
/// Requires work initialized by spawn. Running actors restore drawing/targeting;
/// paused actors update colour alone, and hidden actors disable both and return.
/// A flock death alert starts pursuit and engages battle. A contact can select
/// the death state, but the current living tick still finishes before the next
/// dispatch. Composes the moved root, samples colour and rolls a 1/128 ambient
/// sound with placement-index identity and signed-byte spatial pan/depth.
static void _mothUpdate(Enemy* enemy, Task* task)
{
    enum {
        MOTH_AMBIENT_SOUND           = 0x40070008,
        MOTH_AMBIENT_SOUND_ROLL_MASK = 127
    };

    TmdObject* model;
    MothWork*  work;
    GfxCoord*  rootCoord;

    work      = task->work;
    model     = task->extra.tmd;
    rootCoord = model->coords;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            model->flags                  = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _mothUpdateColor(task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // A killing contact changes the next task state; this living tick still finishes.
    _mothContacts(task);
    _mothOscillateParts(task);
    if (work->alerted == false && gSceneCombatState.actor00700DeathAlert != 0) {
        work->alerted = true;
        sceneEngageBattle(1);
    }
    _mothSteer(task);
    _mothDrift(task);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    _mothUpdateColor(task);
    RANDOM_LCG_ADVANCE();
    if ((gRandomLcgState >> 16 & MOTH_AMBIENT_SOUND_ROLL_MASK) == 0) {
        s32 soundPan;
        s32 soundId;

        soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MOTH_SOUND_PLACE_INDEX_SHIFT) | MOTH_AMBIENT_SOUND;
        soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}

/// Applies resolved grid correction or restores the position before an opposed move.
///
/// Borrows the root, work and the caller's reserved delta; `scratch` must equal
/// `scratchHead - 1`. Clears all four grid contacts after consuming them.
/// Correction uses signed integer halves of Q16.16; the opposed result restores
/// the whole-unit pre-move position. Neither delta pointer is retained.
static inline void _mothApplyGridCorrection(GfxCoord* rootCoord, MothWork* work,
                                            const WorldCollisionDelta* scratchHead,
                                            const WorldCollisionDelta* scratch, s32 pushbackResult)
{
    s32 correctedZ;

    switch (pushbackResult) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            rootCoord->coord.t[0] += scratchHead[-1].fixed.vx.halves.integer;
            rootCoord->coord.t[1] += scratch->fixed.vy.halves.integer;
            correctedZ             = rootCoord->coord.t[2] + scratch->fixed.vz.halves.integer;
            rootCoord->coord.t[2]  = correctedZ;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevPos.vx;
            rootCoord->coord.t[1] = work->prevPos.vy;
            rootCoord->coord.t[2] = work->prevPos.vz;
            break;
    }
    worldCollisionClearContacts(work->gridContacts);
}

/// Applies room-grid pushback and starts the moth's death on player contact or attack.
///
/// Requires initialized work, a live root and enemy spawn argument. Reads all
/// four grid contacts: integer halves of Q16.16 correction move the root, while
/// opposed normals restore the position saved before the preceding move. A
/// player-body contact kills outright; an attack also rolls range-based damage,
/// awards its readout/life drain (zero damage becomes one) and spawns the hit effect.
/// Attack key low-byte bit 7 selects a live player/companion task (0/1); both
/// roots must share a parent frame. Clears both tables and releases one delta.
static void _mothContacts(Task* task)
{
    enum { MOTH_HIT_ACTOR_SLOT_SHIFT = 7 };

    MothWork*            work;
    GfxCoord*            rootCoord;
    s32                  pushbackResult;
    s32                  playerOffsetX;
    s32                  playerOffsetY;
    s32                  playerOffsetZ;
    s32                  attackDamage;
    u16                  contactKind;
    GfxCoord*            attackerRoot;
    WorldCollisionDelta* scratchHead;
    WorldCollisionDelta* scratch;

    work        = task->work;
    scratchHead = SCRATCH_STACK_CURSOR(WorldCollisionDelta);
    scratch     = (SCRATCH_STACK_CURSOR(WorldCollisionDelta) = scratchHead - 1);
    rootCoord   = task->extra.tmd->coords;
    // Consume the previous collision pass before the next flight step.
    pushbackResult = worldCollisionResolvePushback(work->gridContacts, scratch, ARRAY_SIZE(work->gridContacts), 0);
    _mothApplyGridCorrection(rootCoord, work, scratchHead, scratch, pushbackResult);
    contactKind = work->hitContacts[0].key.parts.kind;
    switch ((u32)contactKind) {
        case 0:
            break;
        case WORLD_COLLISION_CONTACT_PLAYER_BODY >> 16:
            task->state                           = MOTH_TASK_DEATH;
            ((Enemy*)task->spawnArg2.pointer)->hp = 0;
            sceneEngageBattle(1);
            break;
        case WORLD_COLLISION_CONTACT_ATTACK >> 16:
            task->state        = contactKind;
            attackerRoot       = gPlayerActorTasks[(u8)work->hitContacts[0].key.parts.id >> MOTH_HIT_ACTOR_SLOT_SHIFT]->extra.tmd->coords;
            playerOffsetX      = attackerRoot->coord.t[0] - rootCoord->coord.t[0];
            scratch->vector.vx = playerOffsetX;
            playerOffsetY      = attackerRoot->coord.t[1] - rootCoord->coord.t[1];
            scratch->vector.vy = playerOffsetY;
            playerOffsetZ      = attackerRoot->coord.t[2] - rootCoord->coord.t[2];
            scratch->vector.vz = playerOffsetZ;
            attackDamage       = damageComputePlayerAttack(work->hitContacts[0].key.value,
                                                           SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetY * playerOffsetY) + (playerOffsetZ * playerOffsetZ)), 0, 0);
            if (attackDamage == 0) {
                attackDamage = 1;
            }
            worldTargetAddReadoutAmount(&((Enemy*)task->spawnArg2.pointer)->node, attackDamage, 0);
            damageAccumulateLifeDrainHp(task->spawnArg2.pointer, work->hitContacts[0].key.value, attackDamage, 0);
            ((Enemy*)task->spawnArg2.pointer)->hp = 0;
            effectSpawnHit(damageGetPlayerAttackEffectId(work->hitContacts[0].key.value), task->extra.tmd->coords, 0,
                           &work->hitEffectArg);
            break;
    }
    worldCollisionClearContacts(work->hitContacts);
    SCRATCH_STACK_RELEASE_BLOCK(WorldCollisionDelta);
}

/// Flaps the model's two wing parts in opposite directions about local Z.
///
/// Requires the moth work and four-part model. Every 16 living ticks rerolls
/// slow/fast mode. Slow mode sweeps -256, 0, +256 angular units and reverses
/// the swing sign on wrapping; fast mode alternates the full swing each tick.
/// Parts 2 and 3 retain their translations and have composition marked dirty.
/// The scratch angle vector is released before return; 4096 units is one turn.
static void _mothOscillateParts(Task* task)
{
    enum {
        MOTH_FLAP_MODE_TICKS = 16,
        MOTH_FLAP_ANGLE      = 0x100,
        MOTH_FLAP_SWEEP_END  = 2 * MOTH_FLAP_ANGLE,
        MOTH_LEFT_WING_PART  = 2,
        MOTH_RIGHT_WING_PART = 3
    };

    MothWork* work;
    GfxCoord* leftWingCoords;
    GfxCoord* rightWingCoords;
    SVECTOR*  wingAngles;
    s32       slowFlapSign;
    s32       fastFlapSign;
    s32       wingAngle;

    wingAngles = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work       = task->work;
    if (++work->timer >= MOTH_FLAP_MODE_TICKS) {
        work->timer = 0;
        RANDOM_LCG_ADVANCE();
        work->flapFast = !(RANDOM_LCG_DRAW() & 1);
    }
    switch (work->flapFast) {
        case false:
            work->flapAngle += MOTH_FLAP_ANGLE;
            if (work->flapAngle >= MOTH_FLAP_SWEEP_END) {
                work->flapAngle = -MOTH_FLAP_ANGLE;
                slowFlapSign    = work->flapSign;
                work->flapSign  = -slowFlapSign;
            }
            break;
        case true:
            work->flapAngle = MOTH_FLAP_ANGLE;
            fastFlapSign    = work->flapSign;
            work->flapSign  = -fastFlapSign;
            break;
    }
    wingAngles->vx = 0;
    wingAngles->vy = 0;
    wingAngles->vz = work->flapAngle * work->flapSign;
    leftWingCoords = task->extra.tmd->coords;
    RotMatrix(wingAngles, &leftWingCoords[MOTH_LEFT_WING_PART].coord);
    leftWingCoords[MOTH_LEFT_WING_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    wingAngles->vx                                   = 0;
    wingAngles->vy                                   = 0;
    wingAngle                                        = work->flapAngle * work->flapSign;
    wingAngles->vz                                   = -wingAngle;
    rightWingCoords                                  = task->extra.tmd->coords;
    RotMatrix(wingAngles, &rightWingCoords[MOTH_RIGHT_WING_PART].coord);
    rightWingCoords[MOTH_RIGHT_WING_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Rebuilds the living moth's root from its wandering or pursuing heading and pitch.
///
/// Angles use 4096 units per turn. Unalerted yaw steps randomly by 0..31;
/// alerted yaw turns toward the player's parent-frame X/Z offset by 16,
/// snapping within 16. Pitch steps randomly by 0..63 and clamps to +/-256.
/// Player offsets narrow to signed halfwords. The wrapped difference is used
/// only for its turn sign; composition is invalidated by the living update.
/// One face scratch block is released before return.
static void _mothSteer(Task* task)
{
    enum {
        MOTH_PURSUIT_TURN_STEP       = 16,
        MOTH_PITCH_LIMIT             = 256,
        MOTH_YAW_STEP_MASK           = 31,
        MOTH_YAW_POSITIVE_STEP_BIT   = 32,
        MOTH_PITCH_STEP_MASK         = 63,
        MOTH_PITCH_POSITIVE_STEP_BIT = 64
    };

    MothWork*         work;
    GfxCoord*         rootCoord;
    ActorFaceScratch* scratch;
    s32               yawDraw;
    s32               yawStep;
    s32               previousYaw;
    s32               pursuitYaw;
    s32               previousPitch;
    s32               pitchDraw;
    s32               pitchStep;
    u16               playerYaw;
    s16               yawDifference;
    s32               yawDistance;
    s16               turnDirection;
    s16               wrappedDifference;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    switch (work->alerted) {
        case false:
            RANDOM_LCG_ADVANCE();
            yawDraw     = RANDOM_LCG_DRAW();
            yawStep     = yawDraw & MOTH_YAW_STEP_MASK;
            previousYaw = work->yaw;
            work->yaw   = !(yawDraw & MOTH_YAW_POSITIVE_STEP_BIT) ? previousYaw - yawStep : previousYaw + yawStep;
            break;
        case true:
            scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
            scratch->delta.vy = 0;
            scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
            playerYaw         = ratan2((s16)scratch->delta.vx, (s16)scratch->delta.vz) & ACTOR_TRANSFORM_ANGLE_MASK;
            yawDifference     = playerYaw - (work->yaw & ACTOR_TRANSFORM_ANGLE_MASK);
            yawDistance       = yawDifference >= 0 ? yawDifference : -yawDifference;
            turnDirection     = yawDifference;
            if (yawDistance < MOTH_PURSUIT_TURN_STEP + 1) {
                work->yaw = playerYaw;
            } else {
                if (yawDistance >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
                    wrappedDifference = yawDifference - ACTOR_TRANSFORM_ANGLE_TURN;
                    if (yawDifference <= 0)
                        wrappedDifference = ACTOR_TRANSFORM_ANGLE_TURN - yawDifference;
                    turnDirection = wrappedDifference;
                }
                pursuitYaw = work->yaw;
                if (turnDirection > 0) {
                    work->yaw = pursuitYaw + MOTH_PURSUIT_TURN_STEP;
                } else {
                    work->yaw = pursuitYaw - MOTH_PURSUIT_TURN_STEP;
                }
            }
            break;
    }
    RANDOM_LCG_ADVANCE();
    pitchDraw     = RANDOM_LCG_DRAW();
    pitchStep     = pitchDraw & MOTH_PITCH_STEP_MASK;
    previousPitch = work->pitch;
    work->pitch   = !(pitchDraw & MOTH_PITCH_POSITIVE_STEP_BIT) ? previousPitch - pitchStep : previousPitch + pitchStep;
    if (work->pitch > MOTH_PITCH_LIMIT) {
        work->pitch = MOTH_PITCH_LIMIT;
    } else if (work->pitch < -MOTH_PITCH_LIMIT) {
        work->pitch = -MOTH_PITCH_LIMIT;
    }
    scratch->rot.vx = work->pitch;
    scratch->rot.vy = work->yaw;
    scratch->rot.vz = 0;
    RotMatrix(&scratch->rot, &rootCoord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Saves the previous root position, then wanders or advances toward the player.
///
/// Positions and speeds are game-coordinate units in the roots' common parent
/// frame. Wandering reverses signed-halfword candidate X/Y steps outside open
/// home bands of +/-200 and +/-500; Z instead tests the signed random draw
/// against home Z +/-200, retaining that distinct behavior. Alerted flight
/// uses rowIndex 0..7 to select speed, adds 0..31 and follows the Q12 forward
/// axis. Height tends toward 1200 above the player within a +/-400 band;
/// the target height narrows to s16. Fast flapping biases Y upward.
static void _mothDrift(Task* task)
{
    enum {
        MOTH_WANDER_HORIZONTAL_HALF_EXTENT = 200,
        MOTH_WANDER_VERTICAL_HALF_EXTENT   = 500,
        MOTH_PURSUIT_HEIGHT_ABOVE_PLAYER   = 1200,
        MOTH_PURSUIT_HEIGHT_HALF_BAND      = 400,
        MOTH_FORWARD_BASIS_FRACTION_BITS   = 12,
        MOTH_FLIGHT_JITTER_MASK            = 31,
        MOTH_WANDER_POSITIVE_STEP_BIT      = 32,
        MOTH_PURSUIT_HEIGHT_STEP_MASK      = 15
    };

    MothWork* work;
    GfxCoord* rootCoord;
    u32       horizontalDraw;
    u32       heightDraw;
    u32       zDraw;
    s32       horizontalStep;
    s32       verticalStep;
    s16       wanderStep;
    s16       forwardSpeed;
    s32       currentY;
    s32       nextY;
    s16       targetY;

    work             = task->work;
    rootCoord        = task->extra.tmd->coords;
    work->prevPos.vx = rootCoord->coord.t[0];
    work->prevPos.vy = rootCoord->coord.t[1];
    work->prevPos.vz = rootCoord->coord.t[2];
    switch (work->alerted) {
        case false:
            horizontalDraw = (RANDOM_LCG_ADVANCE()) >> 16;
            horizontalStep = horizontalDraw & MOTH_FLIGHT_JITTER_MASK;
            if (!(horizontalDraw & MOTH_WANDER_POSITIVE_STEP_BIT)) {
                horizontalStep = -horizontalStep;
            }
            wanderStep = horizontalStep;
            if ((s16)(rootCoord->coord.t[0] + wanderStep) < work->homePos.vx + MOTH_WANDER_HORIZONTAL_HALF_EXTENT &&
                work->homePos.vx - MOTH_WANDER_HORIZONTAL_HALF_EXTENT < (s16)(rootCoord->coord.t[0] + wanderStep)) {
                rootCoord->coord.t[0] += wanderStep;
            } else {
                rootCoord->coord.t[0] -= wanderStep;
            }
            verticalStep = ((RANDOM_LCG_ADVANCE()) >> 16) & MOTH_FLIGHT_JITTER_MASK;
            if (work->flapFast != 0) {
                verticalStep = -verticalStep;
            }
            wanderStep = verticalStep;
            if ((s16)(rootCoord->coord.t[1] + wanderStep) < work->homePos.vy + MOTH_WANDER_VERTICAL_HALF_EXTENT &&
                work->homePos.vy - MOTH_WANDER_VERTICAL_HALF_EXTENT < (s16)(rootCoord->coord.t[1] + wanderStep)) {
                rootCoord->coord.t[1] += wanderStep;
            } else {
                rootCoord->coord.t[1] -= wanderStep;
            }
            zDraw          = (RANDOM_LCG_ADVANCE()) >> 16;
            horizontalStep = zDraw & MOTH_FLIGHT_JITTER_MASK;
            if (!(zDraw & MOTH_WANDER_POSITIVE_STEP_BIT)) {
                horizontalStep = -horizontalStep;
            }
            wanderStep = horizontalStep;
            // The retained Z reversal tests the draw, not the proposed position.
            if ((s16)zDraw < work->homePos.vz + MOTH_WANDER_HORIZONTAL_HALF_EXTENT && work->homePos.vz - MOTH_WANDER_HORIZONTAL_HALF_EXTENT < (s16)zDraw) {
                rootCoord->coord.t[2] += wanderStep;
            } else {
                rootCoord->coord.t[2] -= wanderStep;
            }
            break;
        case true:
            forwardSpeed = _gMothPursuitSpeeds[((Enemy*)task->spawnArg2.pointer)->place->rowIndex] +
                           (((RANDOM_LCG_ADVANCE()) >> 16) & MOTH_FLIGHT_JITTER_MASK);
            rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * forwardSpeed) >> MOTH_FORWARD_BASIS_FRACTION_BITS;
            rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * forwardSpeed) >> MOTH_FORWARD_BASIS_FRACTION_BITS;
            targetY                = gPlayerStatus.coordMtx->t[1] - MOTH_PURSUIT_HEIGHT_ABOVE_PLAYER;
            heightDraw             = (RANDOM_LCG_ADVANCE()) >> 16;
            currentY               = rootCoord->coord.t[1];
            if (currentY >= targetY + MOTH_PURSUIT_HEIGHT_HALF_BAND) {
                rootCoord->coord.t[1] = currentY - (heightDraw & MOTH_PURSUIT_HEIGHT_STEP_MASK);
            } else {
                if (targetY - MOTH_PURSUIT_HEIGHT_HALF_BAND >= currentY) {
                    nextY = currentY + (heightDraw & MOTH_PURSUIT_HEIGHT_STEP_MASK);
                } else {
                    verticalStep = heightDraw & MOTH_FLIGHT_JITTER_MASK;
                    if (work->flapFast != 0) {
                        nextY = currentY - verticalStep;
                    } else {
                        nextY = currentY + verticalStep;
                    }
                }
                rootCoord->coord.t[1] = nextY;
            }
            break;
    }
}

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
                    nextSeed                               = RANDOM_LCG_NEXT(gRandomLcgState);
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

/// Builds the moth burst's square offsets before its screen-space roll.
///
/// Borrows a reserved quad scratch block. `halfSize` is in screen pixels;
/// the projected caller supplies 0..1536. Writes signed-halfword XYZ in
/// top-left, top-right, bottom-left, bottom-right order with zero Z; corner
/// pad words and the projected centre/depth remain intact.
static inline void _mothSetBurstCorners(ActorScreenQuadScratch* scratch, s32 halfSize)
{
    scratch->corners[0].vx = -halfSize;
    scratch->corners[0].vy = -halfSize;
    scratch->corners[0].vz = 0;
    scratch->corners[1].vx = halfSize;
    scratch->corners[1].vy = -halfSize;
    scratch->corners[1].vz = 0;
    scratch->corners[2].vx = -halfSize;
    scratch->corners[2].vy = halfSize;
    scratch->corners[2].vz = 0;
    scratch->corners[3].vx = halfSize;
    scratch->corners[3].vy = halfSize;
    scratch->corners[3].vz = 0;
}

/// Queues the moth's death burst with a fixed random screen roll and subtractive blend.
///
/// Requires a composed root, timer in 1..23, eight UV cells and live primitive/
/// ordering-table storage. Projects the cached root through GsWSMATRIX and
/// skips OT depth below 20; half-size is 30720/depth screen pixels. Timer 1
/// picks the retained screen roll only if projection passes the depth test.
/// Cells are 32 pixels square, selected by timer/3. Uses the model's signed
/// texture-page/CLUT-row offsets and releases its scratch quad before return.
static void _mothDrawBurst(Task* task)
{
    enum {
        MOTH_BURST_MIN_OT_DEPTH       = 20,
        MOTH_BURST_SIZE_NUMERATOR     = 0x7800,
        MOTH_BURST_CELL_PIXELS        = 32,
        MOTH_BURST_TEXTURE_X          = 384,
        MOTH_BURST_TEXTURE_Y          = 256,
        MOTH_BURST_CLUT_ROW           = 245,
        MOTH_BURST_TEXTURE_PAGE_WORDS = 64,
        MOTH_BURST_SCREEN_X_MASK      = 0xFFFF,
        MOTH_BURST_NEUTRAL_MODULATION = 128
    };

    ActorScreenQuadScratch* scratch;
    MothWork*               work;
    TmdObject*              model;
    GfxCoord*               rootCoord;
    s32                     halfSize, screenX, screenY;
    s16                     cornerIndex;
    SVECTOR*                corner;
    POLY_FT4*               quad;
    ActorSpriteUv*          cell;
    model                  = task->extra.tmd;
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(ActorScreenQuadScratch);
    rootCoord              = model->coords;
    work                   = task->work;
    scratch->corners[0].vx = rootCoord->workm.t[0];
    scratch->corners[0].vy = rootCoord->workm.t[1];
    scratch->corners[0].vz = rootCoord->workm.t[2];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(&scratch->corners[0]);
    gte_rtps();
    gte_stsxy(&scratch->screenCentre);
    gte_stszotz(&scratch->otz);
    if (scratch->otz < MOTH_BURST_MIN_OT_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(ActorScreenQuadScratch);
        return;
    }
    if (work->timer == 1) {
        scratch->corners[0].vx = 0;
        scratch->corners[0].vy = 0;
        scratch->corners[0].vz = ((RANDOM_LCG_ADVANCE()) >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
        RotMatrix(&scratch->corners[0], &work->burstRollMtx);
    }
    // Rotate a camera-facing quad in screen space, then bind its animation cell.
    halfSize = MOTH_BURST_SIZE_NUMERATOR / scratch->otz;
    screenX  = scratch->screenCentre & MOTH_BURST_SCREEN_X_MASK;
    screenY  = scratch->screenCentre >> 16;
    _mothSetBurstCorners(scratch, halfSize);
    for (cornerIndex = 0; cornerIndex < (s32)ARRAY_SIZE(scratch->corners); cornerIndex++) {
        gte_SetRotMatrix(&work->burstRollMtx);
        corner = &scratch->corners[cornerIndex];
        gte_ldv0(corner);
        gte_rtv0();
        gte_stsv(corner);
        corner->vx += screenX;
        corner->vy += screenY;
    }
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    setSemiTrans(quad, true);
    setRGB0(quad, MOTH_BURST_NEUTRAL_MODULATION, MOTH_BURST_NEUTRAL_MODULATION, MOTH_BURST_NEUTRAL_MODULATION);
    setShadeTex(quad, true);
    quad->tpage = getTPage(1, GPU_BLEND_SUBTRACT, model->texturePageOffset * MOTH_BURST_TEXTURE_PAGE_WORDS + MOTH_BURST_TEXTURE_X, MOTH_BURST_TEXTURE_Y);
    quad->clut  = (model->clutRowOffset << 6) + getClut(0, MOTH_BURST_CLUT_ROW);
    cell        = &_gMothBurstUvs[(s16)(work->timer / MOTH_BURST_TICKS_PER_CELL)];
    quad->u0    = cell->u;
    quad->v0    = cell->v;
    quad->u1    = cell->u + MOTH_BURST_CELL_PIXELS - 1;
    quad->v1    = cell->v;
    quad->u2    = cell->u;
    quad->v2    = cell->v + MOTH_BURST_CELL_PIXELS - 1;
    quad->u3    = cell->u + MOTH_BURST_CELL_PIXELS - 1;
    quad->v3    = cell->v + MOTH_BURST_CELL_PIXELS - 1;
    quad->x0    = scratch->corners[0].vx;
    quad->y0    = scratch->corners[0].vy;
    quad->x1    = scratch->corners[1].vx;
    quad->y1    = scratch->corners[1].vy;
    quad->x2    = scratch->corners[2].vx;
    quad->y2    = scratch->corners[2].vy;
    quad->x3    = scratch->corners[3].vx;
    quad->y3    = scratch->corners[3].vy;
    addPrim((&gGpuCurrentOt[(((((u32)scratch->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*gGpuCurrentOt)]), quad);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScreenQuadScratch);
}
