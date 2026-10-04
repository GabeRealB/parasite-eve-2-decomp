/* Part of the Desert Chaser library; see desert_chaser.h. */

/* Where the hit effect's offset is built: the work block in the regular
 * build, a stack vector in the run sequence. */
#if DESERT_CHASER_RUN_SEQUENCE
#define DESERT_CHASER_FX_OFFSET effect
#else
#define DESERT_CHASER_FX_OFFSET work->field_898
#endif

/// The chase. On entry it picks a turn state if the player is well to one
/// side, rearms its bodies and tells the scene it is chasing (broadcast 9/1,
/// command 1). Each frame it walks forward (0x55 when the capsule meets the
/// grid, else 0xC8), backs off when pushed from behind, and in its lunge
/// (`field_82E` 3) catches a player it faces: the player is placed in front or
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
    DesertChaserWork*   work;
    Enemy*              ctx;
    GfxCoord*           coord;
    GfxCoord*           facing;
    TmdObject*          obj;
    ActorFacingScratch* scratch;
    Task*               player;
    GameActor*          playerWork;
    s16                 dz;
    s16                 turn;
    s32                 initialYaw;
    s32                 yaw;
    u16                 contactYaw;
    u32                 distanceSquared;
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
    if (work->field_4 != 0) {
        obj              = arg0->extra.tmd;
        initialDelta.pad = actorPositionYaw(arg0, &initialDelta, config);
        initialYaw       = (s16)initialDelta.pad;
        if (initialYaw > 0x300) {
            work->field_0 = DESERT_CHASER_STATE_TURN_RIGHT;
        } else if (initialYaw < -0x300) {
            work->field_0 = DESERT_CHASER_STATE_TURN_LEFT;
        }
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_82E          = 2;
        work->field_828          = 1;
        work->field_82A          = 0;
        work->field_83E          = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
        desertChaserAnimTick(arg0);
        work->capsuleBody.shape.ends[1].vz = 0x320;
        work->field_6                      = 0;
        work->field_8                      = 0;
        work->distance                     = 0;
        work->field_840                    = 0;
        work->broadcast.context.loc.stage  = 9;
        work->broadcast.context.loc.area   = 1;
        work->broadcast.command            = 1;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
        return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorFacingScratch);
    if (work->field_82E == 3) {
        work->field_6 += 1;
    }
    if ((ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, DESERT_CHASER_CONTACTS) != 0) && (work->field_6 >= 0xB)) {
        distanceSquared          = ActorContact_ScratchPosition.vx * ActorContact_ScratchPosition.vx;
        scratch->distanceSquared = distanceSquared;
        scratch->distanceSquared = distanceSquared + ActorContact_ScratchPosition.vz * ActorContact_ScratchPosition.vz;
        yaw                      = actorYawTo(arg0->extra.tmd->coords, ActorContact_ScratchPosition.vx, ActorContact_ScratchPosition.vz);
        if ((abs(yaw) >= 0x601) && (scratch->distanceSquared >= 0xE11U)) {
#if !DESERT_CHASER_RUN_SEQUENCE
            if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) && Actor00100_InRegion(arg0)) {
                if (Actor00100_FacingAway(arg0->extra.tmd->coords)) {
                    work->field_0 = 6;
                } else {
                    work->field_0 = 0x1D;
                }
            } else
#endif
            {
                work->field_0 = 0x23;
            }
        }
    }
    if (((desertChaserAvoidWalk(arg0->extra.tmd->coords, work->objs[0].contacts, DESERT_CHASER_CONTACTS, (SVECTOR*)scratch) << 0x10) != 0) &&
        (work->field_82E == 3) && (playerWork->mode != GAME_ACTOR_MODE_SCRIPTED)) {
        work->queryMode     = 0x80;
        coord               = arg0->extra.tmd->coords;
        scratch->vx         = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
        scratch->vy         = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
        dz                  = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        scratch->vz         = dz;
        scratch->contactYaw = ratan2(scratch->vx, dz);
        facing              = arg0->extra.tmd->coords;
        contactYaw          = scratch->contactYaw - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        scratch->contactYaw = contactYaw;
        yaw                 = actorNormalizeYaw(contactYaw);
        scratch->contactYaw = yaw;
        if (abs(yaw) < 0x180) {
#if !DESERT_CHASER_RUN_SEQUENCE
            printf("EM01 PLAYER WORK %d, %d\n", playerWork->mode, playerWork->state);
#endif
            if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, work->replyBuf, 0) == 0) {
                gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, (SVECTOR*)scratch);
                contactYaw          = ratan2(scratch->vx, scratch->vz) + 0x800;
                scratch->contactYaw = contactYaw;
                scratch->contactYaw = actorNormalizeYaw(contactYaw);
                scratch->turnYaw    = actorYawTo(arg0->extra.tmd->coords, scratch->vx, scratch->vz);
                scratch->vy         = 0;
                scratch->vx         = -scratch->vx;
                scratch->vz         = -scratch->vz;
                scratch->playerYaw  = actorYawTo(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords, scratch->vx, scratch->vz);
                if (abs(scratch->playerYaw) < 0x400) {
                    work->animCommand = gDesertChaserFrontAnim;
                } else {
                    work->animCommand   = gDesertChaserRearAnim;
                    scratch->contactYaw = scratch->contactYaw + 0x800;
                }
                work->playerPlacement.rot.vx = 0;
                work->playerPlacement.rot.vy = scratch->contactYaw;
                work->playerPlacement.rot.vz = 0;
                work->playerPlacement.pos.vx = player->extra.tmd->coords->coord.t[0];
                work->playerPlacement.pos.vy = player->extra.tmd->coords->coord.t[1];
                work->playerPlacement.pos.vz = player->extra.tmd->coords->coord.t[2];
                TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &work->playerPlacement, 0);
                if (work->distance < 0x3E8) {
#if DESERT_CHASER_RUN_SEQUENCE
                    if (ctx->hp > 0)
#endif
                    {
                        if (abs(scratch->playerYaw) < 0x400) {
                            scratch->messageResult = actorPlayerContactMessage(ctx, 2);
                        } else {
                            scratch->messageResult = actorPlayerContactMessage(ctx, 3);
                        }
                    }
                    if (scratch->messageResult != 1) {
                        work->params[0]                    = 3;
                        work->params[1]                    = 0;
                        work->params[2]                    = 0;
                        work->playerMove.displacement.vx   = 0;
                        work->playerMove.displacement.vy   = 0;
                        work->playerMove.displacement.vz   = 0;
                        work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                        work->playerMove.keepControl       = 1;
                        work->reported                     = 1;
#if DESERT_CHASER_RUN_SEQUENCE
                        work->actorId.bytes[3] = 0;
#endif
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->animCommand, 0);
                    }
                    work->field_0 = 0x25;
#if DESERT_CHASER_RUN_SEQUENCE
                    Gp_SpawnPadLerp(3, 0xFF, 8);
#endif
                } else {
#if DESERT_CHASER_RUN_SEQUENCE
                    if (ctx->hp > 0)
#endif
                    {
                        if (abs(scratch->playerYaw) < 0x400) {
                            scratch->messageResult = actorPlayerContactMessage(ctx, 0);
                        } else {
                            scratch->messageResult = actorPlayerContactMessage(ctx, 1);
                        }
                    }
                    if (scratch->messageResult == 1) {
                        ((GameActor*)player->work)->state = 0xA;
                    }
                    work->params[0]                    = 1;
                    work->params[1]                    = 0;
                    work->params[2]                    = 0;
                    work->playerMove.displacement.vx   = 0;
                    work->playerMove.displacement.vy   = 0;
                    work->playerMove.displacement.vz   = 0;
                    work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
                    work->playerMove.keepControl       = 1;
                    work->reported                     = 1;
#if DESERT_CHASER_RUN_SEQUENCE
                    work->actorId.bytes[3] = 0;
#endif
                    TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->animCommand, 0);
                    work->field_0 = 0x1E;
#if DESERT_CHASER_RUN_SEQUENCE
                    Gp_SpawnPadLerp(8, 0xFF, 8);
#endif
                }
            }
        }
        scratch->targetYaw = actorPositionYaw(arg0, (SVECTOR*)scratch, &gPlayerStatus);
    } else {
        yaw                = actorPositionYaw(arg0, (SVECTOR*)scratch, &gPlayerStatus);
        scratch->targetYaw = yaw;
        if ((abs(yaw) >= 0x601) && (work->field_82E == 3)) {
            work->field_0 = 0x1D;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    scratch->turnYaw                      = actorYawTo(arg0->extra.tmd->coords, work->playerDelta.vx, work->playerDelta.vz);
    desertChaserAnimTick(arg0);
    if (work->field_82E == 2) {
        if (scratch->turnYaw >= 0x41) {
            scratch->turnYaw = 0x40;
        }
        if (scratch->turnYaw < -0x40) {
            scratch->turnYaw = -0x40;
        }
        facing           = arg0->extra.tmd->coords;
        turn             = (u16)scratch->turnYaw + ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        scratch->turnYaw = turn;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, turn, 1);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        if (desertChaserCapsuleTouchesGrid(arg0) != 0) {
            actorMoveForward(arg0->extra.tmd->coords, 0x55);
            distance = (u16)work->distance + 0x55;
        } else {
            actorMoveForward(arg0->extra.tmd->coords, 0xC8);
            distance = (u16)work->distance + 0xC8;
        }
        work->distance = distance;
    }
    if (work->field_82E == 2) {
        work->field_8 = (u16)work->field_8 + 1;
    }
    if (work->field_8 > work->poseVy) {
        state = work->field_82E;
        if (state == 2) {
            if ((abs(scratch->targetYaw) < 0x80) || (work->slots[1].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
                work->field_82E = 3;
                work->field_828 = state;
                pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
                SndEvt_EnqueueType6(SOUND_DESERT_CHASER_LUNGE, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
#if !DESERT_CHASER_RUN_SEQUENCE
                work->broadcast.context.loc.stage = 9;
                work->broadcast.context.loc.area  = 1;
                work->broadcast.command           = 4;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
#endif
            }
        }
    }
    if ((work->field_8 < 0xF) || (abs(scratch->targetYaw) >= 0x81)) {
        if (work->field_82E == 2) {
            work->playerDelta.vx = config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0];
            work->playerDelta.vy = config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1];
            work->playerDelta.vz = config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2];
        }
    }
    if (work->field_82E == 3) {
        switch (work->slots[1].currentPose.indices.recordIndex & 0x3FF) {
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
            Gp_SpawnEff(EFFECT_DUST_PUFF, &arg0->extra.tmd->coords[effectJoint], effectFlags | 0x80000000, &DESERT_CHASER_FX_OFFSET);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorFacingScratch);
}

#undef DESERT_CHASER_FX_OFFSET
