/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserTurnStepProbe(Task* arg0)
{
    TmdObject*                   obj;
    Enemy*                       ctx;
    DesertChaserWork*            work;
    GfxCoord*                    coord;
    GfxCoord*                    coord2;
    GfxCoord*                    targetCoord;
    GfxCoord*                    facing2;
    DesertChaserTurnStepScratch* head;
    DesertChaserTurnStepScratch* scratch;
    s16                          yaw;
    s16                          z;
    s16                          steps;
    s32                          firstDelta;

    head    = SCRATCH_STACK_CURSOR(DesertChaserTurnStepScratch);
    scratch = (SCRATCH_STACK_CURSOR(DesertChaserTurnStepScratch) = head - 1);
    work    = arg0->work;
    ctx     = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        obj = arg0->extra.tmd;
#if !DESERT_CHASER_RUN_SEQUENCE
        work->hitFlag = 0;
#endif
        obj->flags                                            = 0;
        work->spheres[DESERT_CHASER_SPHERE_FRONT].body.radius = 0x19C;
        work->spheres[DESERT_CHASER_SPHERE_ROOT].body.flags  |= WORLD_COLLISION_BODY_GRID_ENABLED;
        ctx->node.state.parts.flags                           = 0;
        work->animRequest                                     = DESERT_CHASER_ANIM_REQUEST_BLEND;
        work->animId                                          = DESERT_CHASER_CLIP_TURN_PROBE;
        work->animRate                                        = 0x10;
        work->stateTimer                                      = 0;
    }
    work->stateTimer += 1;
    desertChaserAnimTick(arg0);
    targetCoord         = arg0->extra.tmd->coords;
    head[-1].offset.vx  = (s16)(gPlayerStatus.coordMtx->t[0] - targetCoord->coord.t[0]);
    scratch->offset.vy  = gPlayerStatus.coordMtx->t[1] - targetCoord->coord.t[1];
    z                   = gPlayerStatus.coordMtx->t[2] - targetCoord->coord.t[2];
    scratch->offset.vz  = z;
    firstDelta          = actorYawTo(arg0->extra.tmd->coords, head[-1].offset.vx, z);
    scratch->turn       = (s16)firstDelta;
    work->lookYawTarget = (u16)firstDelta;
    if (scratch->turn > 0) {
        if (abs(scratch->turn) >= 0x401) {
            work->lookYawTarget = firstDelta - 0x800;
            scratch->turn      -= 0x800;
        }
    }
    steps              = 0x1E - work->stateTimer;
    scratch->stepsLeft = steps;
    if (steps == 0) {
        scratch->stepsLeft = 1;
    }
    facing2          = arg0->extra.tmd->coords;
    yaw              = ((s16)scratch->turn / (s16)scratch->stepsLeft) + ratan2((s32)-facing2->coord.m[2][0], (s32)facing2->coord.m[2][2]);
    scratch->heading = yaw;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, (s32)yaw, 1);
    gfxReadMatrixZAxis(&arg0->extra.tmd->coords->coord, &scratch->offset);
    VectorNormalSS(&scratch->offset, &scratch->offset);
    gte_lddp(0x1A);
    gte_ldsv(&scratch->offset);
    gte_gpf12();
    gte_stsv(&scratch->offset);
    coord               = arg0->extra.tmd->coords;
    coord->coord.t[0]  += scratch->offset.vx;
    coord2              = arg0->extra.tmd->coords;
    coord2->coord.t[2] += scratch->offset.vz;
    _actorMovementStepForward(arg0->extra.tmd->coords, -8);
    _actorContactApplyGridPushback(arg0->extra.tmd->coords, work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts, ARRAY_SIZE(work->spheres[DESERT_CHASER_SPHERE_ROOT].contacts));
#if DESERT_CHASER_RUN_SEQUENCE
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
#endif
    if (abs(scratch->turn) < 0x20) {
        work->state = 0x1C;
    }
    if (work->rig.slots[1].status.fields.flags & 0x100) {
        work->state = 0x1C;
    }
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserTurnStepScratch);
}
