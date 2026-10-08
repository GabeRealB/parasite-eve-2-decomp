/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Emits the initial encounter puff and requests the idle animation once.
///
/// Borrows live task/work/root storage; counters and the next state stay with
/// the command handler so restart-only stores retain their order.
static inline void _sucklercephStartCommandPuff(Task* task, SucklercephWork* work, GfxCoord* rootCoord)
{
    enum { SUCKLERCEPH_COMMAND_PUFF_SIZE = 1024 };

    effectSpawn(EFFECT_ADDITIVE_PUFF, rootCoord, SUCKLERCEPH_COMMAND_PUFF_SIZE, &gSucklercephCollapseFxOffset);
    work->animId = SUCKLERCEPH_ANIM_IDLE;
    _sucklercephTickAnim(task);
}

/// Applies Sucklerceph encounter commands to reveal, hide or start puffing.
///
/// Borrows a read-only command for this call; ignores its context and both ABI
/// words. Requires live enemy/work/model. Exact command words 4 and 5 start
/// puffing only in active task state 1; 5 also resets the swelling counter.
/// Neither selects the puffing-death state. Low byte 3 hides the model,
/// disables sensing/body passes, resets its root and returns to drop state 3.
/// Low byte 1 reveals only outside task states 1/2 and arms the drop; its full
/// high byte indexes the current room's spot table without masking or clamping.
/// Reveal requires dumping-hole area with spot 0..11 or incinerator with 0..15,
/// and entry-move bits 4..7 must be zero. Spot positions are root-parent game
/// coordinates and headings use 4096 units per turn. Always returns 0, including
/// ignored commands, and retains no request pointer.
static s32 _sucklercephMessage(Task* task, s32 unusedMessageId, const ActorCommand* request, s32 unusedSecondArg)
{
    enum {
        SUCKLERCEPH_MESSAGE_ACTIVE_TASK       = 1,
        SUCKLERCEPH_MESSAGE_DROP_TASK         = 3,
        SUCKLERCEPH_COMMAND_HIDE              = 3,
        SUCKLERCEPH_COMMAND_RESTART_PUFFING   = 5,
        SUCKLERCEPH_COMMAND_ACTION_MASK       = 0xFF,
        SUCKLERCEPH_COMMAND_SPOT_SHIFT        = 8,
        SUCKLERCEPH_DROP_FORWARD_SPEED        = 200,
        SUCKLERCEPH_DROP_FALL_SPEED           = 100,
        SUCKLERCEPH_DROP_APPEAR_SOUND         = 0x54270006,
        SUCKLERCEPH_DROP_SOUND_INSTANCE_SHIFT = 8
    };
    SucklercephWork* work;
    Enemy*           enemy;
    Enemy*           soundEnemy;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    SVECTOR          spawnRotation;
    u16              actionByte;
    s16              heading;
    s32              headingMagnitude;
    s32              commandWord;
    s32              taskState;
    s32              soundId;
    s32              soundPan;

    model     = task->extra.tmd;
    enemy     = task->spawnArg2.pointer;
    taskState = task->state;
    work      = task->work;
    rootCoord = model->coords;
    if (taskState == SUCKLERCEPH_MESSAGE_ACTIVE_TASK) {
        commandWord = request->command;
        if (commandWord == OVERLAY_ENCOUNTER_COMMAND_STOP) {
            _sucklercephStartCommandPuff(task, work, rootCoord);
            work->animFrames = 0;
            work->state      = SUCKLERCEPH_STATE_PUFFING;
            return 0;
        }
        if (commandWord == SUCKLERCEPH_COMMAND_RESTART_PUFFING) {
            _sucklercephStartCommandPuff(task, work, rootCoord);
            work->animFrames  = 0;
            work->swellFrames = 0;
            work->state       = SUCKLERCEPH_STATE_PUFFING;
            return 0;
        }
    }
    actionByte = request->command & SUCKLERCEPH_COMMAND_ACTION_MASK;
    if ((actionByte & SUCKLERCEPH_COMMAND_ACTION_MASK) == OVERLAY_ENCOUNTER_COMMAND_APPEAR) {
        if ((u32)(task->state - SUCKLERCEPH_MESSAGE_ACTIVE_TASK) >= 2U) {
            // Callers supply an in-bounds full high byte for the live room table.
            if (gGameSession->location.loc.area == GAME_AREA_SHELTER_B3_DUMPING_HOLE) {
                spawnRotation.vx      = 0;
                spawnRotation.vy      = D_shelter_b3_dumping_hole_8018B74C[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].heading;
                spawnRotation.vz      = 0;
                rootCoord->coord.t[0] = D_shelter_b3_dumping_hole_8018B74C[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].x;
                rootCoord->coord.t[1] = D_shelter_b3_dumping_hole_8018B74C[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].y;
                rootCoord->coord.t[2] = D_shelter_b3_dumping_hole_8018B74C[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].z;
                soundEnemy            = task->spawnArg2.pointer;
                soundId               = (((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_DROP_SOUND_INSTANCE_SHIFT) | SUCKLERCEPH_DROP_APPEAR_SOUND);
                soundPan              = (s8)worldCoordGetOriginAudioPan(rootCoord);
                sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            } else if (gGameSession->location.loc.area == GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR) {
                spawnRotation.vx      = 0;
                spawnRotation.vy      = D_shelter_b3_garbage_incinerator_801874C4[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].heading;
                spawnRotation.vz      = 0;
                rootCoord->coord.t[0] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].x;
                rootCoord->coord.t[1] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].y;
                rootCoord->coord.t[2] = D_shelter_b3_garbage_incinerator_801874C4[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].z;
            }
            heading          = spawnRotation.vy;
            work->heading    = heading;
            headingMagnitude = heading >= 0 ? heading : -heading;
            if (headingMagnitude >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
                if (heading >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
                    work->heading = heading - ACTOR_TRANSFORM_ANGLE_TURN;
                } else if (heading < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                    work->heading = heading + ACTOR_TRANSFORM_ANGLE_TURN;
                }
            }
            // Reveal and re-enable only the sensing and ordinary body spheres.
            tmdAllocPrimitiveBuffer(task->extra.tmd);
            task->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            task->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            enemy->node.state.parts.flags = 0;
            work->senseBody.flags        |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->body.flags             |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            RotMatrix(&spawnRotation, &rootCoord->coord);
            work->forwardSpeed                    = SUCKLERCEPH_DROP_FORWARD_SPEED;
            work->dropArmed                       = 1;
            work->fallSpeed                       = SUCKLERCEPH_DROP_FALL_SPEED;
            work->dropCollided                    = 0;
            work->state                           = SUCKLERCEPH_STATE_AWAKE;
            work->awakeStage                      = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(task->extra.tmd->coords);
        }
        return 0;
    }
    if ((actionByte & SUCKLERCEPH_COMMAND_ACTION_MASK) == SUCKLERCEPH_COMMAND_HIDE) {
        task->extra.tmd->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->extra.tmd->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->senseBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        spawnRotation.vz              = 0;
        spawnRotation.vy              = 0;
        spawnRotation.vx              = 0;
        RotMatrix(&spawnRotation, &rootCoord->coord);
        rootCoord->coord.t[2]                 = 0;
        rootCoord->coord.t[1]                 = 0;
        rootCoord->coord.t[0]                 = 0;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
        task->state      = SUCKLERCEPH_MESSAGE_DROP_TASK;
        work->dropArmed  = 0;
        work->state      = SUCKLERCEPH_STATE_DORMANT;
        work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_NONE;
    }
    return 0;
}
