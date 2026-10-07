/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserStrike(Task* arg0)
{
    DesertChaserWork* work;
    Enemy*            ctx;
    TmdObject*        obj;
    SVECTOR*          head;
    SVECTOR*          vec;
    s32               x, z;
    s16               yaw;
    s32               outside;
    s32               state;

    head = SCRATCH_STACK_CURSOR(SVECTOR);
    vec  = (SCRATCH_STACK_CURSOR(SVECTOR) = head - 2);
    work = arg0->work;
    ctx  = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        tmdAllocPrimitiveBuffer(obj);
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->blendActive                                     = 0;
        work->animId                                          = 5;
        work->waistYawTarget                                  = 0;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->playerAnimFrames = 0;
#endif
        work->stateTimer                                     = 0;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->animRate                                       = work->baseRate;
        actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(0x20);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        x                                  = head[-2].vx;
        work->playerMove.displacement.vy   = 0;
        work->playerMove.displacement.vx   = x;
        z                                  = vec->vz;
        work->playerMove.collisionRequests = GAME_ACTOR_COLLISION_REQUEST_MASK;
        work->playerMove.keepControl       = 1;
        work->wallProbe.shape.ends[1].vz   = 0x320;
        work->playerMove.displacement.vz   = z;
#if DESERT_CHASER_RUN_SEQUENCE
        if ((work->lastCommand.word & DESERT_CHASER_COMMAND_MASK) == DESERT_CHASER_COMMAND_WATER_TOWER_1) {
            work->state = 5;
        }
#else
        padScriptSpawnVariableMotorRamp(3, 0xFF, 8);
#endif
    }
    work->stateTimer += 1;
    _desertChaserAnimTick(arg0);
    state = work->animId;
    switch (state) {
        case 5:
            if (work->rig.slots[1].status.fields.flags & 0x100) {
                actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, vec);
                outside = actorOutsideRadius(vec, 2000);
                if (outside) {
                    work->state = 0x26;
                } else {
                    work->state = 0x1F;
                }
            }
            break;
        case 3:
            yaw       = actorPositionYaw(arg0, vec, &gPlayerStatus);
            vec[1].vz = yaw;
            if (_desertChaserCapsuleTouchesGrid(arg0)) {
                _actorMovementStepForward(arg0->extra.tmd->coords, 85);
            } else {
                _actorMovementStepForward(arg0->extra.tmd->coords, 200);
            }
            if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts))) {
                work->state = 0x23;
            }
            if (work->stateTimer >= 0x15) {
                work->animRequest = DESERT_CHASER_ANIM_REQUEST_BLEND;
                work->blendActive = 0;
                work->animId      = 5;
                work->animRate    = work->baseRate;
            }
            break;
    }
    SCRATCH_STACK_CURSOR(SVECTOR) += 2;
}
