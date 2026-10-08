/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Normalizes an XZ offset and scales it to the grab's 1000-unit separation.
///
/// `separation` borrows writable signed-halfword XYZ components with Y zero;
/// normalization produces Q12 components before scaling to parent-space units.
/// Requires a nonzero XZ offset whose squared length fits a positive s32.
/// Its unused fourth halfword stays intact; normalization and scaling clobber GTE state.
static __inline__ void _oddStrangerScaleGrabSeparation(SVECTOR* separation)
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
static __inline__ void _oddStrangerApplyGrabPullPitch(Task* task)
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
        playerPlacement         = &gOddStrangerGrabTransform.placement;
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
#if ODD_STRANGER_VARIANT == 2
        work->state = ODD_STRANGER_STATE_GRAB_STRIKE;
#endif
        work->effectArg.coord      = task->extra.tmd->coords + ODD_STRANGER_GRAB_FX_PART;
        work->effectArg.spawnArgLo = ODD_STRANGER_GRAB_PULL_EFFECT_ARG_LOW;
        work->effectArg.spawnArgHi = ODD_STRANGER_GRAB_EXTRA_PUFF_COUNT;
        effectSpawnHit(damageGetPlayerAttackEffectId(ODD_STRANGER_GRAB_HIT_EFFECT_KEY), task->extra.tmd->coords + 5, NULL, &work->effectArg);
#if ODD_STRANGER_VARIANT == 1
        work->state = ODD_STRANGER_STATE_GRAB_STRIKE;
#endif
    }
}
