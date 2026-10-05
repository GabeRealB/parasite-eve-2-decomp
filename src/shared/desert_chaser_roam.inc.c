/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Wandering between its waypoints while it watches for the player. Once its
/// hold-off (`chaseHoldoff`) has run out and it has wandered past `roamLookDelay`
/// frames, it starts the chase (0x1C) when the player is within 1500, within
/// 8000 and less than 0x300 off its heading after 0x1C3 frames, or when the
/// player's heading is more than 0x600 off the bearing to the chaser. The
/// regular build only looks with a clear line of sight on its own frame of
/// fifteen, also chases a target that left its node slot, and reacts to
/// Parasite Energy in use; the run sequence counts its delay down here.
void desertChaserRoam(Task* arg0)
{
    s32                      radius = 0x5DC;
    Enemy*                   ctx;
    DesertChaserWork*        work;
    WorldCollisionContact*   record;
    GfxCoord*                coord;
    GfxCoord*                coord2;
    GfxCoord*                coord3;
    GfxCoord*                facing3;
    GfxCoord*                facing4;
    GfxCoord*                facing5;
    GfxCoord*                facing;
    GfxCoord*                facing2;
    GfxCoord*                turnCoord;
    MATRIX*                  matrix;
    DesertChaserRoamScratch* scratch;
    SVECTOR*                 target;
    SVECTOR*                 target2;
    DesertChaserRoamScratch* head;
    SVECTOR*                 direction;
    DesertChaserRoamScratch* head2;
    TmdObject*               obj;
    s16                      targetDelta;
    s16                      delta;
    s16                      yaw;
    s16                      delta3;
    s16                      delta4;
    s16                      delta5;
    s32                      playerX;
    s16                      delta1;
    s16                      delta2;
    s16                      targetYaw;
    s16                      z;
    s32                      magnitude;
    s32                      targetMagnitude;
    s16                      adjustedDelta;
    s32                      originalMagnitude;
    s16                      wrapped;
    s16                      wrapped2;
    s16                      wrapped3;
    s16                      wrapped4;
    s16                      wrapped5;
    s16                      wrappedYaw;
    s32                      angle3;
    s32                      angle4;
    s32                      angle5;
    s32                      angle;
    s32                      angle2;
    s32                      finalYaw;
    s32                      turnDelta;
    s32                      finalDelta;
    s32                      yawDifference;
    u16                      unsignedDelta;
    work = arg0->work;
#if !DESERT_CHASER_RUN_SEQUENCE
    ctx = arg0->spawnArg2.pointer; /* also read by the look-around below */
#endif
    if (work->stateEntered != 0) {
        head    = SCRATCH_STACK_CURSOR(DesertChaserRoamScratch);
        obj     = arg0->extra.tmd;
        scratch = (SCRATCH_STACK_CURSOR(DesertChaserRoamScratch) = head - 1);
#if DESERT_CHASER_RUN_SEQUENCE
        ctx = arg0->spawnArg2.pointer;
#endif
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
#if DESERT_CHASER_RUN_SEQUENCE
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        work->blendActive                                    = 0;
        work->animId                                         = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->animRate = DESERT_CHASER_SLOT_RATE(work);
#endif
        desertChaserAnimTick(arg0);
        desertChaserAnimTick(arg0);
        work->stateTimer          = 0;
        work->stateCounter        = 0;
        coord                     = arg0->extra.tmd->coords;
        head[-1].toPatrolPoint.vx = (s16)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
        scratch->toPatrolPoint.vy = gPlayerStatus.coordMtx->t[1] - coord->coord.t[1];
        z                         = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
        scratch->toPatrolPoint.vz = z;
        facing                    = arg0->extra.tmd->coords;
        angle                     = ratan2((s32)head[-1].toPatrolPoint.vx, (s32)z);
        delta1                    = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
        wrapped                   = delta1;
        if (delta1 < 0) {
        wrapNegative:
            if (wrapped < -0x800) {
                wrapped += 0x1000;
                goto wrapNegative;
            }
        } else {
        wrapPositive:
            if (wrapped >= 0x801) {
                wrapped -= 0x1000;
                goto wrapPositive;
            }
        }
        work->lookYawTarget = wrapped;
        matrix              = &scratch->rotation;
        gfxRotMatrixY(matrix, (s16)ratan2((s32)scratch->toPatrolPoint.vx, (s32)scratch->toPatrolPoint.vz) + 0x3E8, 1);
        gfxReadMatrixZAxis(matrix, &scratch->toPatrolPoint);
        VectorNormalSS(&scratch->toPatrolPoint, &scratch->toPatrolPoint);
        gte_lddp(1000);
        gte_ldsv(&scratch->toPatrolPoint);
        gte_gpf12();
        gte_stsv(&scratch->toPatrolPoint);
        work->patrolTarget      = 0;
        work->patrolPoints[0].x = (s16)((u16)scratch->toPatrolPoint.vx + arg0->extra.tmd->coords->coord.t[0]);
        SCRATCH_STACK_RELEASE_BLOCK(DesertChaserRoamScratch);
        work->patrolPoints[0].z          = (s16)((u16)scratch->toPatrolPoint.vz + arg0->extra.tmd->coords->coord.t[2]);
        work->wallProbe.shape.ends[1].vz = 0x26C;
        return;
    }
    work->stateCounter        += 1;
    head2                      = SCRATCH_STACK_CURSOR(DesertChaserRoamScratch);
    scratch                    = (SCRATCH_STACK_CURSOR(DesertChaserRoamScratch) = head2 - 1);
    head2[-1].toPatrolPoint.vx = (s16)(work->patrolPoints[work->patrolTarget].x - arg0->extra.tmd->coords->coord.t[0]);
    scratch->toPatrolPoint.vy  = 0;
    scratch->toPatrolPoint.vz  = work->patrolPoints[work->patrolTarget].z - arg0->extra.tmd->coords->coord.t[2];
    coord2                     = arg0->extra.tmd->coords;
    head2[-1].toPlayer.vx      = (s16)(gPlayerStatus.coordMtx->t[0] - coord2->coord.t[0]);
    target                     = &head2[-1].toPlayer;
    target->vy                 = gPlayerStatus.coordMtx->t[1] - coord2->coord.t[1];
    target->vz                 = gPlayerStatus.coordMtx->t[2] - coord2->coord.t[2];
    if (!actorOutsideRadius(&scratch->toPatrolPoint, 0xA0) || work->stateTimer >= 0x15) {
        facing2  = arg0->extra.tmd->coords;
        angle2   = ratan2((s32)head2[-1].toPlayer.vx, (s32)target->vz);
        delta2   = angle2 - ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
        wrapped2 = delta2;
        if (delta2 < 0) {
        wrapNegative2:
            if (wrapped2 < -0x800) {
                wrapped2 += 0x1000;
                goto wrapNegative2;
            }
        } else {
        wrapPositive2:
            if (wrapped2 >= 0x801) {
                wrapped2 -= 0x1000;
                goto wrapPositive2;
            }
        }
        work->lookYawTarget = wrapped2;
        if (work->patrolTarget == 0) {
            gfxRotMatrixY(&scratch->rotation, (s16)ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) - 0x2EE, 1);
            work->patrolTarget = 1;
        } else {
            gfxRotMatrixY(&scratch->rotation, (s16)ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) + 0x2EE, 1);
            work->patrolTarget = 0;
        }
        direction = &scratch->toPlayer;
        gfxReadMatrixZAxis(&scratch->rotation, direction);
        VectorNormalSS(direction, direction);
        gte_lddp(2000);
        gte_ldsv(direction);
        gte_gpf12();
        gte_stsv(direction);
        work->patrolPoints[work->patrolTarget].x = (s16)((u16)scratch->toPlayer.vx + arg0->extra.tmd->coords->coord.t[0]);
        work->patrolPoints[work->patrolTarget].z = (s16)((u16)scratch->toPlayer.vz + arg0->extra.tmd->coords->coord.t[2]);
        work->stateTimer                         = 0;
    }
    desertChaserAnimTick(arg0);
    facing3  = arg0->extra.tmd->coords;
    angle3   = ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz);
    delta3   = angle3 - ratan2((s32)-facing3->coord.m[2][0], (s32)facing3->coord.m[2][2]);
    wrapped3 = delta3;
    if (delta3 < 0) {
    wrapNegative3:
        if (wrapped3 < -0x800) {
            wrapped3 += 0x1000;
            goto wrapNegative3;
        }
    } else {
    wrapPositive3:
        if (wrapped3 >= 0x801) {
            wrapped3 -= 0x1000;
            goto wrapPositive3;
        }
    }
    work->lookYawTarget = wrapped3;
    facing4             = arg0->extra.tmd->coords;
    angle4              = ratan2((s32)scratch->toPatrolPoint.vx, (s32)scratch->toPatrolPoint.vz);
    delta4              = angle4 - ratan2((s32)-facing4->coord.m[2][0], (s32)facing4->coord.m[2][2]);
    wrapped4            = delta4;
    if (delta4 < 0) {
    wrapNegative4:
        if (wrapped4 < -0x800) {
            wrapped4 += 0x1000;
            goto wrapNegative4;
        }
    } else {
    wrapPositive4:
        if (wrapped4 >= 0x801) {
            wrapped4 -= 0x1000;
            goto wrapPositive4;
        }
    }
    turnDelta         = wrapped4;
    scratch->fullTurn = (scratch->turn = (s16)turnDelta);
    delta             = scratch->turn;
    unsignedDelta     = (u16)scratch->turn;
    magnitude         = abs(scratch->turn);
    if (magnitude >= 0x601) {
        targetDelta     = work->lookYawTarget;
        targetMagnitude = abs(targetDelta);
        if ((targetMagnitude >= 0x101) && ((targetDelta * delta) < 0)) {
            adjustedDelta = unsignedDelta - 0x1000;
            if (delta < 0) {
                adjustedDelta = unsignedDelta + 0x1000;
            }
            scratch->turn = adjustedDelta;
        }
    }
    if (scratch->turn >= 0x21) {
        scratch->turn = 0x20;
    }
    if (scratch->turn < -0x20) {
        scratch->turn = -0x20;
    }
    work->waistYawTarget = scratch->turn * 0x10;
    turnCoord            = arg0->extra.tmd->coords;
    yaw                  = (u16)scratch->turn + ratan2((s32)-turnCoord->coord.m[2][0], (s32)turnCoord->coord.m[2][2]);
    scratch->turn        = yaw;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, (s32)yaw, 1);
    record = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    if (work->blendActive == 0) {
        if (desertChaserCapsuleTouchesGrid(arg0)) {
            actorMoveForward(arg0->extra.tmd->coords, 20);
        } else {
            actorMoveForward(arg0->extra.tmd->coords, 20);
        }
        record = work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts;
    }
    ActorContact_Steer(arg0->extra.tmd->coords, record, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_FRONT].contacts), &scratch->toPatrolPoint);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts)) == 1) {
        originalMagnitude = abs(scratch->fullTurn);
        if (originalMagnitude < 0x20) {
            work->stateTimer += 1;
        }
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
#if !DESERT_CHASER_RUN_SEQUENCE
    if (detectSightBlocked(arg0) != 1)
#endif
    {
        coord3               = arg0->extra.tmd->coords;
        scratch->toPlayer.vx = (s16)(gPlayerStatus.coordMtx->t[0] - coord3->coord.t[0]);
        target2              = &scratch->toPlayer;
        target2->vy          = gPlayerStatus.coordMtx->t[1] - coord3->coord.t[1];
        target2->vz          = gPlayerStatus.coordMtx->t[2] - coord3->coord.t[2];
        if (work->stateCounter > work->roamLookDelay) {
            if (work->chaseHoldoff <= 0) {
#if !DESERT_CHASER_RUN_SEQUENCE
                /* each chaser looks on its own frame of fifteen */
                if ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == (gDisplayState.animFrame % 15))
#endif
                {
                    if (actorOutsideRadius(&scratch->toPlayer, radius)) {
                        if (!actorOutsideRadius(&scratch->toPlayer, 0x1F40) && work->stateCounter >= 0x1C3) {
                            facing5  = arg0->extra.tmd->coords;
                            angle5   = ratan2((s32)scratch->toPatrolPoint.vx, (s32)scratch->toPatrolPoint.vz);
                            delta5   = angle5 - ratan2((s32)-facing5->coord.m[2][0], (s32)facing5->coord.m[2][2]);
                            wrapped5 = delta5;
                            if (delta5 < 0) {
                            wrapNegative5:
                                if (wrapped5 < -0x800) {
                                    wrapped5 += 0x1000;
                                    goto wrapNegative5;
                                }
                            } else {
                            wrapPositive5:
                                if (wrapped5 >= 0x801) {
                                    wrapped5 -= 0x1000;
                                    goto wrapPositive5;
                                }
                            }
                            finalDelta    = wrapped5;
                            scratch->turn = (s16)finalDelta;
                            finalDelta    = abs(finalDelta);
                            if (finalDelta < 0x300) {
                                goto changeState;
                            }
                        }
                    } else {
                    changeState:
                        work->state = 0x1C;
                    }
                    playerX                = -(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][0];
                    scratch->playerYaw     = ratan2((s32)playerX, (s32)(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords->coord.m[2][2]);
                    targetYaw              = ratan2((s32)scratch->toPlayer.vx, (s32)scratch->toPlayer.vz) + 0x800;
                    wrappedYaw             = targetYaw;
                    scratch->yawFromPlayer = targetYaw;
                    if (targetYaw < 0) {
                    wrapYawNegative:
                        if (wrappedYaw < -0x800) {
                            wrappedYaw += 0x1000;
                            goto wrapYawNegative;
                        }
                    } else {
                    wrapYawPositive:
                        if (wrappedYaw >= 0x801) {
                            wrappedYaw -= 0x1000;
                            goto wrapYawPositive;
                        }
                    }
                    finalYaw               = wrappedYaw;
                    scratch->yawFromPlayer = (s16)finalYaw;
                    yawDifference          = finalYaw - scratch->playerYaw;
                    if (yawDifference < 0) {
                        yawDifference = -yawDifference;
                    }
                    if ((yawDifference >= 0x601)
#if !DESERT_CHASER_RUN_SEQUENCE
                        || (Gp_NodeSlotMask(&ctx->node) == 0)
#endif
                    ) {
                        work->state = 0x1C;
                    }
                }
            }
#if DESERT_CHASER_RUN_SEQUENCE
            else {
                work->chaseHoldoff -= 1;
            }
#endif
        }
    }
#if !DESERT_CHASER_RUN_SEQUENCE
    if ((work->chaseHoldoff <= 0) && (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_PE_ACTIVE)) {
        work->state = 0x1C;
    }
#else
    func_actor_421600_80133334(arg0->extra.tmd->coords);
#endif
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserRoamScratch);
#if DESERT_CHASER_RUN_SEQUENCE
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
#endif
}
