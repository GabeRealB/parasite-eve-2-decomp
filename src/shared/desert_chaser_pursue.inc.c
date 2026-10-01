/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserPursue(Task* arg0)
{
    PlayerStatus* config = &gPlayerStatus;
    SVECTOR       initialDelta;

#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    DesertChaserWork* work;
    Enemy*            ctx;
    GfxCoord*         temp_a1_2;

    GfxCoord* temp_a2_2;
    GfxCoord* temp_a2_3;

    GfxCoord* temp_s0_14;
    GfxCoord* temp_s0_17;
    GfxCoord* temp_s0_20;
    GfxCoord* temp_s0_4;

    GfxCoord* temp_s0_8;

    GfxCoord* temp_v0_5;
    GfxCoord* temp_v0_7;

    ActorFacingScratch* scratch;
    TmdObject*          obj;

#else
    SVECTOR effect;
#endif
    s16 temp_a1_3;
    s16 temp_a1_4;
    s16 temp_a1_5;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    s16 temp_s0_10;
    s16 temp_s0_13;
    s16 temp_s0_16;
    s16 temp_s0_19;
#else
    s16 temp_s0_12;
    s16 temp_s0_15;
    s16 temp_s0_18;
#endif
    s16 temp_s0_22;

    s16 temp_s0_6;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    s16 temp_s0_9;
#endif
    s16 temp_v0_4;

    s32 temp_v1_3;

    s32 var_v0_19;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    s16 var_v0_21;
#else
#endif
    s32 var_v0_22;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    s16 var_v0_29;
#else
    s16 var_v0_24;
#endif
    s32 var_v0_30;
    s32 var_v0_31;

    s16 var_v1_2;
    s16 var_v1_4;
    s16 var_v1_5;
    s16 var_v1_6;
    s16 var_v1_7;
    s16 var_v1_8;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    GfxCoord* temp_a1_2;
    GfxCoord* temp_a2_2;
    GfxCoord* temp_a2_3;
    GfxCoord* temp_s0_13;
    GfxCoord* temp_s0_16;
    GfxCoord* temp_s0_20;
    GfxCoord* temp_s0_4;
    GfxCoord* temp_s0_7;
    GfxCoord* temp_v0_5;
    GfxCoord* temp_v0_7;
#endif
    void** scratchHead;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    s32 temp_s0_12;
    s32 temp_s0_15;
    s32 temp_s0_18;
    s32 temp_s0_21;

    s32 temp_s0_5;
    s32 temp_s0_9;
#else
#endif
    s32 temp_v0;

    s32 spawnEffect;

    s32 var_a1_4;
    s32 effectFlags;
    s32 effectJoint;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR

#else
    s32 var_v0_17;
    s32 var_v0_14;
    s32 var_v0_21;
    s32 var_v0_27;
    s32 var_v0_5;
    s32 var_v0_6;
#endif
    s32 var_v0_12;
    s32 var_v0_13;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR

    s32 var_v0_17;
    s32 var_v0_18;

    s32 var_v0_26;
    s32 var_v0_27;

    s32 var_v0_5;
    s32 var_v0_6;

#else
#endif
    s32 pan;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    s32 temp_s0_11;
    s32 temp_s0_14;
    s32 temp_s0_17;
    s32 temp_s0_21;
    s32 temp_s0_5;
    s32 temp_s0_8;
#endif
    u16 temp_v0_6;
    u16 temp_v1_2;
    u16 var_a0;
    u16 var_v1_3;
    u32 distanceSquared;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    GfxCoord* temp_s0_11;
#else
    GfxCoord* temp_s0_10;
#endif
    Task* player;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    TmdObject*        obj;
    Enemy*            ctx;
    DesertChaserWork* work;
#endif
    GameActor* playerWork;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    ActorFacingScratch* scratch;
#endif

    work       = arg0->work;
    player     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    playerWork = (GameActor*)player->work;
    ctx        = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj              = arg0->extra.tmd;
        initialDelta.pad = actorPositionYaw(arg0, &initialDelta, config);
        temp_v0          = (s16)initialDelta.pad;
        if (temp_v0 > 0x300) {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
            work->field_0 = 8;
#else
            work->field_0 = 9;
#endif
        } else if (temp_v0 < -0x300) {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
            work->field_0 = 9;
#else
            work->field_0 = 10;
#endif
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
        desertChaserArmedAnimTick(arg0);
        work->capsuleBody.shape.ends[1].vz = 0x320;
        work->field_6                      = 0;
        work->field_8                      = 0;
        work->distance                     = 0;
        work->field_840                    = 0;
        work->broadcast.context.loc.stage  = 9;
        work->broadcast.context.loc.area   = 1;
        work->broadcast.command            = 1;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, (&work->broadcast), ACTOR_COMMAND_MESSAGE_APPLY);

        return;
    }
    scratch = SCRATCH_STACK_RESERVE_BLOCK(ActorFacingScratch);
    if (work->field_82E == 3) {
        work->field_6 += 1;
    }
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR

    if ((ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5) != 0) && (work->field_6 >= 0xB)) {
#else
    if ((ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 0xC) != 0) && (work->field_6 >= 0xB)) {
#endif
        distanceSquared          = ActorContact_ScratchPosition.vx * ActorContact_ScratchPosition.vx;
        scratch->distanceSquared = distanceSquared;
        scratch->distanceSquared = (u32)(distanceSquared + (ActorContact_ScratchPosition.vz * ActorContact_ScratchPosition.vz));
        temp_s0_4                = arg0->extra.tmd->coords;
        temp_s0_5                = ratan2((s32)ActorContact_ScratchPosition.vx, (s32)ActorContact_ScratchPosition.vz);
        temp_s0_6                = temp_s0_5 - ratan2((s32)-temp_s0_4->coord.m[2][0], (s32)temp_s0_4->coord.m[2][2]);
        var_v1_2                 = actorNormalizeYaw(temp_s0_6);
        var_v0_5                 = var_v1_2 << 0x10;

        var_v0_6 = var_v0_5 >> 0x10;
        if (var_v0_6 < 0) {
            var_v0_6 = -var_v0_6;
        }

        if ((var_v0_6 >= 0x601) && ((u32)scratch->distanceSquared >= 0xE11U)) {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
            if (((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(4, 1, 0, 0)) && Actor00100_InRegion(arg0)) {
                if (Actor00100_FacingAway(arg0->extra.tmd->coords)) {
                    work->field_0 = 6;
                } else {
                    work->field_0 = 0x1D;
                }
            } else {
#else
#endif
                work->field_0 = 0x23;
            }
        }
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    }
    if ((desertChaserAvoidWalk(arg0->extra.tmd->coords, work->objs[0].contacts, 5, (SVECTOR*)scratch) << 0x10) != 0) {
#else
    if ((desertChaserAvoidWalk(arg0->extra.tmd->coords, work->objs[0].contacts, 0xC, (SVECTOR*)scratch) << 0x10) != 0) {
#endif
        if ((work->field_82E == 3) && (playerWork->mode != GAME_ACTOR_MODE_SCRIPTED)) {
            work->queryMode     = 0x80;
            temp_a1_2           = arg0->extra.tmd->coords;
            scratch->vx         = (s16)(gPlayerStatus.coordMtx->t[0] - temp_a1_2->coord.t[0]);
            scratch->vy         = gPlayerStatus.coordMtx->t[1] - temp_a1_2->coord.t[1];
            temp_v0_4           = gPlayerStatus.coordMtx->t[2] - temp_a1_2->coord.t[2];
            scratch->vz         = temp_v0_4;
            scratch->contactYaw = ratan2((s32)scratch->vx, (s32)temp_v0_4);
            temp_v0_5           = arg0->extra.tmd->coords;
            temp_v1_2           = scratch->contactYaw - ratan2((s32)-temp_v0_5->coord.m[2][0], (s32)temp_v0_5->coord.m[2][2]);
            var_a0              = temp_v1_2;
            scratch->contactYaw = temp_v1_2;

            var_a0    = actorNormalizeYaw(temp_v1_2);
            var_v0_12 = var_a0 << 0x10;

            var_v0_13           = var_v0_12 >> 0x10;
            scratch->contactYaw = (u16)var_v0_13;
            var_v0_13           = abs(var_v0_13);
            if (var_v0_13 < 0x180) {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                printf("EM01 PLAYER WORK %d, %d\n", playerWork->mode, playerWork->state);
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, (work->replyBuf), 0) == 0) {
#else
                if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F8, work->replyBuf, 0) == 0) {
#endif
                    Gfx_MatrixCol2(&arg0->extra.tmd->coords->coord, (SVECTOR*)scratch);
                    temp_v0_6           = ratan2((s32)scratch->vx, (s32)scratch->vz) + 0x800;
                    var_v1_3            = temp_v0_6;
                    scratch->contactYaw = temp_v0_6;

                    var_v1_3 = actorNormalizeYaw(temp_v0_6);

                    scratch->contactYaw = var_v1_3;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                    temp_s0_8  = arg0->extra.tmd->coords;
                    temp_s0_9  = ratan2((s32)scratch->vx, (s32)scratch->vz);
                    temp_s0_10 = temp_s0_9 - ratan2((s32)-temp_s0_8->coord.m[2][0], (s32)temp_s0_8->coord.m[2][2]);
                    var_v1_4   = actorNormalizeYaw(temp_s0_10);
#else
                    temp_s0_7 = arg0->extra.tmd->coords;
                    temp_s0_8 = ratan2(scratch->vx, scratch->vz);
                    temp_s0_9 = temp_s0_8 - ratan2(-temp_s0_7->coord.m[2][0], temp_s0_7->coord.m[2][2]);

                    var_v1_4 = actorNormalizeYaw(temp_s0_9);
#endif

                    scratch->turnYaw = var_v1_4;
                    scratch->vy      = 0;
                    scratch->vx      = (s16) - (s16)(u16)scratch->vx;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                    scratch->vz = -(s16)(u16)scratch->vz;
                    temp_s0_11  = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
                    temp_s0_12  = ratan2((s32)scratch->vx, (s32)scratch->vz);
                    temp_s0_13  = temp_s0_12 - ratan2((s32)-temp_s0_11->coord.m[2][0], (s32)temp_s0_11->coord.m[2][2]);
                    var_v1_5    = actorNormalizeYaw(temp_s0_13);
#else
                    scratch->vz = (s16) - (s16)(u16)scratch->vz;
                    temp_s0_10  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                    temp_s0_11  = ratan2(scratch->vx, scratch->vz);
                    temp_s0_12  = temp_s0_11 - ratan2(-temp_s0_10->coord.m[2][0], temp_s0_10->coord.m[2][2]);

                    var_v1_5 = actorNormalizeYaw(temp_s0_12);
#endif
                    var_v0_17 = var_v1_5 << 0x10;

#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                    var_v0_18          = var_v0_17 >> 0x10;
                    scratch->playerYaw = (s16)var_v0_18;
                    var_v0_18          = abs(var_v0_18);
                    if (var_v0_18 < 0x400) {
#else
                    var_v0_14          = var_v0_17 >> 0x10;
                    scratch->playerYaw = (s16)var_v0_14;
                    var_v0_14          = abs(var_v0_14);
                    if (var_v0_14 < 0x400) {
#endif
                        work->animCommand = &gDesertChaserFrontAnim;
                    } else {
                        work->animCommand   = &gDesertChaserRearAnim;
                        scratch->contactYaw = (u16)(scratch->contactYaw + 0x800);
                    }
                    work->playerPlacement.rot.vx = 0;
                    work->playerPlacement.rot.vy = (u16)scratch->contactYaw;
                    work->playerPlacement.rot.vz = 0;
                    work->playerPlacement.pos.vx = (s32)player->extra.tmd->coords->coord.t[0];
                    work->playerPlacement.pos.vy = (s32)player->extra.tmd->coords->coord.t[1];
                    work->playerPlacement.pos.vz = (s32)player->extra.tmd->coords->coord.t[2];
                    TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, (&work->playerPlacement), 0);
                    if (work->distance < 0x3E8) {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
                        if (ctx->hp > 0) {
#endif
                        var_v0_19 = scratch->playerYaw;
                        if (var_v0_19 < 0) {
                            var_v0_19 = -var_v0_19;
                        }
                        if (var_v0_19 < 0x400) {
                            scratch->messageResult = actorPlayerContactMessage(ctx, 2);
                        } else {
                            scratch->messageResult = actorPlayerContactMessage(ctx, 3);
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
                            }
#endif
                        }
                        if (scratch->messageResult != 1) {
                            work->params[0]   = 3;
                            work->params[1]   = 0;
                            work->params[2]   = 0;
                            work->routePos.vx = 0;
                            work->routePos.vy = 0;
                            work->routePos.vz = 0;
                            work->poseId      = 7;
                            work->poseBlend   = 1;
                            work->reported    = 1;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, (&work->animCommand), 0);
#else
                            work->actorId.bytes[3] = 0;
                            TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->animCommand, 0);
#endif
                        }
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                        var_v0_21 = 0x25;
#else
                        work->field_0 = 0x25;
                        Gp_SpawnPadLerp(3, 0xFF, 8);
#endif
                    } else {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
                        if (ctx->hp > 0) {
#endif
                        var_v0_22 = scratch->playerYaw;
                        if (var_v0_22 < 0) {
                            var_v0_22 = -var_v0_22;
                        }
                        if (var_v0_22 < 0x400) {
                            scratch->messageResult = actorPlayerContactMessage(ctx, 0);
                        } else {
                            scratch->messageResult = actorPlayerContactMessage(ctx, 1);
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
                            }
#endif
                        }
                        if (scratch->messageResult == 1) {
                            ((GameActor*)player->work)->state = 0xA;
                        }
                        work->params[0]   = 1;
                        work->params[1]   = 0;
                        work->params[2]   = 0;
                        work->routePos.vx = 0;
                        work->routePos.vy = 0;
                        work->routePos.vz = 0;
                        work->poseId      = 7;
                        work->poseBlend   = 1;
                        work->reported    = 1;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, (&work->animCommand), 0);
                        var_v0_21 = 0x1E;
#else
                        work->actorId.bytes[3] = 0;
                        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->animCommand, 0);
                        work->field_0 = 0x1E;
                        Gp_SpawnPadLerp(8, 0xFF, 8);
#endif
                    }
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                    work->field_0 = var_v0_21;
#else
#endif
                }
            }
            temp_a2_2   = arg0->extra.tmd->coords;
            scratch->vx = (s16)(gPlayerStatus.coordMtx->t[0] - temp_a2_2->coord.t[0]);
            scratch->vy = gPlayerStatus.coordMtx->t[1] - temp_a2_2->coord.t[1];
            temp_a1_3   = gPlayerStatus.coordMtx->t[2] - temp_a2_2->coord.t[2];
            scratch->vz = temp_a1_3;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
            temp_s0_14 = arg0->extra.tmd->coords;
            temp_s0_15 = ratan2((s32)scratch->vx, (s32)temp_a1_3);
            temp_s0_16 = temp_s0_15 - ratan2((s32)-temp_s0_14->coord.m[2][0], (s32)temp_s0_14->coord.m[2][2]);
            var_v1_6   = actorNormalizeYaw(temp_s0_16);
#else
            temp_s0_13 = arg0->extra.tmd->coords;
            temp_s0_14 = ratan2(scratch->vx, temp_a1_3);
            temp_s0_15 = temp_s0_14 - ratan2(-temp_s0_13->coord.m[2][0], temp_s0_13->coord.m[2][2]);

            var_v1_6 = actorNormalizeYaw(temp_s0_15);
#endif

            scratch->targetYaw = var_v1_6;
        } else {
            goto updatePlayerYaw;
        }
    } else {
    updatePlayerYaw:
        temp_a2_3   = arg0->extra.tmd->coords;
        scratch->vx = (s16)(gPlayerStatus.coordMtx->t[0] - temp_a2_3->coord.t[0]);
        scratch->vy = gPlayerStatus.coordMtx->t[1] - temp_a2_3->coord.t[1];
        temp_a1_4   = gPlayerStatus.coordMtx->t[2] - temp_a2_3->coord.t[2];
        scratch->vz = temp_a1_4;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        temp_s0_17 = arg0->extra.tmd->coords;
        temp_s0_18 = ratan2((s32)scratch->vx, (s32)temp_a1_4);
        temp_s0_19 = temp_s0_18 - ratan2((s32)-temp_s0_17->coord.m[2][0], (s32)temp_s0_17->coord.m[2][2]);
        var_v1_7   = actorNormalizeYaw(temp_s0_19);
        var_v0_26  = var_v1_7 << 0x10;
#else
        temp_s0_16 = arg0->extra.tmd->coords;
        temp_s0_17 = ratan2(scratch->vx, temp_a1_4);
        temp_s0_18 = temp_s0_17 - ratan2(-temp_s0_16->coord.m[2][0], temp_s0_16->coord.m[2][2]);
#endif

#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        var_v0_27 = var_v0_26 >> 0x10;
#else
        var_v1_7  = actorNormalizeYaw(temp_s0_18);
        var_v0_21 = var_v1_7 << 0x10;

        var_v0_27 = var_v0_21 >> 0x10;
#endif
        scratch->targetYaw = (s16)var_v0_27;
        var_v0_27          = abs(var_v0_27);
        if ((var_v0_27 >= 0x601) && (work->field_82E == 3)) {
            work->field_0 = 0x1D;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    temp_s0_20                            = arg0->extra.tmd->coords;
    temp_s0_21                            = ratan2((s32)work->playerDelta.vx, (s32)work->playerDelta.vz);
    temp_s0_22                            = temp_s0_21 - ratan2((s32)-temp_s0_20->coord.m[2][0], (s32)temp_s0_20->coord.m[2][2]);
    var_v1_8                              = actorNormalizeYaw(temp_s0_22);

    scratch->turnYaw = var_v1_8;
    desertChaserArmedAnimTick(arg0);
    if (work->field_82E == 2) {
        if (scratch->turnYaw >= 0x41) {
            scratch->turnYaw = 0x40;
        }
        if (scratch->turnYaw < -0x40) {
            scratch->turnYaw = -0x40;
        }
        temp_v0_7        = arg0->extra.tmd->coords;
        temp_a1_5        = (u16)scratch->turnYaw + ratan2((s32)-temp_v0_7->coord.m[2][0], (s32)temp_v0_7->coord.m[2][2]);
        scratch->turnYaw = temp_a1_5;
        gfxRotMatrixY(&arg0->extra.tmd->coords->coord, (s32)temp_a1_5, 1);
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        var_a1_4 = desertChaserCapsuleTouchesGrid(arg0);
        if (var_a1_4 != 0) {
            actorMoveForward(arg0->extra.tmd->coords, 0x55);
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
            var_v0_29 = (u16)work->distance + 0x55;
#else
            var_v0_24 = (u16)work->distance + 0x55;
#endif
        } else {
            actorMoveForward(arg0->extra.tmd->coords, 0xC8);
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
            var_v0_29 = (u16)work->distance + 0xC8;
#else
            var_v0_24 = (u16)work->distance + 0xC8;
#endif
        }
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->distance = var_v0_29;
#else
        work->distance = var_v0_24;
#endif
    }
    if (work->field_82E == 2) {
        work->field_8 = (u16)work->field_8 + 1;
    }
    if (work->field_8 > (s16)work->poseVy) {
        temp_v1_3 = work->field_82E;
        if (temp_v1_3 == 2) {
            var_v0_30 = scratch->targetYaw;
            if (var_v0_30 < 0) {
                var_v0_30 = -var_v0_30;
            }
            if ((var_v0_30 < 0x80) || (work->slots[1].flags & 0x100)) {
                work->field_82E = 3;
                work->field_828 = (u16)temp_v1_3;
                pan             = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                SndEvt_EnqueueType6(0x40010006, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
                work->broadcast.context.loc.stage = 9;
                work->broadcast.context.loc.area  = 1;
                work->broadcast.command           = 4;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &work->broadcast, ACTOR_COMMAND_MESSAGE_APPLY);
#else
                SndEvt_EnqueueType6(0x40010006, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
#endif
            }
        }
    }
    if (work->field_8 >= 0xF) {
        var_v0_31 = scratch->targetYaw;
        if (var_v0_31 < 0) {
            var_v0_31 = -var_v0_31;
        }
        if (var_v0_31 >= 0x81) {
            goto updatePlayerDelta;
        }
    } else {
    updatePlayerDelta:
        if (work->field_82E == 2) {
            work->playerDelta.vx = (s16)(config->coordMtx->t[0] - arg0->extra.tmd->coords->coord.t[0]);
            work->playerDelta.vy = (s16)(config->coordMtx->t[1] - arg0->extra.tmd->coords->coord.t[1]);
            work->playerDelta.vz = (s16)(config->coordMtx->t[2] - arg0->extra.tmd->coords->coord.t[2]);
        }
    }
    if (work->field_82E == 3) {
        switch (work->slots[1].currentPose.indices.recordIndex & 0x3FF) {
            case 5:
                spawnEffect = 1;
                effectJoint = 7;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                effectFlags        = 0x4300;
                work->field_898.vz = 0;
                work->field_898.vx = 0;
                work->field_898.vy = 0x2BC;
#else
                effectFlags = 17152;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
#endif
                break;
            case 8:
                spawnEffect = 1;
                effectJoint = 9;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                effectFlags        = 0x3500;
                work->field_898.vz = 0;
                work->field_898.vx = 0;
                work->field_898.vy = 0x2BC;
#else
                effectFlags = 13568;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 700;
#endif
                break;
            case 10:
                spawnEffect = 1;
                effectJoint = 14;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                effectFlags        = 0x5A00;
                work->field_898.vz = 0;
                work->field_898.vx = 0;
                work->field_898.vy = 0x258;
#else
                effectFlags = 23040;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
#endif
                break;
            case 13:
                spawnEffect = 1;
                effectJoint = 17;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                effectFlags        = 0x4800;
                work->field_898.vz = 0;
                work->field_898.vx = 0;
                work->field_898.vy = 0x258;
#else
                effectFlags = 18432;
                effect.vz   = 0;
                effect.vx   = 0;
                effect.vy   = 600;
#endif
                break;
            default:
                spawnEffect = 0;
                effectJoint = 0;
                effectFlags = 1;
                break;
        }
        if (gRoomEffectState->roomEffectMode == ROOM_EFFECT_VIEW_ENABLED) {
            scratchHead = SCRATCH_HEAD_ADDR;
            if (spawnEffect == 1) {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[effectJoint], effectFlags | 0x80000000, &work->field_898);
#else
                Gp_SpawnEff(0x60054, &arg0->extra.tmd->coords[effectJoint], effectFlags | 0x80000000, &effect);
#endif
                goto releaseScratch;
            }
        } else {
            goto releaseScratch;
        }
    } else {
    releaseScratch:
        scratchHead = SCRATCH_HEAD_ADDR;
    }
    SCRATCH_POP_BYTES_AT(scratchHead, 0x18);
}
