/* Part of the Glutton library; see glutton.h. */

/// Applies the current area's escort texture offsets to a live model.
///
/// Requires at least three placements in the current variant. Placement row 2
/// supplies the page and CLUT-row offsets. An existing primitive buffer has
/// both halves rebuilt; this does not allocate a buffer or take ownership.
static __inline__ void _gluttonApplyEscortTextureOffsets(TmdObject* model)
{
    enum { GLUTTON_ESCORT_PLACEMENT_INDEX = 2 };
    const AreaPlacement* placement;

    placement                = &(_areaGetCurrentVariant()->placements)[GLUTTON_ESCORT_PLACEMENT_INDEX];
    model->texturePageOffset = placement->texturePageOffset;
    model->clutRowOffset     = placement->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}

/// Shows the healing host and live escorts and restores their primitive buffers.
///
/// Clears the host's deferred-free countdown and all host/live-escort draw flags.
/// Visits every escort slot, skipping NULLs. Incinerator allocation is guarded
/// by a missing buffer; dumping-hole calls allocation unconditionally. Requires
/// live host/work/escort models; primitive-buffer ownership stays with each model.
static inline void _gluttonPrepareHealModels(Task* task)
{
    GluttonWork* drawWork;
    GluttonWork* bufferWork;
    s16          escortIndex;
    s16          bufferIndex;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
    TmdObject* hostModel;
    TmdObject* escortModel;
#endif
    drawWork                = task->work;
    drawWork->freeCountdown = 0;
    task->extra.tmd->flags  = 0;
    for (escortIndex = 0; escortIndex < ARRAY_SIZE(drawWork->escorts); escortIndex++) {
        if (drawWork->escorts[escortIndex] != NULL) {
            drawWork->escorts[escortIndex]->task->extra.tmd->flags =
                task->extra.tmd->flags;
        }
    }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    bufferWork = task->work;
    tmdAllocPrimitiveBuffer(task->extra.tmd);
#else
    hostModel  = task->extra.tmd;
    bufferWork = task->work;
    if (hostModel->buffer == NULL) {
        tmdAllocPrimitiveBuffer(hostModel);
    }
#endif
    for (bufferIndex = 0; bufferIndex < ARRAY_SIZE(bufferWork->escorts); bufferIndex++) {
        if (bufferWork->escorts[bufferIndex] != NULL) {
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
            tmdAllocPrimitiveBuffer(bufferWork->escorts[bufferIndex]->task->extra.tmd);
#else
            escortModel = bufferWork->escorts[bufferIndex]->task->extra.tmd;
            if (escortModel->buffer == NULL) {
                tmdAllocPrimitiveBuffer(escortModel);
            }
#endif
        }
    }
}

/// Presents the Glutton's healing sequence and releases its debris chunks.
///
/// Requires live host work/enemy, player root, seven escort slots and rigs.
/// Entry shows host/escorts, restores primitive buffers and arms neck yaw;
/// the incinerator allocates only absent buffers, while the dumping-hole
/// variant calls allocation on every model. Positive signed pendingHeals at
/// ticks 100/260 consumes one and requests the heal clip; no pending cue or
/// tick 420 returns to attack selection. Ticks 175/325 spawn descriptor 3's
/// debris and apply placement row 2 texture offsets. Spawn must succeed: the
/// original writes workType before testing the returned pointer.
/// Root/player offsets share the parent frame, narrow to signed halfwords and
/// drive wrapped yaw. Animation ticks every call; dumping-hole ticks >=21 also
/// select view 3. Models borrow work/rig data for their live task lifetime.
static void _gluttonHealState(Task* task)
{
    enum {
        GLUTTON_HEAL_ANIM_IDLE           = 14,
        GLUTTON_HEAL_ANIM_CUE            = 16,
        GLUTTON_HEAL_STATE_SELECT_ATTACK = 10,
        GLUTTON_HEAL_FIRST_CUE_TICK      = 100,
        GLUTTON_HEAL_SECOND_CUE_TICK     = 260,
        GLUTTON_HEAL_SOUND_TICK          = 116,
        GLUTTON_HEAL_END_TICK            = 420,
        GLUTTON_HEAL_FIRST_PITCH_TICK    = 155,
        GLUTTON_HEAL_SECOND_PITCH_TICK   = 275,
        GLUTTON_HEAL_FIRST_CHUNK_TICK    = 175,
        GLUTTON_HEAL_SECOND_CHUNK_TICK   = 325,
        GLUTTON_HEAL_NECK_PITCH          = 128,
        GLUTTON_HEAL_CHUNK_DESCRIPTOR    = 3,
        GLUTTON_HEAL_WALL_DISTANCE       = 3200,
        GLUTTON_HEAL_VIEW_TICK           = 21,
        GLUTTON_HEAL_VIEW                = 3,
        GLUTTON_HEAL_SOUND               = SOUND_CHARACTER(SOUND_BANK_GLUTTON, 23)
    };
    GluttonWork* work;
    Enemy*       enemy;
    Enemy*       spawned;
#if GLUTTON_ROOM == GLUTTON_INCINERATOR
    GfxCoord* rootCoord;
    GfxCoord* headingCoord;
#endif
    SVECTOR  playerOffset;
    SVECTOR* offset;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    GfxCoord* rootCoord;
    GfxCoord* headingCoord;
#endif
    s32 soundId;
    s32 soundPan;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    if (work->stateChanged != 0) {
        work->animId   = GLUTTON_HEAL_ANIM_IDLE;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
        _gluttonPrepareHealModels(task);
        work->neckYawEnabled   = 1;
        work->neckPitchEnabled = 0;
        work->hostExposed      = 0;
        work->neckPitchTarget  = 0;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
        work->wallDistanceTarget = GLUTTON_HEAL_WALL_DISTANCE;
#endif
    }
    // Fixed tick cues consume heal notifications and release chunks in two cycles.
    switch (work->stateTicks) {
        case GLUTTON_HEAL_FIRST_CUE_TICK:
        case GLUTTON_HEAL_SECOND_CUE_TICK:
            if ((s8)work->pendingHeals > 0) {
                work->animId           = GLUTTON_HEAL_ANIM_CUE;
                work->animStep         = GLUTTON_ANIM_STEP_BLEND;
                work->neckPitchEnabled = 1;
                work->pendingHeals--;
            } else {
                work->state           = GLUTTON_HEAL_STATE_SELECT_ATTACK;
                work->neckPitchTarget = 0;
            }
            break;
        case GLUTTON_HEAL_SOUND_TICK:
            soundId  = (((u16)enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | GLUTTON_HEAL_SOUND;
            soundPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(
                soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
            break;
        case GLUTTON_HEAL_END_TICK:
            work->state           = GLUTTON_HEAL_STATE_SELECT_ATTACK;
            work->neckPitchTarget = 0;
            work->pendingHeals    = 0;
            break;
        case GLUTTON_HEAL_FIRST_PITCH_TICK:
        case GLUTTON_HEAL_SECOND_PITCH_TICK:
            work->neckPitchTarget = GLUTTON_HEAL_NECK_PITCH;
            break;
        case GLUTTON_HEAL_FIRST_CHUNK_TICK:
        case GLUTTON_HEAL_SECOND_CHUNK_TICK:
            spawned           = enemySpawnFromTable(gGluttonEscortTasks, GLUTTON_HEAL_CHUNK_DESCRIPTOR, 0, task->spawnArg2.pointer);
            spawned->workType = ENEMY_WORK_PLAIN;
            work->lastSpawned = spawned;
            if (spawned != NULL) {
                _gluttonApplyEscortTextureOffsets(spawned->task->extra.tmd);
                work->neckPitchTarget = 0;
            }
            break;
    }
    // Keep separate position and heading aliases across animation-call lifetimes.
    rootCoord           = task->extra.tmd->coords;
    offset              = &playerOffset;
    offset->vx          = gPlayerStatus.coordMtx->t[0] - rootCoord->coord.t[0];
    offset->vy          = gPlayerStatus.coordMtx->t[1] - rootCoord->coord.t[1];
    offset->vz          = gPlayerStatus.coordMtx->t[2] - rootCoord->coord.t[2];
    headingCoord        = task->extra.tmd->coords;
    work->neckYawTarget = _actorAngleTurnToOffset(headingCoord, offset->vx, offset->vz);
    _gluttonTickAnim(task);
    if (work->animId == GLUTTON_HEAL_ANIM_CUE && (work->hostRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
        work->animId   = GLUTTON_HEAL_ANIM_IDLE;
        work->animStep = GLUTTON_ANIM_STEP_BLEND;
    }
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE
    if (work->stateTicks >= GLUTTON_HEAL_VIEW_TICK) {
        work->viewSelector = GLUTTON_HEAL_VIEW;
    }
#endif
}
