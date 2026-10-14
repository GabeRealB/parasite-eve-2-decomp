/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Normalizes an XZ offset and scales it to the grab's 1000-unit separation.
///
/// `separation` borrows writable signed-halfword XYZ components with Y zero;
/// normalization produces Q12 components before scaling to parent-space units.
/// Requires a nonzero XZ offset whose squared length fits a positive s32.
/// Its unused fourth halfword stays intact; normalization and scaling clobber GTE state.
static inline void _oddStrangerScaleGrabSeparation(SVECTOR* separation)
{
    enum { ODD_STRANGER_GRAB_DISTANCE = 1000 };

    VectorNormalSS(separation, separation);
    gte_lddp(ODD_STRANGER_GRAB_DISTANCE);
    gte_ldsv(separation);
    gte_gpf12();
    gte_stsv(separation);
}

/// Applies the pull's joint pitch and composes each changed joint in order.
///
/// Borrows the live task's model coordinates, including parts 2..5, after
/// animation has written their local pose. The two rotations compose a -128
/// pitch (4096 units per turn); dirty stamps on parts 4 and 5 make later
/// descendant composition consume that pose. Parts 2 then 3 are composed now.
static inline void _oddStrangerApplyGrabPullPitch(Task* task)
{
    gfxRotMatrixX(&task->extra.tmd->coords[2].coord, ODD_STRANGER_GRAB_PITCH, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[2]);
    gfxRotMatrixX(&task->extra.tmd->coords[3].coord, ODD_STRANGER_GRAB_PITCH, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[3]);
}

/// Places the held player and the enemy for the grab strike, then plays the pull.
///
/// Handles `ODD_STRANGER_STATE_GRAB_PULL` on a live Odd Stranger task with both
/// animation rigs bound. The player task and both models must remain live;
/// their roots use the same parent-coordinate space. On entry the player keeps
/// its position and faces the enemy, which moves 1000 units away along that XZ
/// bearing. The bearing calculation narrows X/Z offsets to signed halfwords.
/// The static placement is borrowed only for the synchronous message dispatch.
/// The pull boundary emits hit puffs and selects `ODD_STRANGER_STATE_GRAB_STRIKE`.
static void _oddStrangerGrabPull(Task* task)
{
    enum {
        ODD_STRANGER_GRAB_PULL_EFFECT_ARG_LOW = 0x200, // Retained low half; the puff recipe only reads the high half
        ODD_STRANGER_GRAB_PULL_RAMP_FRAMES    = 12,
        ODD_STRANGER_GRAB_PULL_RAMP_START     = 8,
        ODD_STRANGER_GRAB_PULL_RAMP_END       = 143
    };
    SVECTOR          playerToEnemy;
    OddStrangerWork* work;
    Enemy*           enemy;
    Task*            player;
    SVECTOR*         separation;
    ActorTransform*  playerPlacement;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        player                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->hitBody.radius          = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags       &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags         |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = 0;
        work->animRequest             = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                = ANIMATION_RATE_ONE;
        work->animId                  = ODD_STRANGER_ANIM_GRAB_PULL;
        // Preserve the player's origin while turning it toward the enemy.
        player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(player->extra.tmd->coords);
        playerPlacement         = &_gOddStrangerPlayerGrabPlacement.placement;
        playerPlacement->pos.vx = player->extra.tmd->coords->coord.t[0];
        playerPlacement->pos.vy = player->extra.tmd->coords->coord.t[1];
        playerPlacement->pos.vz = player->extra.tmd->coords->coord.t[2];
        separation              = &playerToEnemy;
        playerToEnemy.vx        = (u16)task->extra.tmd->coords->coord.t[0] - (u16)player->extra.tmd->coords->coord.t[0];
        playerToEnemy.vy        = 0;
        playerToEnemy.vz        = (u16)task->extra.tmd->coords->coord.t[2] - (u16)player->extra.tmd->coords->coord.t[2];
        _oddStrangerScaleGrabSeparation(separation);
        task->extra.tmd->coords->coord.t[0]   = player->extra.tmd->coords->coord.t[0] + playerToEnemy.vx;
        task->extra.tmd->coords->coord.t[2]   = player->extra.tmd->coords->coord.t[2] + playerToEnemy.vz;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        playerPlacement->rot.vx               = 0;
        playerPlacement->rot.vy               = ratan2(playerToEnemy.vx, playerToEnemy.vz);
        playerPlacement->rot.vz               = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, playerPlacement, 0);
        padScriptSpawnVariableMotorRamp(ODD_STRANGER_GRAB_PULL_RAMP_FRAMES, ODD_STRANGER_GRAB_PULL_RAMP_START, ODD_STRANGER_GRAB_PULL_RAMP_END);
    }
    _oddStrangerDriveAnimation(task);
    _oddStrangerApplyGrabPullPitch(task);
    if (work->animId == ODD_STRANGER_ANIM_GRAB_PULL && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401800
        work->state = ODD_STRANGER_STATE_GRAB_STRIKE;
#endif
        work->effectArg.coord      = task->extra.tmd->coords + ODD_STRANGER_GRAB_PULL_EFFECT_RECORD_PART;
        work->effectArg.spawnArgLo = ODD_STRANGER_GRAB_PULL_EFFECT_ARG_LOW;
        work->effectArg.spawnArgHi = ODD_STRANGER_GRAB_EXTRA_PUFF_COUNT;
        effectSpawnHit(damageGetPlayerAttackEffectId(ODD_STRANGER_GRAB_HIT_EFFECT_KEY), task->extra.tmd->coords + 5, NULL, &work->effectArg);
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401000
        work->state = ODD_STRANGER_STATE_GRAB_STRIKE;
#endif
    }
}

/// Applies the strike's joint pitch with its retained cross-joint compose order.
///
/// Borrows the live task's model coordinates, including parts 2..5, after
/// animation has written their local pose. The rotations compose a -128 pitch
/// (4096 units per turn) and invalidate parts 4 and 5. Part 3 is composed
/// after part 2's rotation; part 2 is composed after part 3's rotation.
static inline void _oddStrangerApplyGrabStrikePitch(Task* task)
{
    gfxRotMatrixX(&task->extra.tmd->coords[2].coord, ODD_STRANGER_GRAB_PITCH, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[3]);
    gfxRotMatrixX(&task->extra.tmd->coords[3].coord, ODD_STRANGER_GRAB_PITCH, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[2]);
}

/// Strikes the held player with the enemy's first attack and requests the strike pose.
///
/// Handles `ODD_STRANGER_STATE_GRAB_STRIKE` on a live Odd Stranger task with
/// bound rigs and an existing player task/model. The grab's character-specific
/// player animation bank must already be selected and contain animation 2.
/// Its table and clip data must remain live through the player's playback.
/// Entry lends the static animation request for synchronous dispatch, applies
/// attack-table element 0, and starts a five-frame vibration ramp.
/// The existing primary-slot boundary flags select `ODD_STRANGER_STATE_GRAB_RELEASE`
/// before this call resets or advances playback. `grabAnimFrame` stores the
/// low ten record-index bits before that advance, rather than elapsed frames.
static void _oddStrangerGrabStrike(Task* task)
{
    enum {
        ODD_STRANGER_PLAYER_ANIM_GRAB_STRIKE = 2,
        ODD_STRANGER_GRAB_ATTACK_INDEX       = 0,
        ODD_STRANGER_GRAB_STRIKE_RAMP_FRAMES = 5,
        ODD_STRANGER_GRAB_STRIKE_RAMP_START  = 255,
        ODD_STRANGER_GRAB_STRIKE_RAMP_END    = 8
    };
    OddStrangerWork*      work;
    AnimationPlayRequest* playerAnimation;
    Enemy*                enemy;
    Task*                 player;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        work->animRate               = ANIMATION_RATE_ONE;
        work->animId                 = ODD_STRANGER_ANIM_GRAB_STRIKE;
        work->animRequest            = ODD_STRANGER_ANIM_REQUEST_RESET;
        playerAnimation              = &_gOddStrangerPlayerGrabAnimationRequest;
        playerAnimation->animationId = ODD_STRANGER_PLAYER_ANIM_GRAB_STRIKE;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, playerAnimation, 0);
        player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ODD_STRANGER_GRAB_ATTACK_INDEX), 0);
        padScriptSpawnVariableMotorRamp(ODD_STRANGER_GRAB_STRIKE_RAMP_FRAMES, ODD_STRANGER_GRAB_STRIKE_RAMP_START, ODD_STRANGER_GRAB_STRIKE_RAMP_END);
    }
    // Consume the prior tick's boundary before applying the pending restart.
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401800
        work->state = ODD_STRANGER_STATE_GRAB_RELEASE;
#endif
        work->effectArg.coord      = task->extra.tmd->coords + 1;
        work->effectArg.spawnArgLo = ODD_STRANGER_PUFF_UNUSED_LOW_ARG;
        work->effectArg.spawnArgHi = ODD_STRANGER_GRAB_EXTRA_PUFF_COUNT;
        effectSpawnHit(damageGetPlayerAttackEffectId(ODD_STRANGER_GRAB_HIT_EFFECT_KEY), task->extra.tmd->coords + 5, NULL, &work->effectArg);
#if ODD_STRANGER_BUILD == ODD_STRANGER_BUILD_401000
        work->state = ODD_STRANGER_STATE_GRAB_RELEASE;
#endif
    }
    work->grabAnimFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    _oddStrangerDriveAnimation(task);
    _oddStrangerApplyGrabStrikePitch(task);
}

/// Ends the player's hold, backs away during the release clip and resumes combat.
///
/// Handles `ODD_STRANGER_STATE_GRAB_RELEASE` on a live Odd Stranger task with
/// bound rigs and a live player task/model in the same root-parent space.
/// Entry sends player clip 3 only while HP is positive. If still held, a living
/// player is released once that clip stops or at the enemy clip boundary.
/// Enemy records 16..22 allow a -120-unit step; grid pushback halves that
/// signed step with truncation toward zero. At the boundary a targeted enemy
/// with clear sight selects `SIDESTEP`, otherwise `ALERT`. The animation
/// request is lent from overlay-static storage only during synchronous dispatch.
static void _oddStrangerGrabRelease(Task* task)
{
    enum {
        ODD_STRANGER_PLAYER_ANIM_GRAB_RELEASE  = 3,
        ODD_STRANGER_GRAB_RELEASE_STEP         = -120,
        ODD_STRANGER_GRAB_RELEASE_FIRST_RECORD = 16,
        ODD_STRANGER_GRAB_RELEASE_RECORD_COUNT = 7
    };
    OddStrangerWork*      work;
    Enemy*                enemy;
    AnimationPlayRequest* playerAnimation;
    PlayerStatus*         playerStatus;
    u8                    targeted;

    work         = task->work;
    enemy        = task->spawnArg2.pointer;
    playerStatus = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->animRate    = ANIMATION_RATE_ONE;
        work->animId      = ODD_STRANGER_ANIM_GRAB_RELEASE;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
        _oddStrangerDriveAnimation(task);
        playerAnimation              = &_gOddStrangerPlayerGrabAnimationRequest;
        playerAnimation->animationId = ODD_STRANGER_PLAYER_ANIM_GRAB_RELEASE;
        if (playerStatus->hp > 0) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, playerAnimation, 0);
        }
        work->releaseStep     = ODD_STRANGER_GRAB_RELEASE_STEP;
        work->stateTimer      = 0;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        return;
    }
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0 && playerStatus->hp > 0 && work->playerHeld == 1) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        work->playerHeld = 0;
    }
    // Only the release clip's retreat records move the enemy away from the player.
    if ((u32)((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - ODD_STRANGER_GRAB_RELEASE_FIRST_RECORD) < (u32)ODD_STRANGER_GRAB_RELEASE_RECORD_COUNT) {
        if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, work->releaseStep) != 0) {
            _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, (u16)work->releaseStep);
        }
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1) {
            work->releaseStep = work->releaseStep / 2;
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    _oddStrangerDriveAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        targeted = enemy->node.state.parts.targeted;
        if (targeted == 1) {
            if (_playerDetectionSightBlocked(task) == targeted) {
                work->state = ODD_STRANGER_STATE_ALERT;
            } else {
                work->state = ODD_STRANGER_STATE_SIDESTEP;
            }
        } else {
            work->state = ODD_STRANGER_STATE_ALERT;
        }
        if (playerStatus->hp > 0 && work->playerHeld == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work->playerHeld = 0;
        }
    }
}
