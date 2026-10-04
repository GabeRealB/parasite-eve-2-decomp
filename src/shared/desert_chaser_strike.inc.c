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
    if (work->field_4 != 0) {
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
        work->field_82A          = 0;
        work->field_82E          = 5;
        work->field_83E          = 0;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->field_C28 = 0;
#endif
        work->field_6            = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->field_832          = work->field_834;
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
        work->capsuleBody.shape.ends[1].vz = 0x320;
        work->playerMove.displacement.vz   = z;
#if DESERT_CHASER_RUN_SEQUENCE
        if ((work->actorId.word & DESERT_CHASER_COMMAND_MASK) == DESERT_CHASER_COMMAND_WATER_TOWER_1) {
            work->field_0 = 5;
        }
#else
        Gp_SpawnPadLerp(3, 0xFF, 8);
#endif
    }
    work->field_6 += 1;
    desertChaserAnimTick(arg0);
    state = work->field_82E;
    switch (state) {
        case 5:
            if (work->slots[1].status.fields.flags & 0x100) {
                actorConfigPositionDelta(&gPlayerStatus, arg0->extra.tmd->coords, vec);
                outside = actorOutsideRadius(vec, 2000);
                if (outside) {
                    work->field_0 = 0x26;
                } else {
                    work->field_0 = 0x1F;
                }
            }
            break;
        case 3:
            yaw       = actorPositionYaw(arg0, vec, &gPlayerStatus);
            vec[1].vz = yaw;
            if (desertChaserCapsuleTouchesGrid(arg0)) {
                actorMoveForward(arg0->extra.tmd->coords, 85);
            } else {
                actorMoveForward(arg0->extra.tmd->coords, 200);
            }
            if (ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, DESERT_CHASER_CONTACTS)) {
                work->field_0 = 0x23;
            }
            if (work->field_6 >= 0x15) {
                work->field_828 = 1;
                work->field_82A = 0;
                work->field_82E = 5;
                work->field_832 = work->field_834;
            }
            break;
    }
    SCRATCH_STACK_CURSOR(SVECTOR) += 2;
}
