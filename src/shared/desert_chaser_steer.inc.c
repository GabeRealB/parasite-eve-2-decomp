/* Part of the Desert Chaser library; see desert_chaser.h. */

void desertChaserSteer(Task* arg0)
{
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    SVECTOR delta;
#else
#endif
    DesertChaserWork* work;
    Enemy*            enemy;
    TmdObject*        obj;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
    GfxCoord*            coord;
    MATRIX*              target;
    OverlayRangeScratch* savedScratchHead;
    OverlayRangeScratch* rangeScratch;
    SVECTOR              vec;
    SVECTOR*             direction;
    u8*                  scratchBase;
    u8*                  scratchRestoreBase;
#endif
    s32 radius;

    work = arg0->work;
    if (work->field_4 != 0) {
        enemy                                                     = arg0->spawnArg2.pointer;
        obj                                                       = arg0->extra.tmd;
        ((Enemy*)arg0->spawnArg2.pointer)->node.state.parts.flags = 0;
        obj->flags                                                = 0;
        Tmd_AllocBuffers(obj);
        work->objs[0].obj.radius = 0x19C;
        work->field_828          = 2;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
#else
        work->field_832 = 0x10;
        work->field_82A = 0;
#endif
        work->field_82E = 7;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->field_82A = 0;
#else
#endif
        work->objs[2].obj.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
        work->field_832 = work->field_834;
#else
#endif
        desertChaserArmedAnimTick(arg0);
    }
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    radius = 2000;
#else
#endif
    desertChaserArmedAnimTick(arg0);
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    if (((s16)ActorContact_Steer(arg0->extra.tmd->coords, work->objs[0].contacts, 5, &delta) != 0) || ((s16)ActorContact_Steer(arg0->extra.tmd->coords, work->objs[1].contacts, 5, &delta) != 0)) {
#else
    if (((ActorContact_Steer(arg0->extra.tmd->coords, work->objs[0].contacts, 0xC, &vec) << 0x10) != 0) || ((ActorContact_Steer(arg0->extra.tmd->coords, work->objs[1].contacts, 0xC, &vec) << 0x10) != 0)) {
#endif
        work->field_0 = 0x22;
    }
#if DESERT_CHASER_BUILD == DESERT_CHASER_REGULAR
    Actor00100_PositionDelta(arg0->extra.tmd->coords, &delta);
    if (actorOutsideRadius(&delta, radius) == 0) {
#else
    target        = gPlayerStatus.coordMtx;
    coord         = arg0->extra.tmd->coords;
    vec.vx        = (u16)target->t[0] - (u16)coord->coord.t[0];
    direction     = &vec;
    direction->vy = (u16)target->t[1] - (u16)coord->coord.t[1];
    direction->vz = (u16)target->t[2] - (u16)coord->coord.t[2];
    // Reserve squared-distance operands; no allocation intervenes after release.
    savedScratchHead                                                       = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    rangeScratch                                                           = savedScratchHead - 1;
    scratchBase                                                            = PLAYSTATION_SCRATCHPAD_BASE;
    *(OverlayRangeScratch**)(scratchBase + SCRATCH_STACK_HEAD_BYTE_OFFSET) = rangeScratch;
    rangeScratch->dx                                                       = vec.vx;
    rangeScratch->dz                                                       = direction->vz;
    rangeScratch->r                                                        = 0x5DC;
    rangeScratch->dx                                                       = rangeScratch->dx * rangeScratch->dx;
    rangeScratch->dz                                                       = rangeScratch->dz * rangeScratch->dz;
    rangeScratch->r                                                        = rangeScratch->r * rangeScratch->r;
    scratchRestoreBase                                                     = PLAYSTATION_SCRATCHPAD_BASE + (SCRATCH_STACK_HEAD_BYTE_OFFSET - sizeof(void*));
    *(OverlayRangeScratch**)(scratchRestoreBase + sizeof(void*))           = savedScratchHead;
    radius                                                                 = rangeScratch->dx + rangeScratch->dz >= rangeScratch->r;
    if (radius == 0) {
#endif
        work->field_0 = 0x22;
    }
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}
