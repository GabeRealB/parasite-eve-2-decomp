/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserApproach(Task* arg0)
{
    DesertChaserWork*      work;
    Enemy*                 ctx;
    DesertChaserWork*      move;
    ActorTurnScratch*      head;
    ActorTurnScratch*      scratch;
    TmdObject*             obj;
    GfxCoord*              coord;
    GfxCoord*              playerCoord;
    GfxCoord*              turnCoord;
    WorldCollisionContact* records;
    u16                    angle;
    s16                    delta;
    s32                    value;
    s32                    magnitude;
    s16                    yaw;

    work = arg0->work;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    if (work->field_4 != 0) {
#endif
    ctx = arg0->spawnArg2.pointer;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    if (work->field_4 != 0) {
#else
#endif
        obj                         = arg0->extra.tmd;
        ctx->node.state.parts.flags = 0;
        obj->flags                  = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 1;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
        work->field_832 = 0x10;
#endif
        work->field_82A          = 0;
        work->field_82E          = 0;
        work->field_83E          = 0;
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->field_832 = work->field_834;
#else
#endif
        desertChaserArmedAnimTick(arg0);
        desertChaserArmedAnimTick(arg0);
        work->field_6                      = 0;
        work->capsuleBody.shape.ends[1].vz = 0x26C;
        return;
    }
    head              = SCRATCH_STACK_CURSOR(ActorTurnScratch);
    scratch           = (SCRATCH_STACK_CURSOR(ActorTurnScratch) = head - 1);
    move              = (DesertChaserWork*)work;
    head[-1].delta.vx = move->field_C[move->field_14].x - arg0->extra.tmd->coords->coord.t[0];
    scratch->delta.vy = 0;
    scratch->delta.vz = move->field_C[move->field_14].z - arg0->extra.tmd->coords->coord.t[2];
    if (!actorOutsideRadius(&scratch->delta, 0xA0) || (s16)work->field_6 >= 0x15) {
        if (move->field_14 == 0)
            move->field_14 = 1;
        else
            move->field_14 = 0;
        work->field_6 = 0;
    }
    desertChaserArmedAnimTick(arg0);
    coord           = arg0->extra.tmd->coords;
    angle           = ratan2(scratch->delta.vx, scratch->delta.vz);
    delta           = angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    value           = actorNormalizeYaw(delta);
    scratch->angle  = value;
    work->field_840 = value;
    if (scratch->angle >= 0x11)
        scratch->angle = 0x10;
    if (scratch->angle < -0x10)
        scratch->angle = -0x10;
    work->field_83E = scratch->angle;
    turnCoord       = arg0->extra.tmd->coords;
    yaw             = (u16)scratch->angle + ratan2(-turnCoord->coord.m[2][0], turnCoord->coord.m[2][2]);
    scratch->angle  = yaw;
    gfxRotMatrixY(&arg0->extra.tmd->coords->coord, yaw, 1);
    records = work->objs[0].contacts;
    if (work->field_82A == 0) {
        if (desertChaserCapsuleTouchesGrid(arg0)) {
            actorMoveForward(arg0->extra.tmd->coords, 20);
        } else {
            actorMoveForward(arg0->extra.tmd->coords, 20);
        }
        records = work->objs[0].contacts;
    }
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    ActorContact_Steer(arg0->extra.tmd->coords, records, 5, &scratch->delta);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 5) == 1) {
#else
    ActorContact_Steer(arg0->extra.tmd->coords, records, 0xC, &scratch->delta);
    if (ActorContact_PushContact(arg0->extra.tmd->coords, work->objs[2].contacts, 0xC) == 1) {
#endif
        magnitude = abs((s16)work->field_840);
        if (magnitude < 0x80)
            work->field_6 = (u16)work->field_6 + 1;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    if (detectSightBlocked(arg0) != 1) {
#else
#endif
        playerCoord       = arg0->extra.tmd->coords;
        scratch->delta.vx = gPlayerStatus.coordMtx->t[0] - playerCoord->coord.t[0];
        scratch->delta.vy = gPlayerStatus.coordMtx->t[1] - playerCoord->coord.t[1];
        scratch->delta.vz = gPlayerStatus.coordMtx->t[2] - playerCoord->coord.t[2];
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        if ((ctx->placeKey >> ENEMY_PLACE_INDEX_SHIFT) == gDisplayState.animFrame % 15) {
            if (!overlayOutOfRange(&scratch->delta, 2000)) {
                Gp_ArmStateF0(1);
                work->field_0 = 0x26;
            } else if (!overlayOutOfRange(&scratch->delta, 4000)) {
#else
    if (!actorOutsideRadius(&scratch->delta, 2000)) {
        work->field_0 = 0x1C;
    } else if (!actorOutsideRadius(&scratch->delta, 4000)) {
#endif
                coord          = arg0->extra.tmd->coords;
                angle          = ratan2(scratch->delta.vx, scratch->delta.vz);
                delta          = angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
                value          = actorNormalizeYaw(delta);
                scratch->angle = value;
                value          = abs(value);
                if (value < 0x300) {
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
                    Gp_ArmStateF0(1);
                    work->field_0 = 0x26;
#else
            work->field_0 = 0x1C;
#endif
                }
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
            }
        }
        if (gSceneCombatState.signals.bytes.actionFlags & SCENE_COMBAT_ACTION_NOISE)
            work->field_0 = 0x26;
#else
#endif
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorTurnScratch);
}
