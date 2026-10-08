/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Applies the strike's joint pitch with its retained cross-joint compose order.
///
/// Borrows the live task's model coordinates, including parts 2..5, after
/// animation has written their local pose. The rotations compose a -128 pitch
/// (4096 units per turn) and invalidate parts 4 and 5. Part 3 is composed
/// after part 2's rotation; part 2 is composed after part 3's rotation.
static __inline__ void _oddStrangerApplyGrabStrikePitch(Task* task)
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
        playerAnimation              = &gOddStrangerPlayerAnim;
        playerAnimation->animationId = ODD_STRANGER_PLAYER_ANIM_GRAB_STRIKE;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, playerAnimation, 0);
        player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        taskMessageDispatch(player, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, damagePackEnemyAttackKey(enemy, ODD_STRANGER_GRAB_ATTACK_INDEX), 0);
        padScriptSpawnVariableMotorRamp(ODD_STRANGER_GRAB_STRIKE_RAMP_FRAMES, ODD_STRANGER_GRAB_STRIKE_RAMP_START, ODD_STRANGER_GRAB_STRIKE_RAMP_END);
    }
    // Consume the prior tick's boundary before applying the pending restart.
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
#if ODD_STRANGER_VARIANT == 2
        work->state = ODD_STRANGER_STATE_GRAB_RELEASE;
#endif
        work->effectArg.coord      = task->extra.tmd->coords + 1;
        work->effectArg.spawnArgLo = ODD_STRANGER_PART1_FX_SCALE;
        work->effectArg.spawnArgHi = ODD_STRANGER_GRAB_EXTRA_PUFF_COUNT;
        effectSpawnHit(damageGetPlayerAttackEffectId(ODD_STRANGER_GRAB_HIT_EFFECT_KEY), task->extra.tmd->coords + 5, NULL, &work->effectArg);
#if ODD_STRANGER_VARIANT == 1
        work->state = ODD_STRANGER_STATE_GRAB_RELEASE;
#endif
    }
    work->grabAnimFrame = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
    _oddStrangerDriveAnimation(task);
    _oddStrangerApplyGrabStrikePitch(task);
}
