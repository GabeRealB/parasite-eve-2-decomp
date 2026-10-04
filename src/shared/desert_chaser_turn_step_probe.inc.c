/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserTurnStepProbe(Task* arg0)
{
    TmdObject*            obj;
    Enemy*                ctx;
    DesertChaserWork*     work;
    GfxCoord*             coord;
    GfxCoord*             coord2;
    GfxCoord*             targetCoord;
    GfxCoord*             facing;
    GfxCoord*             facing2;
    ActorTurnStepScratch* head;
    ActorTurnStepScratch* scratch;
    s16                   yaw;
    s16                   delta;
    s16                   z;
    s16                   steps;
    s16                   wrapped;
    s32                   angle;
    s32                   firstDelta;

    head    = SCRATCH_STACK_CURSOR(ActorTurnStepScratch);
    scratch = (SCRATCH_STACK_CURSOR(ActorTurnStepScratch) = head - 1);
    work    = arg0->work;
    ctx     = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        obj = arg0->extra.tmd;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->hitFlag = 0;
#endif
        obj->flags                  = 0;
        work->objs[0].body.radius   = 0x19C;
        work->objs[2].body.flags   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        ctx->node.state.parts.flags = 0;
        work->field_828             = 1;
        work->field_82E             = DESERT_CHASER_CLIP_TURN_PROBE;
        work->field_832             = 0x10;
        work->field_6               = 0;
    }
    work->field_6 += 1;
    desertChaserAnimTick(arg0);
    targetCoord     = arg0->extra.tmd->coords;
    head[-1].vec.vx = (s16)(gPlayerStatus.coordMtx->t[0] - targetCoord->coord.t[0]);
    scratch->vec.vy = gPlayerStatus.coordMtx->t[1] - targetCoord->coord.t[1];
    z               = gPlayerStatus.coordMtx->t[2] - targetCoord->coord.t[2];
    scratch->vec.vz = z;
    facing          = arg0->extra.tmd->coords;
    angle           = ratan2((s32)head[-1].vec.vx, (s32)z);
    delta           = angle - ratan2((s32)-facing->coord.m[2][0], (s32)facing->coord.m[2][2]);
    wrapped         = delta;
    if (delta < 0) {
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
    firstDelta      = wrapped;
    scratch->delta  = (s16)firstDelta;
    work->field_840 = (u16)firstDelta;
    if (scratch->delta > 0) {
        if (abs(scratch->delta) >= 0x401) {
            work->field_840 = firstDelta - 0x800;
            scratch->delta -= 0x800;
        }
    }
    steps          = 0x1E - work->field_6;
    scratch->steps = steps;
    if (steps == 0) {
        scratch->steps = 1;
    }
    facing2      = arg0->extra.tmd->coords;
    yaw          = ((s16)scratch->delta / (s16)scratch->steps) + ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    scratch->yaw = yaw;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, (s32)yaw, 1);
    gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &scratch->vec);
    VectorNormalSS(&scratch->vec, &scratch->vec);
    gte_lddp(0x1A);
    gte_ldsv(&scratch->vec);
    gte_gpf12();
    gte_stsv(&scratch->vec);
    coord               = arg0->extra.tmd->coords;
    coord->coord.t[0]  += scratch->vec.vx;
    coord2              = arg0->extra.tmd->coords;
    coord2->coord.t[2] += scratch->vec.vz;
    actorMoveForward(arg0->extra.tmd->coords, -8);
    ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, ARRAY_SIZE(work->objs[2].contacts));
#if DESERT_CHASER_RUN_SEQUENCE
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
#endif
    if (abs(scratch->delta) < 0x20) {
        work->field_0 = 0x1C;
    }
    if (work->slots[1].status.fields.flags & 0x100) {
        work->field_0 = 0x1C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnStepScratch);
}
