/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Aims the beam from the actor's fourth part. While `auxTimer` is positive
/// the offset (-0x28, -0x78, 0xDC) from that part, brought into root space,
/// becomes `aimBeamCapsule.ends[1]` and `aimBeamBody` is grid- and
/// pair-enabled; otherwise the body is switched off. Below
/// `GOLEM_KNIGHT_BISHOP_AIM_TIME` - 1 it projects the beam's two ends into
/// `beamScreenX`, `beamScreenY` and `beamDepth` and draws it: the same point
/// on the part, and a point 0x514 above the root as far ahead as the contact
/// in `aimBeamContacts` (10000 when the capsule touched nothing).
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
    if (work->auxTimer > 0) {
        gfxMakeRelativeTransform(&coord->workm, &part->workm, &sc->m);
        sc->pts[1].vx = -0x28;
        sc->pts[1].vy = -0x78;
        sc->pts[1].vz = 0xDC;
        gte_SetRotMatrix(&sc->m);
        gte_ldv0(&sc->pts[1]);
        gte_rtv0();
        gte_stlvnl(&sc->out);
        work->aimBeamCapsule.ends[1].vx = sc->m.t[0] + sc->out.vx;
        work->aimBeamCapsule.ends[1].vy = sc->m.t[1] + sc->out.vy;
        work->aimBeamCapsule.ends[1].vz = sc->m.t[2] + sc->out.vz;
        work->aimBeamBody.flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
    if (work->auxTimer < GOLEM_KNIGHT_BISHOP_AIM_TIME - 1) {
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
        if (Gp_FindRec18(work->aimBeamContacts, 0) != 0) {
            sc->out.vx    = work->aimBeamContacts[0].point.vx - sc->pts[1].vx;
            sc->out.vy    = work->aimBeamContacts[0].point.vy - sc->pts[1].vy;
            sc->out.vz    = work->aimBeamContacts[0].point.vz - sc->pts[1].vz;
            dist          = SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vy * sc->out.vy + sc->out.vz * sc->out.vz);
            sc->pts[0].vz = dist;
            if ((work->aimBeamContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000) {
                sc->pts[0].vz = dist + 0x12C;
            }
            Gp_ClearRec18Occupied(work->aimBeamContacts);
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
            work->beamScreenX[i] = sc->sxy;
            work->beamScreenY[i] = sc->sxy >> 16;
            work->beamDepth[i]   = sc->otz;
        }
        golemKnightBishopDrawAimBeam(arg0);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopAimScratch));
}
