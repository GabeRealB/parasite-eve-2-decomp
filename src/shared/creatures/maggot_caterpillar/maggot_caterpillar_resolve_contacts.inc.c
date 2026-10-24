/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Consumes grid, hit-body and attack contacts for one enemy frame.
///
/// Resolves four grid contacts as signed 16.16 room-space pushback, restoring
/// the prior root position for opposed normals. Scans both hit-body entries:
/// player attacks reduce halfword HP and may change behavior, while bodies
/// contribute only the deepest overlap's horizontal Q12-normal push.
/// Own flame contacts deal damage only at burn-cycle frame zero, bypass
/// critical/life-drain processing and do not set `struck`. Committed actions
/// retain behavior on damage; a rebound interrupted by a hit reverses yaw.
/// Requires valid attack-table keys and live player/companion task index 0/1.
/// Clears the consumed tables and releases its temporary scratch block.
static void _maggotCaterpillarResolveContacts(Task* actor)
{
    enum { MAGGOT_CATERPILLAR_BURST_REACTION = 4 };
    MaggotCaterpillarWork*            work;
    MaggotCaterpillarContactsScratch* scratchEnd;
    MaggotCaterpillarContactsScratch* scratch;
    Enemy*                            enemy;
    GfxCoord*                         coord;
    GfxCoord*                         attackerCoord;
    s32                               result;
    s32                               lastEffectKey;
    u32                               baseDamage;
    s16                               hitDamage;
    s32                               deepestOverlap;
    s32                               overlapDepth;
    s32                               attackerOffsetX;
    s32                               attackerOffsetY;
    s32                               attackerOffsetZ;
    VECTOR*                           pushNormal;
    s32                               contactIndex;
    s32                               one;
    u32                               contactKind;

    // Apply room-grid correction before measuring hit ranges and body overlap.
    deepestOverlap = 0;
    lastEffectKey  = 0;
    work           = actor->work;
    coord          = actor->extra.tmd->coords;
    scratchEnd     = SCRATCH_STACK_CURSOR(MaggotCaterpillarContactsScratch);
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(MaggotCaterpillarContactsScratch);
    enemy          = actor->spawnArg2.pointer;
    work->landed   = 0;
    result         = worldCollisionResolvePushback(work->gridContacts, &scratch->delta, ARRAY_SIZE(work->gridContacts), NULL);
    if (result != 0) {
        if (work->behaviour == MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH) {
            work->landed = 1;
        }
        switch (result) {
            case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
                break;
            case WORLD_COLLISION_PUSHBACK_GRID_HIT:
                coord->coord.t[0] += scratchEnd[-1].delta.fixed.vx.halves.integer;
                coord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                coord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
                break;
            case WORLD_COLLISION_PUSHBACK_OPPOSED:
                coord->coord.t[0] = work->prevPos.vx;
                coord->coord.t[1] = work->prevPos.vy;
                coord->coord.t[2] = work->prevPos.vz;
                break;
        }
    }
    worldCollisionClearContacts(work->gridContacts);
    if (work->hitCooldown != 0) {
        if (--work->hitCooldown <= 0) {
            work->hitCooldown = 0;
        }
    }
    one = MAGGOT_CATERPILLAR_REACTION_COMMITTED; // Shared value for committed reactions, category 1 and latched flags.

    work->struck = 0;
    work->burst  = 0;
    pushNormal   = &scratch->normal;
    for (contactIndex = 0; contactIndex < (s32)ARRAY_SIZE(work->bodyContacts); contactIndex++) {
        contactKind = (u32)work->bodyContacts[contactIndex].key.value >> 16;
        if (contactKind == one)
            goto physical;
        if (contactKind == 0)
            goto next_contact;
        if (contactKind == (WORLD_COLLISION_CONTACT_ATTACK >> 16))
            goto damage_contact;
        if (contactKind == (WORLD_COLLISION_CONTACT_ENEMY_BODY >> 16))
            goto physical;
        goto next_contact;
    damage_contact:
        if (work->hitCooldown == 0) {
            result = 0;
            if ((((u32)work->bodyContacts[contactIndex].key.value >> 8) & 0x3F) == MAGGOT_CATERPILLAR_FLAME_ATTACK_ROW) {
                if ((work->bodyContacts[contactIndex].key.value & 0x3F) == MAGGOT_CATERPILLAR_FLAME_ATTACK_ROW) {
                    result = 1;
                }
            }
            if ((result != one) || (work->burnFrame == 0)) {
                attackerCoord            = gPlayerActorTasks[((u32)work->bodyContacts[contactIndex].key.value >> 7) & 1]->extra.tmd->coords;
                attackerOffsetX          = attackerCoord->coord.t[0] - coord->coord.t[0];
                scratch->delta.vector.vx = attackerOffsetX;
                attackerOffsetY          = attackerCoord->coord.t[1] - coord->coord.t[1];
                scratch->delta.vector.vy = attackerOffsetY;
                attackerOffsetZ          = attackerCoord->coord.t[2] - coord->coord.t[2];
                scratch->delta.vector.vz = attackerOffsetZ;
                baseDamage               = damageComputePlayerAttack(work->bodyContacts[contactIndex].key.value, SquareRoot0(attackerOffsetX * attackerOffsetX + attackerOffsetY * attackerOffsetY + attackerOffsetZ * attackerOffsetZ), 0, 0);
                hitDamage                = baseDamage;
                if (result == 0) {
                    if (work->midLeap != 0) {
                        hitDamage = (baseDamage << 16) >> 15;
                        effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 3, NULL);
                    }
                    if (damageRollCriticalHit(enemy, work->bodyContacts[contactIndex].key.value, 0) != 0) {
                        hitDamage = (hitDamage << 16) >> 14;
                        if (work->midLeap == 0) {
                            effectSpawn(EFFECT_CRITICAL_HIT, actor->extra.tmd->coords + 1, 0, NULL);
                        }
                    }
                    damageAccumulateLifeDrainHp(enemy, work->bodyContacts[contactIndex].key.value, hitDamage, 0);
                }
                // Preserve signed-halfword damage truncation and HP arithmetic.
                worldTargetAddReadoutAmount(&enemy->node, hitDamage, 0);
                enemy->hp -= hitDamage;
                if (work->reactionMode != one) {
                    if (enemy->hp <= 0) {
                        work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                        work->step      = 0;
                        actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
                    } else if (result == 0) {
                        work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_HURT;
                        work->step      = 0;
                    }
                }
                if (work->reactionMode == MAGGOT_CATERPILLAR_REACTION_REBOUND) {
                    if ((work->behaviour == MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD) || (result == 0)) {
                        work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                        MAGGOT_CATERPILLAR_TURN_AROUND(work, coord, &scratch->shortVector);
                    }
                }
                if (result == 0) {
                    work->struck            = one;
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
                switch (damageGetPlayerAttackReaction(work->bodyContacts[contactIndex].key.value) & 0xFFFF) {
                    case DAMAGE_PLAYER_REACTION_NONE:
                    case DAMAGE_PLAYER_REACTION_STAGGER:
                    case 5:
                    case 8:
                    case 9:
                        break;
                    case DAMAGE_PLAYER_REACTION_BUILDUP:
                        damageStartEnemyBuildup(enemy, work->bodyContacts[contactIndex].key.value, 0);
                        break;
                    case DAMAGE_PLAYER_REACTION_POISON:
                        damageTryStartEnemyDamageOverTime(enemy, work->bodyContacts[contactIndex].key.value, 0);
                        break;
                    case MAGGOT_CATERPILLAR_BURST_REACTION:
                    case DAMAGE_PLAYER_REACTION_EXPLOSION:
                        if (work->reactionMode != one) {
                            work->burst = one;
                        }
                        break;
                    case DAMAGE_PLAYER_REACTION_INCENDIARY:
                        if (work->burning == 0) {
                            work->burning          = one;
                            work->burnSoundTimer   = 0;
                            work->burnFrame        = 0;
                            work->flameBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                            worldCoordSetActorColorMode(actor->spawnArg2.pointer, ENEMY_COLOR_TINT);
                        }
                        break;
                }
                if (lastEffectKey != work->bodyContacts[contactIndex].key.value) {
                    lastEffectKey           = work->bodyContacts[contactIndex].key.value;
                    scratch->shortVector.vx = 0;
                    scratch->shortVector.vy = -0xC8;
                    scratch->shortVector.vz = 0;
                    effectSpawnHit(damageGetPlayerAttackEffectId(work->bodyContacts[contactIndex].key.value), actor->extra.tmd->coords + 1, &scratch->shortVector, &work->effectArg);
                }
                result = damageGetPlayerAttackHitCooldown(work->bodyContacts[contactIndex].key.value);
                if (result > 0) {
                    work->hitCooldown = result;
                }
            }
        }
        goto next_contact;
    physical:
        MAGGOT_CATERPILLAR_CONTACT_OVERLAP(overlapDepth, coord, work->bodyContacts[contactIndex], scratch->delta);
        if (deepestOverlap < overlapDepth) {
            deepestOverlap = overlapDepth;
            MAGGOT_CATERPILLAR_GRID_DIRECTION(&scratch->delta, pushNormal, &scratch->pushDirection);
        }
    next_contact:;
    }
    if (deepestOverlap > 0) {
        coord->coord.t[0] += (deepestOverlap * scratch->pushDirection.vx) >> 12;
        coord->coord.t[2] += (deepestOverlap * scratch->pushDirection.vz) >> 12;
    }
    worldCollisionClearContacts(work->bodyContacts);
    work->blocked = 0;
    if (work->attackContacts[0].flags & WORLD_COLLISION_CONTACT_OCCUPIED) {
        MAGGOT_CATERPILLAR_NOTE_BLOCKING_CONTACT(work, work->attackContacts[0]);
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        worldCollisionClearContacts(work->attackContacts);
    }
    SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarContactsScratch);
}

/// Puff enemy callbacks indexed by task state: 0 setup, 1 flight, 2 destroy.
///
/// The dispatcher copies the typed callbacks before passing the live
/// enemy and task; destruction releases their owned resources.
static const EnemyTaskFuncTable3 _gMaggotCaterpillarPuffStates = {
    {
        _maggotCaterpillarPuffSetup,
        _maggotCaterpillarPuffTick,
        enemyDestroy,
    },
};

/// Body enemy callbacks indexed by task state: 0 spawn, 1 active, 2 dying.
///
/// The dispatcher copies the typed callbacks before passing the live
/// enemy and task. The dying callback completes the enemy's lifetime.
static const EnemyTaskFuncTable3 _gMaggotCaterpillarStates = {
    {
        _maggotCaterpillarSpawn,
        _maggotCaterpillarTick,
        _maggotCaterpillarDyingState,
    },
};

/// Wakes a dormant enemy on player proximity, a hit or scene alert class 2.
///
/// The player and enemy roots must share their parent frame. The strict X/Z
/// wake radius uses game units. During wake ticks 11..60 the root moves four
/// units along parent +Z, independently of facing. Tick 75 starts roam with
/// an idle delay selected by placement row 0..7 plus a random 0..15 ticks.
/// Reserves and releases one VECTOR on the initialized scratch stack.
static void _maggotCaterpillarWaitState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_WAIT_DORMANT          = 0,
        MAGGOT_CATERPILLAR_WAIT_WAKING           = 1,
        MAGGOT_CATERPILLAR_WAKE_MOVE_FIRST_FRAME = 11,
        MAGGOT_CATERPILLAR_WAKE_MOVE_END_FRAME   = 61,
        MAGGOT_CATERPILLAR_WAKE_END_FRAME        = 75,
        MAGGOT_CATERPILLAR_WAKE_ALERT_CLASS      = 2
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    waitStep;
    s32                    playerOffsetX;
    s32                    playerOffsetZ;
    u32                    randomDelay;
    s32                    placementRow;
    VECTOR*                playerOffset;

    playerOffset = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    work         = actor->work;
    waitStep     = work->step;
    coord        = actor->extra.tmd->coords;
    switch (waitStep) {
        case MAGGOT_CATERPILLAR_WAIT_DORMANT:
            playerOffset->vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            playerOffset->vy = 0;
            playerOffsetZ    = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            playerOffset->vz = playerOffsetZ;
            playerOffsetX    = playerOffset->vx;
            if ((SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ)) < MAGGOT_CATERPILLAR_WAKE_RANGE) || (work->struck != 0) || (gSceneCombatState.signals.bytes.enemyAlert == MAGGOT_CATERPILLAR_WAKE_ALERT_CLASS)) {
                work->step   = MAGGOT_CATERPILLAR_WAIT_WAKING;
                work->animId = MAGGOT_CATERPILLAR_ANIM_WAKE;
                sceneEngageBattle(1);
            }
            break;
        case MAGGOT_CATERPILLAR_WAIT_WAKING:
            if ((work->animFrame >= MAGGOT_CATERPILLAR_WAKE_MOVE_FIRST_FRAME) && (work->animFrame < MAGGOT_CATERPILLAR_WAKE_MOVE_END_FRAME)) {
                coord->coord.t[2] += 4;
            }
            if (work->animFrame >= MAGGOT_CATERPILLAR_WAKE_END_FRAME) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                placementRow       = ((Enemy*)actor->spawnArg2.pointer)->place->rowIndex;
                randomDelay        = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState    = randomDelay;
                work->stateCounter = _gMaggotCaterpillarIdleDelay[placementRow] + ((randomDelay >> 0x10) & 0xF);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Starts a pounce when the player lies close enough ahead of the actor.
///
/// The actor and player translations must share a parent frame. Distance uses
/// full-width X/Z coordinates; the bearing uses signed-halfword offsets. The
/// facing tolerance is 128 angle units, with 4096 units per turn. Reserves and
/// releases one vector on the scratch stack.
static void _maggotCaterpillarAimState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_AIM_FACING_TOLERANCE = 128,
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s16                    signedTurn;
    s32                    turnMagnitude;
    s16                    wrappedTurn;
    s16                    facingError;
    s32                    playerDistance;
    s32                    playerOffsetX;
    s32                    playerOffsetZ;
    VECTOR*                delta;
    VECTOR*                scratchEnd;

    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    coord                        = actor->extra.tmd->coords;
    delta                        = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    work                         = actor->work;
    work->yaw                    = ratan2((s32)coord->coord.m[0][2], (s32)coord->coord.m[2][2]) & (ONE - 1);
    scratchEnd[-1].vx            = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
    delta->vy                    = 0;
    playerOffsetZ                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
    delta->vz                    = playerOffsetZ;
    playerOffsetX                = scratchEnd[-1].vx;
    playerDistance               = SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ));
    signedTurn                   = work->yaw - (ratan2((s32)(s16)scratchEnd[-1].vx, (s32)(s16)delta->vz) & (ONE - 1));
    turnMagnitude                = __builtin_abs((s32)signedTurn);
    if (turnMagnitude < (ONE / 2)) {
        facingError = turnMagnitude;
    } else {
        if (signedTurn > 0) {
            wrappedTurn = ONE - signedTurn;
        } else {
            wrappedTurn = signedTurn + ONE;
        }
        facingError = wrappedTurn;
    }
    if ((playerDistance < MAGGOT_CATERPILLAR_POUNCE_RANGE) && (facingError < MAGGOT_CATERPILLAR_AIM_FACING_TOLERANCE)) {
        work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE;
        work->step      = 0;
        work->animId    = MAGGOT_CATERPILLAR_ANIM_POUNCE;
        sceneEngageBattle(1);
    }
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Runs the hanging ambush, drop, landing and recovery sequence.
///
/// A leader triggers on player range; followers wait for the room trigger or a
/// reward, and a hit can start the landing clip directly. The placement row must
/// fit the eight-entry drop-speed and idle tables. During the drop the root
/// world matrix supplies the thread frame. Committed deaths wait for recovery.
static void _maggotCaterpillarAmbushState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_AMBUSH_WAIT               = 0,
        MAGGOT_CATERPILLAR_AMBUSH_DROP               = 1,
        MAGGOT_CATERPILLAR_AMBUSH_LAND               = 2,
        MAGGOT_CATERPILLAR_AMBUSH_GET_UP             = 3,
        MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS      = 300,
        MAGGOT_CATERPILLAR_AMBUSH_LANDING_FALL_FRAME = 12,
        MAGGOT_CATERPILLAR_AMBUSH_RECOVERY_FRAMES    = 30,
        MAGGOT_CATERPILLAR_AMBUSH_GROUND_STEP        = 128,
        MAGGOT_CATERPILLAR_AMBUSH_LAND_HEIGHT        = 1000,
        MAGGOT_CATERPILLAR_AMBUSH_DROP_SOUND         = 0x55200006,
        MAGGOT_CATERPILLAR_AMBUSH_LAND_SOUND         = 0x401A0002,
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    step;
    s32                    dropPan;
    s32                    hitDropPan;
    s32                    landingPan;
    s32                    locationWord;
    s32                    playerOffsetX;
    s32                    playerOffsetZ;
    s32                    playerInRange;
    s32                    soundKey;
    VECTOR*                delta;
    VECTOR*                scratchEnd;

    scratchEnd                   = SCRATCH_STACK_CURSOR(VECTOR);
    delta                        = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    coord                        = actor->extra.tmd->coords;
    work                         = actor->work;
    step                         = work->step;
    locationWord                 = GAME_LOCATION_WORD(gGameSession->location);
    playerInRange                = 0;
    switch (step) {
        case MAGGOT_CATERPILLAR_AMBUSH_WAIT:
            if (work->ambushFollower == 0) {
                scratchEnd[-1].vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
                delta->vy         = 0;
                playerOffsetZ     = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                delta->vz         = playerOffsetZ;
                playerOffsetX     = scratchEnd[-1].vx;
                if (SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ)) < MAGGOT_CATERPILLAR_AMBUSH_RANGE) {
                    playerInRange = 1;
                }
            }
            if ((playerInRange != 0) || (gSceneCombatState.maggotCaterpillarAmbushReady != 0) || (gSceneCombatState.expReward != 0)) {
                if (work->ambushFollower == 0) {
                    gSceneCombatState.maggotCaterpillarAmbushReady = 1;
                }
                sceneEngageBattle(1);
                work->step              = MAGGOT_CATERPILLAR_AMBUSH_DROP;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_DROP;
                work->fallSpeed         = _gMaggotCaterpillarDropSpeed[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->attackBody.coord  = coord;
                work->attackBody.radius = MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS;
                work->attackBody.pos.vy = -MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS;
                work->attackBody.key    = damagePackAttackKey(_gMaggotCaterpillarAttacks, MAGGOT_CATERPILLAR_ATTACK_DROP);
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    soundKey = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_AMBUSH_DROP_SOUND;
                    dropPan  = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(soundKey, (s32)dropPan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                work->baseMatrix = coord->workm;
                break;
            }
            if (work->struck != 0) {
                if (work->ambushFollower == 0) {
                    gSceneCombatState.maggotCaterpillarAmbushReady = 1;
                }
                sceneEngageBattle(1);
                work->step              = MAGGOT_CATERPILLAR_AMBUSH_LAND;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_LAND;
                work->fallSpeed         = _gMaggotCaterpillarDropSpeed[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex];
                work->threadFade        = MAGGOT_CATERPILLAR_THREAD_FADE_FRAMES;
                work->attackBody.radius = MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS;
                work->attackBody.coord  = coord;
                work->attackBody.pos.vy = -MAGGOT_CATERPILLAR_AMBUSH_ATTACK_RADIUS;
                work->attackBody.key    = damagePackAttackKey(_gMaggotCaterpillarAttacks, MAGGOT_CATERPILLAR_ATTACK_DROP);
                work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                if ((locationWord & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(5, 32, 0, 0)) {
                    soundKey   = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_AMBUSH_DROP_SOUND;
                    hitDropPan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(soundKey, (s32)hitDropPan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                work->baseMatrix = coord->workm;
                break;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->baseMatrix = coord->workm;
            break;
        // Keep the upper thread endpoint fixed while the root descends.
        case MAGGOT_CATERPILLAR_AMBUSH_DROP:
            work->vertical.threadRise += work->prevPos.vy - coord->coord.t[1];
            if (((gPlayerStatus.coordMtx->t[1] - MAGGOT_CATERPILLAR_AMBUSH_LAND_HEIGHT) < coord->coord.t[1]) || (work->struck != 0) || (work->blocked != 0)) {
                work->step       = MAGGOT_CATERPILLAR_AMBUSH_LAND;
                work->animId     = MAGGOT_CATERPILLAR_ANIM_LAND;
                work->threadFade = MAGGOT_CATERPILLAR_THREAD_FADE_FRAMES;
            }
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            work->baseMatrix = coord->workm;
            break;
        case MAGGOT_CATERPILLAR_AMBUSH_LAND:
            work->fallSpeed = (work->animFrame >= MAGGOT_CATERPILLAR_AMBUSH_LANDING_FALL_FRAME) << 7;
            if (work->landed != 0) {
                work->step              = MAGGOT_CATERPILLAR_AMBUSH_GET_UP;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_GET_UP;
                work->fallSpeed         = MAGGOT_CATERPILLAR_AMBUSH_GROUND_STEP;
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                soundKey                = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_AMBUSH_LAND_SOUND;
                landingPan              = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, (s32)landingPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case MAGGOT_CATERPILLAR_AMBUSH_GET_UP:
            if (work->animFrame >= MAGGOT_CATERPILLAR_AMBUSH_RECOVERY_FRAMES) {
                sceneEngageBattle(1);
                work->behaviour         = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step              = MAGGOT_CATERPILLAR_AMBUSH_WAIT;
                work->animId            = MAGGOT_CATERPILLAR_ANIM_IDLE;
                work->stateCounter      = _gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + (((RANDOM_LCG_ADVANCE()) >> 0x10) & 0xF);
                work->attackBody.coord  = actor->extra.tmd->coords + 4;
                work->attackBody.radius = 0xC8;
                work->attackBody.pos.vy = 0;
                work->reactionMode      = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
                }
            }
            break;
    }
    _maggotCaterpillarDrawThread(actor);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

/// Alternates timed idle with a crawl toward the player and chooses an attack.
///
/// The placement row must fit the eight-entry idle and crawl-distance tables.
/// Idle timing comes from `_gMaggotCaterpillarIdleDelay`; the crawl counter
/// spends coordinate units, including a random 0..1023-unit spread. A facing
/// Maggot can spray if not burning and the player is not blinded; either kind
/// can pounce in range. Player and actor roots must share a parent frame.
///
/// One 24-byte scratch block stays reserved until the next game-frame reset.
/// Each roaming instance adds another reservation; subsequent scratch users
/// must fit the space left below the cursor.
static void _maggotCaterpillarRoamState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_ROAM_IDLE         = 0,
        MAGGOT_CATERPILLAR_ROAM_CRAWL        = 1,
        MAGGOT_CATERPILLAR_ROAM_SPRAY_RANGE  = 1400,
        MAGGOT_CATERPILLAR_ROAM_TURN_RATE    = 18,
        MAGGOT_CATERPILLAR_ROAM_SPEED        = 23,
        MAGGOT_CATERPILLAR_ROAM_MOTION_FRAME = 11,
        MAGGOT_CATERPILLAR_ROAM_SOUND_FRAME  = 12,
        MAGGOT_CATERPILLAR_ROAM_LOOP_FRAME   = 41,
        MAGGOT_CATERPILLAR_ROAM_SOUND        = 0x401A0001,
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    step;
    s32                    playerDistance;
    s32                    soundKey;
    s32                    playerOffsetX;
    s32                    playerOffsetZ;
    s32                    crawlPan;
    u32                    crawlRandom;
    u32                    idleRandom;
    ActorFaceScratch*      scratch;
    ActorFaceScratch*      scratchEnd;

    scratchEnd                             = SCRATCH_STACK_CURSOR(ActorFaceScratch);
    scratch                                = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(ActorFaceScratch) = scratch;
    work                                   = actor->work;
    step                                   = work->step;
    coord                                  = actor->extra.tmd->coords;
    switch (step) {
        case MAGGOT_CATERPILLAR_ROAM_IDLE:
            work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (--work->stateCounter <= 0) {
                work->step         = MAGGOT_CATERPILLAR_ROAM_CRAWL;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_CRAWL;
                crawlRandom        = RANDOM_LCG_NEXT(gRandomLcgState);
                work->stateCounter = _gMaggotCaterpillarRoamDistance[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((crawlRandom >> 0x10) & 0x3FF);
                gRandomLcgState    = crawlRandom;
                return;
            }
            return;
        case MAGGOT_CATERPILLAR_ROAM_CRAWL:
            scratchEnd[-1].delta.vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            scratch->delta.vy       = 0;
            scratch->delta.vz       = (s32)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
            work->targetYaw         = ratan2((s32)(s16)scratchEnd[-1].delta.vx, (s32)(s16)scratch->delta.vz) & (ONE - 1);
            work->turnRate          = MAGGOT_CATERPILLAR_ROAM_TURN_RATE;
            if (work->animFrame >= MAGGOT_CATERPILLAR_ROAM_MOTION_FRAME) {
                work->forwardSpeed = MAGGOT_CATERPILLAR_ROAM_SPEED;
            }
            work->stateCounter -= work->forwardSpeed;
            if (work->stateCounter <= 0) {
                work->step         = MAGGOT_CATERPILLAR_ROAM_IDLE;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                idleRandom         = RANDOM_LCG_NEXT(gRandomLcgState);
                work->stateCounter = _gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((idleRandom >> 0x10) & 0xF);
                gRandomLcgState    = idleRandom;
                return;
            }
            if (work->animFrame == MAGGOT_CATERPILLAR_ROAM_SOUND_FRAME) {
                soundKey = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_ROAM_SOUND;
                crawlPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, (s32)crawlPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= MAGGOT_CATERPILLAR_ROAM_LOOP_FRAME) {
                work->animFrame = MAGGOT_CATERPILLAR_ROAM_MOTION_FRAME;
            }
            if (work->targetYaw == work->yaw) {
                scratchEnd[-1].delta.vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
                scratch->delta.vy       = 0;
                playerOffsetZ           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                scratch->delta.vz       = playerOffsetZ;
                playerOffsetX           = scratchEnd[-1].delta.vx;
                playerDistance          = SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ));
                if ((work->isCaterpillar == 0) && (playerDistance < MAGGOT_CATERPILLAR_ROAM_SPRAY_RANGE) && (work->burning == 0) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_SPRAY;
                    work->step      = 0;
                    work->animId    = MAGGOT_CATERPILLAR_ANIM_SPRAY;
                    work->puffCount = 0;
                } else if (playerDistance < MAGGOT_CATERPILLAR_POUNCE_RANGE) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE;
                    work->step      = 0;
                    work->animId    = MAGGOT_CATERPILLAR_ANIM_POUNCE;
                }
            }
            break;
    }
}

/// Holds the enemy still while its spray clip emits seven puff children.
///
/// Requires initialized work, a body/puff descriptor run covering index 1,
/// and a placement row in 0..7 for the idle-delay table. Sound plays at tick
/// 40; ticks 43..49 each request a puff, counting attempts even on allocation
/// failure. At clip tick 60 plus its blend length, the enemy returns to roam
/// with the row's idle delay plus a random 0..15 ticks.
static void _maggotCaterpillarSprayState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_SPRAY_SOUND_FRAME      = 40,
        MAGGOT_CATERPILLAR_SPRAY_FIRST_PUFF_FRAME = 43,
        MAGGOT_CATERPILLAR_SPRAY_PUFF_END_FRAME   = 50,
        MAGGOT_CATERPILLAR_SPRAY_CLIP_FRAMES      = 60,
        MAGGOT_CATERPILLAR_SPRAY_SOUND            = 0x401A0003,
        MAGGOT_CATERPILLAR_PUFF_DESCRIPTOR_INDEX  = 1
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    soundKey;
    s32                    audioPan;
    u32                    randomDelay;

    work               = actor->work;
    coord              = actor->extra.tmd->coords;
    work->forwardSpeed = 0;
    work->turnRate     = 0;
    if (work->animFrame == MAGGOT_CATERPILLAR_SPRAY_SOUND_FRAME) {
        soundKey = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_SPRAY_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
    if ((work->animFrame >= MAGGOT_CATERPILLAR_SPRAY_FIRST_PUFF_FRAME) && (work->animFrame < MAGGOT_CATERPILLAR_SPRAY_PUFF_END_FRAME)) {
        enemySpawnFromTable(work->taskTable, MAGGOT_CATERPILLAR_PUFF_DESCRIPTOR_INDEX, 0, actor->spawnArg2.pointer);
        work->puffCount++;
    }
    if (work->animFrame >= (_gMaggotCaterpillarAnimBlendFrames[MAGGOT_CATERPILLAR_ANIM_SPRAY] + MAGGOT_CATERPILLAR_SPRAY_CLIP_FRAMES)) {
        work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
        work->step         = 0;
        work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
        randomDelay        = RANDOM_LCG_NEXT(gRandomLcgState);
        work->stateCounter = _gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((randomDelay >> 0x10) & 0xF);
        gRandomLcgState    = randomDelay;
    }
}

#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Runs the bite leap and the recoil of a blocked or struck leap.
///
/// The leap enables the attack sphere during ticks 27..39 and defers lethal hit
/// reactions until landing. Burning selects elemental bite entries. Nine-entry
/// stride tables provide X/Z displacement in coordinate units, timed against
/// clip ticks; the pounce thresholds include its blend duration. Recoil can
/// finish with a half turn and a slot restart. Releases its rotation scratch.
static void _maggotCaterpillarPounceState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_POUNCE_LEAP            = 0,
        MAGGOT_CATERPILLAR_POUNCE_REBOUND         = 1,
        MAGGOT_CATERPILLAR_POUNCE_ATTACK_START    = 27,
        MAGGOT_CATERPILLAR_POUNCE_LAND_FRAME      = 40,
        MAGGOT_CATERPILLAR_POUNCE_EARLY_BITE_END  = 34,
        MAGGOT_CATERPILLAR_POUNCE_REBOUND_CUTOFF  = 35,
        MAGGOT_CATERPILLAR_POUNCE_RECOVERY_FRAMES = 70,
        MAGGOT_CATERPILLAR_REBOUND_SIDE_START     = 31,
        MAGGOT_CATERPILLAR_REBOUND_SIDE_END       = 46,
        MAGGOT_CATERPILLAR_REBOUND_LAND_FRAME     = 16,
        MAGGOT_CATERPILLAR_POUNCE_LAND_SOUND      = 0x401A0002,
    };
    MaggotCaterpillarWork*             work;
    GfxCoord*                          coord;
    SVECTOR*                           rotation;
    const MaggotCaterpillarStrideStep* pounceStride;
    const MaggotCaterpillarStrideStep* reboundStride;
    s16                                step;
    s32                                soundKey;
    s32                                tableIndex;
    s32                                landingPan;
    s32                                reboundPan;
    u32                                random;

    rotation = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work     = actor->work;
    step     = work->step;
    coord    = actor->extra.tmd->coords;
    switch (step) {
        case MAGGOT_CATERPILLAR_POUNCE_LEAP:
            work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
            work->midLeap      = 0;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            // Contacts during the attack window can turn the leap into recoil.
            if ((work->animFrame >= MAGGOT_CATERPILLAR_POUNCE_ATTACK_START) && (work->animFrame < MAGGOT_CATERPILLAR_POUNCE_LAND_FRAME)) {
                work->reactionMode      = MAGGOT_CATERPILLAR_REACTION_COMMITTED;
                work->midLeap           = 1;
                work->attackBody.flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (work->burning == 0) {
                    tableIndex = work->animFrame < MAGGOT_CATERPILLAR_POUNCE_EARLY_BITE_END ? MAGGOT_CATERPILLAR_ATTACK_BITE_EARLY : MAGGOT_CATERPILLAR_ATTACK_BITE_LATE;
                } else {
                    tableIndex = MAGGOT_CATERPILLAR_ATTACK_BURNING_BITE_LATE;
                    if (work->animFrame < MAGGOT_CATERPILLAR_POUNCE_EARLY_BITE_END) {
                        tableIndex = MAGGOT_CATERPILLAR_ATTACK_BURNING_BITE_EARLY;
                    }
                }
                work->attackBody.key = damagePackAttackKey(_gMaggotCaterpillarAttacks, tableIndex);
                if ((work->animFrame < MAGGOT_CATERPILLAR_POUNCE_REBOUND_CUTOFF) && ((work->struck != 0) || (work->blocked != 0))) {
                    work->step              = MAGGOT_CATERPILLAR_POUNCE_REBOUND;
                    work->midLeap           = 0;
                    work->animId            = MAGGOT_CATERPILLAR_ANIM_REBOUND;
                    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    break;
                }
            } else {
                work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            tableIndex   = 0;
            pounceStride = _gMaggotCaterpillarPounceStride;
            for (; tableIndex < (s32)ARRAY_SIZE(_gMaggotCaterpillarPounceStride); tableIndex++, pounceStride++) {
                if (work->animFrame <= (pounceStride->lastFrame + _gMaggotCaterpillarAnimBlendFrames[MAGGOT_CATERPILLAR_ANIM_POUNCE])) {
                    coord->coord.t[0] += (s32)(pounceStride->forwardStep * rsin(work->yaw)) >> 0xC;
                    coord->coord.t[2] += (s32)(pounceStride->forwardStep * rcos(work->yaw)) >> 0xC;
                    break;
                }
            }
            if (work->animFrame == MAGGOT_CATERPILLAR_POUNCE_LAND_FRAME) {
                soundKey   = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_POUNCE_LAND_SOUND;
                landingPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, (s32)landingPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
                }
            }
            if (work->animFrame >= (_gMaggotCaterpillarAnimBlendFrames[MAGGOT_CATERPILLAR_ANIM_POUNCE] + MAGGOT_CATERPILLAR_POUNCE_RECOVERY_FRAMES)) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                random             = RANDOM_LCG_NEXT(gRandomLcgState);
                work->stateCounter = _gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0xF);
                gRandomLcgState    = random;
            }
            break;
        case MAGGOT_CATERPILLAR_POUNCE_REBOUND:
            tableIndex         = 0;
            reboundStride      = _gMaggotCaterpillarReboundStride;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            for (; tableIndex < (s32)ARRAY_SIZE(_gMaggotCaterpillarReboundStride); tableIndex++, reboundStride++) {
                if (work->animFrame <= reboundStride->lastFrame) {
                    coord->coord.t[0] += (s32)(reboundStride->forwardStep * rsin(work->yaw)) >> 0xC;
                    coord->coord.t[2] += (s32)(reboundStride->forwardStep * rcos(work->yaw)) >> 0xC;
                    break;
                }
            }
            if ((work->animFrame >= MAGGOT_CATERPILLAR_REBOUND_SIDE_START) && (work->animFrame < MAGGOT_CATERPILLAR_REBOUND_SIDE_END)) {
                coord->coord.t[0] += (s32)(rcos(work->yaw) * 0xB) >> 0xC;
                coord->coord.t[2] += (s32)(rsin(work->yaw) * 0xB) >> 0xC;
            }
            if (work->animFrame == MAGGOT_CATERPILLAR_REBOUND_LAND_FRAME) {
                soundKey   = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_POUNCE_LAND_SOUND;
                reboundPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, (s32)reboundPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->reactionMode = MAGGOT_CATERPILLAR_REACTION_REBOUND;
                if (((Enemy*)actor->spawnArg2.pointer)->hp <= 0) {
                    MAGGOT_CATERPILLAR_TURN_AROUND(work, coord, rotation);
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_DEAD;
                    work->step      = 0;
                    actor->state    = MAGGOT_CATERPILLAR_TASK_DYING;
                }
            }
            if (work->animFrame >= MAGGOT_CATERPILLAR_POUNCE_RECOVERY_FRAMES) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                work->stateCounter = _gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + (((RANDOM_LCG_ADVANCE()) >> 0x10) & 0xF);
                work->field_3AA    = 1;
                if (work->reactionMode == MAGGOT_CATERPILLAR_REACTION_REBOUND) {
                    MAGGOT_CATERPILLAR_TURN_AROUND(work, coord, rotation);
                    work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
                }
                // Restart all driven parts after restoring the root facing.
                for (tableIndex = 1; tableIndex < (s32)ARRAY_SIZE(work->rig.slots); tableIndex++) {
                    animationSeekSlotWithBlend(&work->rig.anim, tableIndex, work->animId, 0, 0);
                }
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Plays a stationary hit reaction, then resumes stun or an idle roam phase.
///
/// The hurt clip is forced to restart even if it was already selected. Recovery
/// after 21 ticks retains pending stun; otherwise the idle lasts 0..15 ticks.
/// The placement index selects the spatial sound instance.
static void _maggotCaterpillarHurtState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_HURT_START   = 0,
        MAGGOT_CATERPILLAR_HURT_RECOVER = 1,
        MAGGOT_CATERPILLAR_HURT_FRAMES  = 21,
        MAGGOT_CATERPILLAR_HURT_SOUND   = 0x401A0004,
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    step;
    s32                    soundKey;
    s32                    hurtPan;
    u32                    random;

    work  = actor->work;
    step  = work->step;
    coord = actor->extra.tmd->coords;
    switch (step) {
        case MAGGOT_CATERPILLAR_HURT_START:
            work->animId = MAGGOT_CATERPILLAR_ANIM_HURT;
            // Force the hurt request to differ even when the hurt clip repeats.
            work->appliedAnim  = MAGGOT_CATERPILLAR_ANIM_IDLE;
            work->step         = MAGGOT_CATERPILLAR_HURT_RECOVER;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            soundKey           = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_HURT_SOUND;
            hurtPan            = (s8)worldCoordGetOriginAudioPan(coord);
            sndEvtRequestScriptStart(soundKey, hurtPan, (s8)worldCoordGetOriginAudioDepth(coord));
            return;
        case MAGGOT_CATERPILLAR_HURT_RECOVER:
            if (work->animFrame >= MAGGOT_CATERPILLAR_HURT_FRAMES) {
                if (work->stunned == 1) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_STUN;
                    work->step      = 0;
                    return;
                }
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                random             = RANDOM_LCG_NEXT(gRandomLcgState);
                gRandomLcgState    = random;
                work->stateCounter = (random >> 0x10) & 0xF;
            } else {
                return;
            }
            break;
    }
}

/// Releases a hidden enemy into its scripted leap or drop entrance.
///
/// Requires placement variant 0..3, initialized work and an enemy/model pair.
/// The room release starts a per-variant delay before allocating draw buffers.
/// A drop emits five leaf effects and hands over to the ambush drop; a leap
/// emits three leaves, follows the nine-row stride table, then starts roam.
/// Leaf vectors are coordinate-local spawn offsets, copied during each spawn;
/// their scratch storage is released before return. Idle placement rows are
/// 0..7. Animation counters include the clip's initial blend duration.
static void _maggotCaterpillarEntranceState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_ENTRANCE_HIDDEN           = 0,
        MAGGOT_CATERPILLAR_ENTRANCE_DELAY            = 1,
        MAGGOT_CATERPILLAR_ENTRANCE_START            = 2,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAPING          = 3,
        MAGGOT_CATERPILLAR_ENTRANCE_DROP_LEAVES      = 5,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAP_LEAVES      = 3,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAP_SOUND_FRAME = 30,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAF_FRAME       = 39,
        MAGGOT_CATERPILLAR_ENTRANCE_LAND_FRAME       = 40,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAP_FRAMES      = 70,
        MAGGOT_CATERPILLAR_ENTRANCE_DROP_SOUND       = 0x510D0012,
        MAGGOT_CATERPILLAR_ENTRANCE_LEAP_SOUND       = 0x51090007,
        MAGGOT_CATERPILLAR_ENTRANCE_LAND_SOUND       = 0x401A0002
    };
    TmdObject*                         model;
    MaggotCaterpillarWork*             work;
    GfxCoord*                          coord;
    const MaggotCaterpillarStrideStep* stride;
    SVECTOR*                           scratchEnd;
    SVECTOR*                           spawnOffset;
    s32                                entranceStep;
    s32                                one;
    s32                                dropAudioPan;
    s32                                leapAudioPan;
    s32                                landingAudioPan;
    Enemy*                             enemy;
    s32                                loopIndex;
    s32                                soundKey;
    u32                                randomY;
    u32                                randomZ;
    u32                                randomX;
    u32                                randomDelay;
    u32                                randomRise;

    model        = actor->extra.tmd;
    work         = actor->work;
    enemy        = actor->spawnArg2.pointer;
    scratchEnd   = SCRATCH_STACK_CURSOR(SVECTOR);
    spawnOffset  = (SCRATCH_STACK_CURSOR(SVECTOR) = scratchEnd - 1);
    entranceStep = work->step;
    coord        = model->coords;
    one          = WORLD_TARGET_NOT_LOCKABLE; // Retained shared value for visibility and the release test.
    switch (entranceStep) {
        case MAGGOT_CATERPILLAR_ENTRANCE_HIDDEN:
            work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->gridBody.flags         &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            model->flags                  = (u16)model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            enemy->node.state.parts.flags = one;
            if (gSceneCombatState.maggotCaterpillarEntranceReady == one) {
                if (work->entranceKind == MAGGOT_CATERPILLAR_ENTRANCE_LEAP) {
                    work->stateCounter = _gMaggotCaterpillarLeapInDelay[work->entranceSlot];
                } else {
                    work->stateCounter = _gMaggotCaterpillarDropInDelay[work->entranceSlot];
                }
                work->step = MAGGOT_CATERPILLAR_ENTRANCE_DELAY;
            }
            break;
        case MAGGOT_CATERPILLAR_ENTRANCE_DELAY:
            if (--work->stateCounter <= 0) {
                work->stateCounter = 0;
                work->step         = MAGGOT_CATERPILLAR_ENTRANCE_START;
            }
            break;
        case MAGGOT_CATERPILLAR_ENTRANCE_START:
            // Restore draw storage before handing a drop to the ambush behavior.
            tmdAllocPrimitiveBuffer(model);
            model->flags = (u16)model->flags & (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            loopIndex    = 0;
            if (work->entranceKind == MAGGOT_CATERPILLAR_ENTRANCE_LEAP) {
                work->step         = MAGGOT_CATERPILLAR_ENTRANCE_LEAPING;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_POUNCE;
                work->forwardSpeed = 0;
                work->turnRate     = 0;
                work->yaw          = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
            } else {
                work->animId          = MAGGOT_CATERPILLAR_ANIM_DROP;
                work->behaviour       = MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH;
                work->step            = 1;
                work->fallSpeed       = _gMaggotCaterpillarDropInSpeed[work->entranceSlot];
                work->body.flags     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                do {
                    spawnOffset->vx = 0;
                    spawnOffset->vz = 0;
                    randomRise      = RANDOM_LCG_NEXT(gRandomLcgState);
                    gRandomLcgState = randomRise;
                    spawnOffset->vy = ((randomRise >> 0x10) & 0x1FF) + 0x2EE;
                    effectSpawn(EFFECT_AKROPOLIS_ROOF_GARDEN_LEAF, coord, 0, spawnOffset);
                    loopIndex++;
                } while (loopIndex < MAGGOT_CATERPILLAR_ENTRANCE_DROP_LEAVES);
                soundKey     = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_ENTRANCE_DROP_SOUND;
                dropAudioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, dropAudioPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case MAGGOT_CATERPILLAR_ENTRANCE_LEAPING:
            if (work->animFrame == MAGGOT_CATERPILLAR_ENTRANCE_LEAP_SOUND_FRAME) {
                soundKey     = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_ENTRANCE_LEAP_SOUND;
                leapAudioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, leapAudioPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            loopIndex = 0;
            if (work->animFrame == MAGGOT_CATERPILLAR_ENTRANCE_LEAF_FRAME) {
                do {
                    randomX         = RANDOM_LCG_NEXT(gRandomLcgState);
                    gRandomLcgState = randomX;
                    // These randomized offsets place leaves behind the facing axis.
                    spawnOffset->vx = -((coord->coord.m[0][2] * (s32)(((randomX >> 16) & 0x3F) + 0xAF)) >> 12);
                    randomY         = RANDOM_LCG_NEXT(gRandomLcgState);
                    gRandomLcgState = randomY;
                    spawnOffset->vy = ((randomY >> 16) & 0x1FF) - 0x6D6;
                    randomZ         = RANDOM_LCG_NEXT(gRandomLcgState);
                    gRandomLcgState = randomZ;
                    spawnOffset->vz = -((coord->coord.m[2][2] * (s32)(((randomZ >> 16) & 0x3F) + 0xAF)) >> 12);
                    effectSpawn(EFFECT_AKROPOLIS_FORKED_ROAD_FALLING_LEAF, coord, 0, spawnOffset);
                    loopIndex++;
                } while (loopIndex < MAGGOT_CATERPILLAR_ENTRANCE_LEAP_LEAVES);
                loopIndex = 0;
            }
            stride = _gMaggotCaterpillarPounceStride;
            do {
                loopIndex++;
                if (work->animFrame <= (stride->lastFrame + _gMaggotCaterpillarAnimBlendFrames[MAGGOT_CATERPILLAR_ANIM_POUNCE])) {
                    coord->coord.t[0] += (stride->forwardStep * rsin(work->yaw)) >> 12;
                    coord->coord.t[2] += (stride->forwardStep * rcos(work->yaw)) >> 12;
                    break;
                }
                stride++;
            } while (loopIndex < (s32)ARRAY_SIZE(_gMaggotCaterpillarPounceStride));
            if (work->animFrame == MAGGOT_CATERPILLAR_ENTRANCE_LAND_FRAME) {
                soundKey        = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_ENTRANCE_LAND_SOUND;
                landingAudioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, landingAudioPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->fallSpeed       = MAGGOT_CATERPILLAR_GROUND_FALL_STEP;
                work->body.flags     |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
            }
            if (work->animFrame >= (_gMaggotCaterpillarAnimBlendFrames[MAGGOT_CATERPILLAR_ANIM_POUNCE] + MAGGOT_CATERPILLAR_ENTRANCE_LEAP_FRAMES)) {
                work->behaviour    = MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM;
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                randomDelay        = RANDOM_LCG_NEXT(gRandomLcgState);
                work->stateCounter = _gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((randomDelay >> 0x10) & 0xF);
                gRandomLcgState    = randomDelay;
                sceneEngageBattle(1);
            }
            break;
    }
    SCRATCH_STACK_CURSOR(SVECTOR) = SCRATCH_STACK_CURSOR(SVECTOR) + 1;
}

/// Advances the burning damage cycle, periodic blast effects and positional sound.
///
/// Requires a live enemy model with coordinates 0..5 and initialized work.
/// The 80-tick cycle permits flame-contact damage only at frame zero; effects
/// repeat every 12 ticks and sound every 36. Part arguments alternate between
/// 3 and 5, but the blast uses the effect record's retained root coordinate.
/// Audio uses the owning enemy's placement index and signed-byte pan/depth
/// from the composed root.
static void _maggotCaterpillarBurnStep(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_BURN_CYCLE_TICKS  = 80,
        MAGGOT_CATERPILLAR_BURN_EFFECT_TICKS = 12,
        MAGGOT_CATERPILLAR_BURN_SOUND_TICKS  = 36,
        MAGGOT_CATERPILLAR_BURN_SOUND        = 0x401A0005
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    soundKey;
    s32                    audioPan;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    if (++work->burnFrame >= MAGGOT_CATERPILLAR_BURN_CYCLE_TICKS) {
        work->burnFrame = 0;
    }
    if (++work->burnEffectTimer == MAGGOT_CATERPILLAR_BURN_EFFECT_TICKS) {
        work->burnEffectTimer = 0;
        if (work->burnEffectSide == 0) {
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, actor->extra.tmd->coords + 3, NULL, &work->effectArg);
            work->burnEffectSide = 1;
        } else {
            effectSpawnHit(EFFECT_HIT_KIND_BLAST, actor->extra.tmd->coords + 5, NULL, &work->effectArg);
            work->burnEffectSide = 0;
        }
    }
    if (--work->burnSoundTimer <= 0) {
        work->burnSoundTimer = MAGGOT_CATERPILLAR_BURN_SOUND_TICKS;
        soundKey             = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_BURN_SOUND;
        audioPan             = (s8)worldCoordGetOriginAudioPan(coord);
        sndEvtRequestScriptStart(soundKey, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// Turns the root toward its requested heading and replaces its rotation with level yaw.
///
/// Headings use 4096 units per turn; `turnRate` is a nonnegative angular step.
/// The target must be in the same one-turn domain as the matrix-derived yaw.
/// The difference narrows to s16 before choosing the shorter arc; an exact
/// half-turn takes the wrapped arc. Translation is retained, and the caller
/// must mark the coordinate dirty and recompose it after this step.
static void _maggotCaterpillarTurnStep(Task* actor)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    ActorFaceScratch*      turnScratch;
    s32                    currentYaw;
    u16                    targetYaw;
    s16                    yawDelta;
    s32                    absoluteYawDelta;
    s32                    turnRate;
    s32                    previousYaw;
    s32                    nextYaw;
    s32                    wrappedTurnRate;

    turnScratch      = SCRATCH_STACK_RESERVE_BLOCK(ActorFaceScratch);
    coord            = actor->extra.tmd->coords;
    work             = actor->work;
    currentYaw       = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & ACTOR_TRANSFORM_ANGLE_MASK;
    targetYaw        = work->targetYaw;
    yawDelta         = targetYaw - currentYaw;
    absoluteYawDelta = yawDelta >= 0 ? yawDelta : -yawDelta;

    work->yaw = currentYaw;
    if (absoluteYawDelta < ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
        turnRate = work->turnRate;
        if (turnRate >= absoluteYawDelta) {
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
            previousYaw     = work->yaw;
            if (yawDelta > 0) {
                work->yaw = previousYaw - wrappedTurnRate;
            } else {
                work->yaw = previousYaw + wrappedTurnRate;
            }
        }
    }
    turnScratch->rot.vx = 0;
    turnScratch->rot.vy = work->yaw;
    turnScratch->rot.vz = 0;
    RotMatrix(&turnScratch->rot, &coord->coord);
    SCRATCH_STACK_RELEASE_BLOCK(ActorFaceScratch);
}

/// Restarts changed animation requests or advances the seven driven part slots.
///
/// The live rig, model and clip tables must cover slots 1..7; slot zero is
/// untouched. The blend table must cover the requested `animId`. A restart
/// resets `animFrame` and blends each slot from record zero; an unchanged
/// request increments the frame counter and ticks each slot once.
static inline void _maggotCaterpillarTickAnimInline(Task* task)
{
    MaggotCaterpillarWork* work;
    s32                    slotIndex;
    s32                    blendFrames;

    work = task->work;
    // A changed request captures the current pose for the new clip blend.
    if (work->animId != work->appliedAnim) {
        work->appliedAnim = work->animId;
        work->animFrame   = 0;
        blendFrames       = _gMaggotCaterpillarAnimBlendFrames[work->animId];
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->animId, 0, blendFrames);
        }
    } else {
        work->animFrame++;
        for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
}

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

    /// Advances corpse animation and samples lighting at the cached root translation.
    ///
    /// `actor` is a live TMD task with initialized MaggotCaterpillarWork, slots
    /// 1..7 and a valid requested clip/blend-table index. It is evaluated three
    /// times; `colorCoord` is a writable GfxCoord* lvalue assigned once and read
    /// three times, and `colorPosition` a writable VECTOR lvalue with its pad
    /// untouched. All expressions must be stable and disjoint from inputs.
    /// Changed requests reset/blend the slots; unchanged requests tick them.
    /// Then full-width cached XYZ is sampled without composing or converting
    /// its frame. spawnArg2 borrows the Enemy to colour; no temporary pointer
    /// escapes the lighting query. Captures no caller locals. One statement.
#define MAGGOT_CATERPILLAR_ANIMATE_AND_LIGHT_CORPSE(actor, colorCoord, colorPosition)   \
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
                    MAGGOT_CATERPILLAR_ANIMATE_AND_LIGHT_CORPSE(actor, colorCoord, colorPosition);
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
                    MAGGOT_CATERPILLAR_ANIMATE_AND_LIGHT_CORPSE(actor, colorCoord, colorPosition);
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
#undef MAGGOT_CATERPILLAR_ANIMATE_AND_LIGHT_CORPSE
}

#include "main/random.h"

/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Advances a slowing puff and ends it on a grid contact or after fifteen ticks.
///
/// Paused actors draw the current age without advancing; hidden actors do
/// nothing. Otherwise grid and pair collision tests are enabled every fourth
/// tick. Non-grid contacts are cleared, and movement follows all three local
/// forward-axis components with 12 fractional bits. The enemy parameter is
/// unused by this handler but belongs to its dispatch signature. The caller
/// keeps the parent model alive for drawing.
static void _maggotCaterpillarPuffTick(Enemy* enemy, Task* task)
{
    enum { MAGGOT_CATERPILLAR_PUFF_CONTACT_PERIOD    = 4,
           MAGGOT_CATERPILLAR_PUFF_DECELERATION_MASK = 31 };
    MaggotCaterpillarPuffWork* work;
    GfxCoord*                  coord;
    s32                        contactKey;
    u16                        flags;
    u32                        random;

    coord = task->extra.coordBody->coord;
    work  = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _maggotCaterpillarDrawPuff(task, work->age);
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            contactKey = work->contacts[0].key.value;
            if (contactKey != 0) {
                if ((contactKey & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_GRID) {
                    work->body.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCollisionClearContacts(work->contacts);
                } else {
                    worldCollisionUnlinkBody(&work->body);
                    task->state = MAGGOT_CATERPILLAR_PUFF_TASK_DESTROY;
                    return;
                }
            }
            // Test collision once per four flight ticks.
            if (!(work->age & (MAGGOT_CATERPILLAR_PUFF_CONTACT_PERIOD - 1))) {
                flags = work->body.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else {
                flags = work->body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            work->body.flags    = flags;
            coord->coord.t[0]  += (s32)(coord->coord.m[0][2] * work->forwardSpeed) >> 12;
            coord->coord.t[1]  += (s32)(coord->coord.m[1][2] * work->forwardSpeed) >> 12;
            coord->coord.t[2]  += (s32)(coord->coord.m[2][2] * work->forwardSpeed) >> 12;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            _maggotCaterpillarDrawPuff(task, work->age);
            work->age++;
            if (work->age >= MAGGOT_CATERPILLAR_PUFF_LIFETIME) {
                worldCollisionUnlinkBody(&work->body);
                task->state = MAGGOT_CATERPILLAR_PUFF_TASK_DESTROY;
                return;
            }
            random              = RANDOM_LCG_NEXT(gRandomLcgState);
            gRandomLcgState     = random;
            work->forwardSpeed -= (random >> 0x10) & MAGGOT_CATERPILLAR_PUFF_DECELERATION_MASK;
            if (work->forwardSpeed < 0) {
                work->forwardSpeed = 0;
            }
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            return;
    }
}

/// Draws the puff as a growing textured billboard using its parent actor atlas.
///
/// The task must own a live coordinate body and have a live TMD parent. Its
/// coordinate must already be composed. The frame is the puff age, 0..14; the
/// cell changes every two ticks. The radius table covers ages 0..13; age 14
/// retains a read of the adjacent body recipe flags (1). Its intended storage
/// relationship is unproven.
/// Depth is projected Z / 4; values below 20 emit no primitive. One scratch
/// quad is reserved and released on every path.
static void _maggotCaterpillarDrawPuff(Task* actor, s32 frame)
{
    enum { MAGGOT_CATERPILLAR_PUFF_NEAR_DEPTH              = 20,
           MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL         = 31,
           MAGGOT_CATERPILLAR_PUFF_RADIUS_PROJECTION_SCALE = 0x300 };
    POLY_FT4*               quad;
    GfxCoord*               coord;
    s32                     depth;
    s32                     screenCentre;
    s32                     y;
    s32                     radius;
    s32                     x;
    s32                     bottom;
    s32                     top;
    s32                     left;
    s32                     right;
    ActorScreenQuadScratch* scratchEnd;
    ActorScreenQuadScratch* scratch;
    TmdObject*              parentModel;
    ActorSpriteUv*          cell;
    SVECTOR*                projectedPoint;

    // Project the composed origin before reusing the vector storage as corners.
    scratchEnd                   = (ActorScreenQuadScratch*)SCRATCH_STACK_CURSOR(u8);
    coord                        = actor->extra.coordBody->coord;
    actor                        = actor->parent;
    parentModel                  = actor->extra.tmd;
    scratchEnd[-1].corners[0].vx = (u16)coord->workm.t[0];
    scratch                      = scratchEnd - 1;
    scratch->corners[0].vy       = (u16)coord->workm.t[1];
    SCRATCH_STACK_CURSOR(u8)     = (u8*)scratch;
    scratch->corners[0].vz       = (u16)coord->workm.t[2];
    projectedPoint               = &scratch->corners[0];
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_ldv0(projectedPoint);
    gte_rtps();
    gte_stsxy(&scratchEnd[-1].screenCentre);
    gte_stszotz(&scratchEnd[-1].otz);
    depth = scratch->otz;
    if (depth >= MAGGOT_CATERPILLAR_PUFF_NEAR_DEPTH) {
        radius                       = (s32)(_gMaggotCaterpillarPuffRadius[frame] * MAGGOT_CATERPILLAR_PUFF_RADIUS_PROJECTION_SCALE) / depth;
        quad                         = gGpuPrimCursor;
        screenCentre                 = scratch->screenCentre;
        gGpuPrimCursor               = quad + 1;
        x                            = screenCentre & 0xFFFF;
        y                            = screenCentre >> 0x10;
        left                         = x - radius;
        top                          = y - radius;
        right                        = x + radius;
        bottom                       = y + radius;
        scratchEnd[-1].corners[0].vx = left;
        scratch->corners[0].vy       = top;
        scratch->corners[0].vz       = 0;
        scratch->corners[1].vx       = right;
        scratch->corners[1].vy       = top;
        scratch->corners[1].vz       = 0;
        scratch->corners[2].vx       = left;
        scratch->corners[2].vy       = bottom;
        scratch->corners[2].vz       = 0;
        scratch->corners[3].vx       = right;
        scratch->corners[3].vy       = bottom;
        scratch->corners[3].vz       = 0;
        setPolyFT4(quad);
        setSemiTrans(quad, 1);
        setRGB0(quad, 0x80, 0x80, 0x80);
        setShadeTex(quad, 1);
        quad->tpage = (s16)(((s32)(((parentModel->texturePageOffset << 6) + 0x180) & 0x3FF) >> 6) | 0xB0);
        quad->clut  = (s16)(((s32)((u8)parentModel->clutRowOffset << 0x18) >> 0x12) + 0x3D40);
        cell        = &_gMaggotCaterpillarPuffCells[frame >> 1];
        quad->u0    = cell->u;
        quad->v0    = cell->v;
        quad->u1    = cell->u + MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL;
        quad->v1    = cell->v;
        quad->u2    = cell->u;
        quad->v2    = cell->v + MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL;
        quad->u3    = cell->u + MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL;
        quad->v3    = cell->v + MAGGOT_CATERPILLAR_PUFF_CELL_LAST_PIXEL;
        quad->x0    = (u16)scratchEnd[-1].corners[0].vx;
        quad->y0    = (u16)scratch->corners[0].vy;
        quad->x1    = (u16)scratch->corners[1].vx;
        quad->y1    = (u16)scratch->corners[1].vy;
        quad->x2    = (u16)scratch->corners[2].vx;
        quad->y2    = (u16)scratch->corners[2].vy;
        quad->x3    = (u16)scratch->corners[3].vx;
        quad->y3    = (u16)scratch->corners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->otz), quad);
    }
    SCRATCH_STACK_CURSOR(u8) += sizeof(ActorScreenQuadScratch);
}

/// Draws the fading vertical thread of a hanging or falling actor.
///
/// Endpoints lie on the Y axis of `baseMatrix`, with the upper endpoint offset
/// from the lower by `vertical.threadRise`. Both must project to Z / 4 >= 30;
/// the lower endpoint supplies ordering depth. Zero fade draws full brightness;
/// other values count down from 45 and stay at one. Releases its scratch block
/// on every path and queues a semitransparent Gouraud line and draw mode.
static void _maggotCaterpillarDrawThread(Task* actor)
{
    enum { MAGGOT_CATERPILLAR_THREAD_NEAR_DEPTH = 30,
           MAGGOT_CATERPILLAR_THREAD_LOWER_Y    = -0x352 };
    MaggotCaterpillarLineScratch* scratch;
    MaggotCaterpillarWork*        work;
    LINE_G2*                      line;
    DR_TPAGE*                     drawMode;
    s32                           upperX;
    s32                           upperY;
    s32                           upperScreenPos;
    s32                           lowerScreenPos;

    /// Projects one thread endpoint on the saved composed frame's local Y axis.
    ///
    /// `scratch` is a live writable MaggotCaterpillarLineScratch pointer and
    /// `work` supplies the saved world/view baseMatrix; both repeat and must
    /// be stable and side-effect-free. Height is evaluated once in local game
    /// units and narrows to s16. Writes position XYZ, packed screen XY and
    /// projected Z/4 depth; changes GTE matrices and projection state, retaining
    /// no pointers. A standalone statement list, used only in a braced scope
    /// and undefined after this function's two calls; no projection-flag test.
#define MAGGOT_CATERPILLAR_PROJECT_THREAD_END(scratch, work, height) \
    (scratch)->position.vx = 0;                                      \
    (scratch)->position.vy = (height);                               \
    (scratch)->position.vz = 0;                                      \
    gte_SetRotMatrix(&(work)->baseMatrix);                           \
    gte_SetTransMatrix(&(work)->baseMatrix);                         \
    gte_ldv0(&(scratch)->position);                                  \
    gte_rtps();                                                      \
    gte_stsxy(&(scratch)->screenPos);                                \
    gte_stszotz(&(scratch)->depth)

    scratch = SCRATCH_STACK_RESERVE_BLOCK(MaggotCaterpillarLineScratch);
    work    = actor->work;
    MAGGOT_CATERPILLAR_PROJECT_THREAD_END(scratch, work, work->vertical.threadRise + MAGGOT_CATERPILLAR_THREAD_LOWER_Y);
    if (scratch->depth < MAGGOT_CATERPILLAR_THREAD_NEAR_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarLineScratch);
        return;
    }
    upperScreenPos = scratch->screenPos;
    upperX         = upperScreenPos & 0xFFFF;
    upperY         = upperScreenPos >> 16;
    MAGGOT_CATERPILLAR_PROJECT_THREAD_END(scratch, work, MAGGOT_CATERPILLAR_THREAD_LOWER_Y);
    if (scratch->depth < MAGGOT_CATERPILLAR_THREAD_NEAR_DEPTH) {
        SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarLineScratch);
        return;
    }
    line           = gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    lowerScreenPos = scratch->screenPos;
    setLineG2(line);
    setSemiTrans(line, 1);
    line->x0 = upperX;
    line->y0 = upperY;
    line->x1 = lowerScreenPos;
    line->y1 = lowerScreenPos >> 16;
    if (work->threadFade == 0) {
        line->b0 = line->g0 = line->r0 = 0x80;
        line->b1 = line->g1 = line->r1 = 0xC0;
    } else {
        if (--work->threadFade <= 0) {
            work->threadFade = 1;
        }
        line->r0 = (work->threadFade * 0x80) / MAGGOT_CATERPILLAR_THREAD_FADE_FRAMES;
        line->g0 = line->r0;
        line->b0 = line->r0;
        line->r1 = (work->threadFade * 0xC0) / MAGGOT_CATERPILLAR_THREAD_FADE_FRAMES;
        line->g1 = line->r1;
        line->b1 = line->r1;
    }
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth), line);
    // The draw mode is linked last so the GPU executes it before the line.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 1, 1, getTPage(0, 1, 0, 0));
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_DEPTH(scratch->depth), drawMode);
    SCRATCH_STACK_RELEASE_BLOCK(MaggotCaterpillarLineScratch);
#undef MAGGOT_CATERPILLAR_PROJECT_THREAD_END
}
