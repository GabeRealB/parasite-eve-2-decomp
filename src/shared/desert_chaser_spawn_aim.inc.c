/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Plays the far-catch throw and prepares the held player's forward displacement.
///
/// Requires the armed chaser's live enemy/model and player task. Entry plays
/// clip 5, limits the recorded lunge distance to 4000 world units and stores a
/// 133-unit step along the facing for the frame driver to send to the player.
/// Tick 15 emits the throw cue if player collision is still requested. Once
/// the clip settles, the chaser leaps back, or flees on Water Tower command 1.
/// Sixteen scratch bytes are reserved per call; only the first vector is used.
static void _desertChaserThrowPlayer(Task* task)
{
    // Throw ticks, world-unit distances and the dust effect's packed options.
    enum {
        DESERT_CHASER_CLIP_THROW               = 5,
        DESERT_CHASER_THROW_MAX_LUNGE_DISTANCE = 4000,
        DESERT_CHASER_THROW_PLAYER_STEP        = 133,
        DESERT_CHASER_THROW_CUE_TICK           = 15,
        DESERT_CHASER_SOUND_THROW              = 0x4001000A,
        DESERT_CHASER_THROW_DUST_ARGUMENT      = DESERT_CHASER_CUE_DUST_RECURSIVE | (3 << DESERT_CHASER_CUE_DUST_PERIOD_SHIFT) | 0xA00
    };

    Enemy*            enemy;
    DesertChaserWork* work;
    SVECTOR*          throwStep;
    TmdObject*        model;
    s32               x;
    s32               z;
    s32               throwSound;
    s32               entryPan;
    s32               eventPan;
    s32               nextState;
    u16               elapsedFrames;
    Task*             player;

    work      = task->work;
    player    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    throwStep = SCRATCH_STACK_RESERVE_BYTES(2 * sizeof(*throwStep));
    enemy     = task->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        model                         = task->extra.tmd;
        enemy->node.state.parts.flags = 0;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->hitFlag = 0;
#endif
        model->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = DESERT_CHASER_FRONT_RADIUS;
        work->animId                                          = DESERT_CHASER_CLIP_THROW;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        _desertChaserAnimTick(task);
        // The frame driver sends this facing-aligned push to the held player.
        gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, throwStep);
#if !DESERT_CHASER_RUN_SEQUENCE
        work->playerAnimFrames = 0;
#endif
        work->stateTimer = 0;
        VectorNormalSS(throwStep, throwStep);
        if (work->lungeDistance > DESERT_CHASER_THROW_MAX_LUNGE_DISTANCE) {
            work->lungeDistance = DESERT_CHASER_THROW_MAX_LUNGE_DISTANCE;
        }
        gte_lddp(DESERT_CHASER_THROW_PLAYER_STEP);
        gte_ldsv(throwStep);
        gte_gpf12();
        gte_stsv(throwStep);
        x                                  = throwStep->vx;
        work->playerMove.displacement.vy   = 0;
        work->playerMove.displacement.vx   = x;
        z                                  = throwStep->vz;
        work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        work->playerMove.keepControl       = 1;
        work->playerMove.displacement.vz   = z;
        entryPan                           = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(SOUND_COMMON(7), (s32)entryPan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
#if !DESERT_CHASER_RUN_SEQUENCE
        padScriptSpawnVariableMotorRamp(8, 0xFF, 8);
#endif
    }
    elapsedFrames    = work->stateTimer + 1;
    work->stateTimer = elapsedFrames;
    if (((s16)elapsedFrames == DESERT_CHASER_THROW_CUE_TICK) && (work->playerMove.collisionRequests == GAME_ACTOR_COLLISION_REQUEST_MASK)) {
        throwSound = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | DESERT_CHASER_SOUND_THROW;
        eventPan   = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(throwSound, (s32)eventPan, (s32)(s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
            effectSpawn(EFFECT_DUST_PUFF, player->extra.tmd->coords + 1, DESERT_CHASER_THROW_DUST_ARGUMENT, NULL);
        }
    }
    _desertChaserAnimTick(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED) {
#if !DESERT_CHASER_RUN_SEQUENCE
        work->state = DESERT_CHASER_STATE_LEAP_BACK;
#else
        nextState = work->lastCommand.word & DESERT_CHASER_COMMAND_MASK;
        if (nextState == DESERT_CHASER_COMMAND_WATER_TOWER_1) {
            nextState = DESERT_CHASER_STATE_FLEE;
        } else {
            nextState = DESERT_CHASER_STATE_LEAP_BACK;
        }
        work->state = nextState;
#endif
    }
    SCRATCH_STACK_RELEASE_BYTES(2 * sizeof(*throwStep));
}
