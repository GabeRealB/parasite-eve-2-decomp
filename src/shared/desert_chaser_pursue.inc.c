/* Part of the Desert Chaser library; see desert_chaser.h. */

/* Where the hit effect's offset is built: the work block in the regular
 * build, a stack vector in the run sequence. */
#if DESERT_CHASER_RUN_SEQUENCE
#define DESERT_CHASER_FX_OFFSET effect
#else
#define DESERT_CHASER_FX_OFFSET work->effectOffset
#endif

/// The chase. On entry it picks a turn state if the player is well to one
/// side, rearms its bodies and tells the scene it is chasing (broadcast 9/1,
/// command 1). Each frame it walks forward (0x55 when the capsule meets the
/// grid, else 0xC8), backs off when pushed from behind, and in its lunge
/// (`animId` 3) catches a player it faces: the player is placed in front or
/// behind, plays the caught animation, and the chaser goes to its hold state
/// (0x25 close in, 0x1E farther out). The lunge spawns the 0x60054 swipe
/// effect on its claw joints at the clip's frames 5, 8, 10 and 13.
void desertChaserPursue(Task* arg0)
{
    PlayerStatus* config = &gPlayerStatus;
    SVECTOR       initialDelta;
#if DESERT_CHASER_RUN_SEQUENCE
    SVECTOR effect;
#endif
    DesertChaserWork*          work;
    Enemy*                     ctx;
    GfxCoord*                  coord;
    GfxCoord*                  facing;
    TmdObject*                 obj;
    DesertChaserPursueScratch* scratch;
    Task*                      player;
    GameActor*                 playerWork;
    s16                        dz;
    s16                        turn;
    s32                        initialYaw;
    s32                        yaw;
    u16                        contactYaw;
    u32                        distanceSquared;
#if !DESERT_CHASER_RUN_SEQUENCE
    s16 nextState;
#endif
    u16 distance;
    s32 state;
    s32 spawnEffect;
    s32 effectFlags;
    s32 effectJoint;
    s32 pan;

    work       = arg0->work;
    player     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerWork = (GameActor*)player->work;
    ctx        = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj              = arg0->extra.tmd;
        initialDelta.pad = _actorAngleTurnToPlayer(arg0, &initialDelta, config);
        initialYaw       = (s16)initialDelta.pad;
        if (initialYaw > 0x300) {
            work->state = DESERT_CHASER_STATE_TURN_RIGHT;
        } else if (initialYaw < -0x300) {
            work->state = DESERT_CHASER_STATE_TURN_LEFT;
        }
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animId                                          = 2;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->waistYawTarget                                  = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                        = work->baseRate;
        _desertChaserAnimTick(arg0);
        work->wallProbe.shape.ends[1].vz  = 0x320;
        work->stateTimer                  = 0;
        work->stateCounter                = 0;
        work->lungeDistance               = 0;
        work->lookYawTarget               = 0;
        work->broadcast.context.loc.stage = 9;
        work->broadcast.context.loc.area  = 1;
        work->broadcast.command           = 1;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
        return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserPursueScratch);
    if (work->animId == 3) {
        work->stateTimer += 1;
    }
    if ((_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) != 0) && (work->stateTimer >= 0xB)) {
        distanceSquared            = ActorContact_ScratchPosition.vx * ActorContact_ScratchPosition.vx;
        scratch->pushLengthSquared = distanceSquared;
        scratch->pushLengthSquared = distanceSquared + ActorContact_ScratchPosition.vz * ActorContact_ScratchPosition.vz;
        yaw                        = _actorAngleTurnToOffset(arg0->extra.tmd->coords, ActorContact_ScratchPosition.vx, ActorContact_ScratchPosition.vz);
        if ((abs(yaw) >= 0x601) && (scratch->pushLengthSquared >= 0xE11U)) {
#if !DESERT_CHASER_RUN_SEQUENCE
            if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) && _actor00100IsInMesaDropRegion(arg0)) {
                if (_actor00100FacesMesaDrop(arg0->extra.tmd->coords)) {
                    work->state = 6;
                } else {
                    work->state = 0x1D;
                }
            } else
#endif
            {
                work->state = 0x23;
            }
        }
    }
    if (((desertChaserAvoidWalk(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->offset) << 0x10) != 0) &&
        (work->animId == 3) && (playerWork->mode != GAME_ACTOR_MODE_SCRIPTED)) {
        work->playerButtonHold.pressCount = 0x80;
        coord                             = arg0->extra.tmd->coords;
        scratch->offset.vx                = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
        scratch->offset.vy                = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
        dz                                = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        scratch->offset.vz                = dz;
        scratch->catchYaw                 = ratan2(scratch->offset.vx, dz);
        facing                            = arg0->extra.tmd->coords;
        contactYaw                        = scratch->catchYaw - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        scratch->catchYaw                 = contactYaw;
        yaw                               = _actorAngleNormalizeYaw(contactYaw);
        scratch->catchYaw                 = yaw;
        if (abs(yaw) < 0x180) {
#if !DESERT_CHASER_RUN_SEQUENCE
            printf("EM01 PLAYER WORK %d, %d\n", playerWork->mode, playerWork->state);
#endif
            if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &work->playerButtonHold, 0) == 0) {
                gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &scratch->offset);
                contactYaw          = ratan2(scratch->offset.vx, scratch->offset.vz) + 0x800;
                scratch->catchYaw   = contactYaw;
                scratch->catchYaw   = _actorAngleNormalizeYaw(contactYaw);
                scratch->turn       = _actorAngleTurnToOffset(arg0->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
                scratch->offset.vy  = 0;
                scratch->offset.vx  = -scratch->offset.vx;
                scratch->offset.vz  = -scratch->offset.vz;
                scratch->playerTurn = _actorAngleTurnToOffset(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords, scratch->offset.vx, scratch->offset.vz);
                if (abs(scratch->playerTurn) < 0x400) {
                    work->playerAnim.source.sets = gDesertChaserFrontAnim;
                } else {
                    work->playerAnim.source.sets = gDesertChaserRearAnim;
                    scratch->catchYaw            = scratch->catchYaw + 0x800;
                }
                work->playerPlacement.rot.vx = 0;
                work->playerPlacement.rot.vy = scratch->catchYaw;
                work->playerPlacement.rot.vz = 0;
                work->playerPlacement.pos.vx = player->extra.tmd->coords->coord.t[0];
                work->playerPlacement.pos.vy = player->extra.tmd->coords->coord.t[1];
                work->playerPlacement.pos.vz = player->extra.tmd->coords->coord.t[2];
                TASK_MESSAGE_DISPATCH_POINTER(player, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
                if (work->lungeDistance < 0x3E8) {
#if DESERT_CHASER_RUN_SEQUENCE
                    if (ctx->hp > 0)
#endif
                    {
                        if (abs(scratch->playerTurn) < 0x400) {
                            scratch->playerKilled = actorPlayerContactMessage(ctx, 2);
                        } else {
                            scratch->playerKilled = actorPlayerContactMessage(ctx, 3);
                        }
                    }
                    if (scratch->playerKilled != 1) {
                        work->playerAnim.animationId       = 3;
                        work->playerAnim.blend             = ANIMATION_BLEND_RESET;
                        work->playerAnim.blendFrames       = 0;
                        work->playerMove.displacement.vx   = 0;
                        work->playerMove.displacement.vy   = 0;
                        work->playerMove.displacement.vz   = 0;
                        work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                        work->playerMove.keepControl       = 1;
                        work->playerHeld                   = 1;
#if DESERT_CHASER_RUN_SEQUENCE
                        work->lastCommand.fields.catchFrames = 0;
#endif
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                    }
                    work->state = 0x25;
#if DESERT_CHASER_RUN_SEQUENCE
                    padScriptSpawnVariableMotorRamp(3, 0xFF, 8);
#endif
                } else {
#if DESERT_CHASER_RUN_SEQUENCE
                    if (ctx->hp > 0)
#endif
                    {
                        if (abs(scratch->playerTurn) < 0x400) {
                            scratch->playerKilled = actorPlayerContactMessage(ctx, 0);
                        } else {
                            scratch->playerKilled = actorPlayerContactMessage(ctx, 1);
                        }
                    }
                    if (scratch->playerKilled == 1) {
                        ((GameActor*)player->work)->state = 0xA;
                    }
                    work->playerAnim.animationId       = 1;
                    work->playerAnim.blend             = ANIMATION_BLEND_RESET;
                    work->playerAnim.blendFrames       = 0;
                    work->playerMove.displacement.vx   = 0;
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vz   = 0;
                    work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl       = 1;
                    work->playerHeld                   = 1;
#if DESERT_CHASER_RUN_SEQUENCE
                    work->lastCommand.fields.catchFrames = 0;
#endif
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
                    work->state = 0x1E;
#if DESERT_CHASER_RUN_SEQUENCE
                    padScriptSpawnVariableMotorRamp(8, 0xFF, 8);
#endif
                }
            }
        }
        scratch->playerBearing = _actorAngleTurnToPlayer(arg0, &scratch->offset, &gPlayerStatus);
    } else {
        yaw                    = _actorAngleTurnToPlayer(arg0, &scratch->offset, &gPlayerStatus);
        scratch->playerBearing = yaw;
        if ((abs(yaw) >= 0x601) && (work->animId == 3)) {
            work->state = 0x1D;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    scratch->turn                         = _actorAngleTurnToOffset(arg0->extra.tmd->coords, work->playerDelta.vx, work->playerDelta.vz);
    _desertChaserAnimTick(arg0);
    if (work->animId == 2) {
        if (scratch->turn >= 0x41) {
            scratch->turn = 0x40;
        }
        if (scratch->turn < -0x40) {
            scratch->turn = -0x40;
        }
        facing        = arg0->extra.tmd->coords;
        turn          = (u16)scratch->turn + ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        scratch->turn = turn;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn, 1);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        if (_desertChaserCapsuleTouchesGrid(arg0) != 0) {
            _actorMovementStepForward(arg0->extra.tmd->coords, 0x55);
            distance = (u16)work->lungeDistance + 0x55;
        } else {
            _actorMovementStepForward(arg0->extra.tmd->coords, 0xC8);
            distance = (u16)work->lungeDistance + 0xC8;
        }
        work->lungeDistance = distance;
    }
    if (work->animId == 2) {
        work->stateCounter = (u16)work->stateCounter + 1;
    }
    if (work->stateCounter > work->windupFrames) {
        state = work->animId;
        if (state == 2) {
            if ((abs(scratch->playerBearing) < 0x80) || (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
                work->animId      = 3;
                work->animRequest = state;
                pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                sndEvtRequestScriptStart(SOUND_DESERT_CHASER_LUNGE, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
#if !DESERT_CHASER_RUN_SEQUENCE
                work->broadcast.context.loc.stage = 9;
                work->broadcast.context.loc.area  = 1;
                work->broadcast.command           = 4;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
#endif
            }
        }
    }
    if ((work->stateCounter < 0xF) || (abs(scratch->playerBearing) >= 0x81)) {
        if (work->animId == 2) {
            work->playerDelta.vx = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            work->playerDelta.vy = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            work->playerDelta.vz = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        }
    }
    if (work->animId == 3) {
        switch (work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) {
            case 5:
                spawnEffect                = 1;
                effectJoint                = 7;
                effectFlags                = 0x4300;
                DESERT_CHASER_FX_OFFSET.vz = 0;
                DESERT_CHASER_FX_OFFSET.vx = 0;
                DESERT_CHASER_FX_OFFSET.vy = 0x2BC;
                break;
            case 8:
                spawnEffect                = 1;
                effectJoint                = 9;
                effectFlags                = 0x3500;
                DESERT_CHASER_FX_OFFSET.vz = 0;
                DESERT_CHASER_FX_OFFSET.vx = 0;
                DESERT_CHASER_FX_OFFSET.vy = 0x2BC;
                break;
            case 10:
                spawnEffect                = 1;
                effectJoint                = 14;
                effectFlags                = 0x5A00;
                DESERT_CHASER_FX_OFFSET.vz = 0;
                DESERT_CHASER_FX_OFFSET.vx = 0;
                DESERT_CHASER_FX_OFFSET.vy = 0x258;
                break;
            case 13:
                spawnEffect                = 1;
                effectJoint                = 17;
                effectFlags                = 0x4800;
                DESERT_CHASER_FX_OFFSET.vz = 0;
                DESERT_CHASER_FX_OFFSET.vx = 0;
                DESERT_CHASER_FX_OFFSET.vy = 0x258;
                break;
            default:
                spawnEffect = 0;
                effectJoint = 0;
                effectFlags = 1;
                break;
        }
        if ((gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) && (spawnEffect == 1)) {
            effectSpawn(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[effectJoint], effectFlags | 0x80000000, &DESERT_CHASER_FX_OFFSET);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserPursueScratch);
}

#undef DESERT_CHASER_FX_OFFSET
