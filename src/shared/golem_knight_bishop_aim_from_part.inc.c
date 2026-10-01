/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Aims the actor off its fourth part. While `field_6D6` is positive the
/// offset (-0x28, -0x78, 0xDC) through the root-to-part matrix lands in
/// `field_634`..`field_638` with bits 0xC000 of `field_62A` raised. Below 0x13
/// it projects two points into `field_6FC`..`field_704`: the same offset off
/// the part, and a point 0x514 up and the `field_644` target's distance out
/// from the root.
void golemKnightBishopAimFromPart(Task* arg0)
{
    u8*                          head;
    GolemKnightBishopAimScratch* sc;
    GolemKnightBishopWork*       work;
    GfxCoord*                    coord;
    GfxCoord*                    part;
    s32                          i;
    s16                          dist;

    coord                    = arg0->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(GolemKnightBishopAimScratch);
    sc                       = (GolemKnightBishopAimScratch*)(head - sizeof(GolemKnightBishopAimScratch));
    work                     = arg0->work;
    part                     = &coord[3] + 1;
    coord->composeStamp      = GRAPHICS_COORD_DIRTY;
    part->composeStamp       = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(part);
    if (work->field_6D6 > 0) {
        gfxMakeRelativeTransform(&coord->workm, &part->workm, &sc->m);
        sc->pts[1].vx = -0x28;
        sc->pts[1].vy = -0x78;
        sc->pts[1].vz = 0xDC;
        gte_SetRotMatrix(&sc->m);
        gte_ldv0(&sc->pts[1]);
        gte_rtv0();
        gte_stlvnl(&sc->out);
        work->field_634  = sc->m.t[0] + sc->out.vx;
        work->field_636  = sc->m.t[1] + sc->out.vy;
        work->field_638  = sc->m.t[2] + sc->out.vz;
        work->field_62A |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
    if (work->field_6D6 < GOLEM_KNIGHT_BISHOP_AIM_TIME - 1) {
        sc->pts[1].vx = -0x28;
        sc->pts[1].vy = -0x78;
        sc->pts[1].vz = 0xDC;
        gte_SetRotMatrix(&part->workm);
        gte_ldv0(&sc->pts[1]);
        gte_rtv0();
        gte_stlvnl(&sc->out);
        sc->pts[1].vx = part->workm.t[0] + sc->out.vx;
        sc->pts[1].vy = part->workm.t[1] + sc->out.vy;
        sc->pts[1].vz = part->workm.t[2] + sc->out.vz;
        sc->pts[0].vx = 0;
        sc->pts[0].vy = -0x514;
        if (Gp_FindRec18(&work->field_644, 0) != 0) {
            sc->out.vx    = work->field_644.point.vx - sc->pts[1].vx;
            sc->out.vy    = work->field_644.point.vy - sc->pts[1].vy;
            sc->out.vz    = work->field_644.point.vz - sc->pts[1].vz;
            dist          = SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vy * sc->out.vy + sc->out.vz * sc->out.vz);
            sc->pts[0].vz = dist;
            if ((work->field_644.key.value & 0xFFFF0000) == 0x10000) {
                sc->pts[0].vz = dist + 0x12C;
            }
            Gp_ClearRec18Occupied(&work->field_644);
        } else {
            sc->pts[0].vz = 10000;
        }
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&sc->pts[0]);
        gte_rtv0();
        gte_stlvnl(&sc->out);
        sc->pts[0].vx = coord->workm.t[0] + sc->out.vx;
        sc->pts[0].vy = coord->workm.t[1] + sc->out.vy;
        sc->pts[0].vz = coord->workm.t[2] + sc->out.vz;
        for (i = 0; i < 2; i++) {
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_ldv0(&sc->pts[i]);
            gte_rtps();
            gte_stsxy(&sc->sxy);
            gte_stszotz(&sc->otz);
            work->field_6FC[i] = sc->sxy;
            work->field_700[i] = sc->sxy >> 16;
            work->field_704[i] = sc->otz;
        }
        golemKnightBishopDrawAimBeam(arg0);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopAimScratch));
}
