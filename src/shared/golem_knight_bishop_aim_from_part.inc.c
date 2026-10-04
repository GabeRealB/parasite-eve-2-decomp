/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Scratch-stack block the aim beam is set up in.
///
/// The point the beam leaves is a fixed offset from part 4. It is carried
/// into the root's frame for the beam's collision capsule, and into world
/// space, with the beam's far end, for the projection through `GsWSMATRIX`.
/// Reserved for the length of the call; nothing in it outlives that.
typedef struct {
    MATRIX  partInRoot; // part 4's transform expressed in the root's frame
    VECTOR  offset;     // result of a GTE rotation: a point's offset along the target frame's axes, before that frame's translation is added; also the contact point less the point on part 4, measured for the beam's length
    SVECTOR ends[2];    // the beam's ends, each first in its own frame and then in world space: [0] the far end, placed from the root; [1] the point on part 4
    s32     screenXY;   // projected screen position of the end last transformed: x in the low half, y in the high
    s32     depth;      // its ordering depth, a quarter of the screen z
} _GolemKnightBishopAimScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopAimScratch, 0x48);

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
    u8*                           head;
    _GolemKnightBishopAimScratch* sc;
    GolemKnightBishopWork*        work;
    GfxCoord*                     coord;
    GfxCoord*                     part;
    s32                           i;
    s16                           dist;

    coord                    = arg0->extra.tmd->coords;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(_GolemKnightBishopAimScratch);
    sc                       = (_GolemKnightBishopAimScratch*)(head - sizeof(_GolemKnightBishopAimScratch));
    work                     = arg0->work;
    part                     = &coord[3] + 1;
    coord->composeStamp      = GRAPHICS_COORD_DIRTY;
    part->composeStamp       = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(part);
    if (work->auxTimer > 0) {
        gfxMakeRelativeTransform(&coord->workm, &part->workm, &sc->partInRoot);
        sc->ends[1].vx = -0x28;
        sc->ends[1].vy = -0x78;
        sc->ends[1].vz = 0xDC;
        gte_SetRotMatrix(&sc->partInRoot);
        gte_ldv0(&sc->ends[1]);
        gte_rtv0();
        gte_stlvnl(&sc->offset);
        work->aimBeamCapsule.ends[1].vx = sc->partInRoot.t[0] + sc->offset.vx;
        work->aimBeamCapsule.ends[1].vy = sc->partInRoot.t[1] + sc->offset.vy;
        work->aimBeamCapsule.ends[1].vz = sc->partInRoot.t[2] + sc->offset.vz;
        work->aimBeamBody.flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
    if (work->auxTimer < GOLEM_KNIGHT_BISHOP_AIM_TIME - 1) {
        sc->ends[1].vx = -0x28;
        sc->ends[1].vy = -0x78;
        sc->ends[1].vz = 0xDC;
        gte_SetRotMatrix(&part->workm);
        gte_ldv0(&sc->ends[1]);
        gte_rtv0();
        gte_stlvnl(&sc->offset);
        sc->ends[1].vx = part->workm.t[0] + sc->offset.vx;
        sc->ends[1].vy = part->workm.t[1] + sc->offset.vy;
        sc->ends[1].vz = part->workm.t[2] + sc->offset.vz;
        sc->ends[0].vx = 0;
        sc->ends[0].vy = -0x514;
        if (Gp_FindRec18(work->aimBeamContacts, 0) != 0) {
            sc->offset.vx  = work->aimBeamContacts[0].point.vx - sc->ends[1].vx;
            sc->offset.vy  = work->aimBeamContacts[0].point.vy - sc->ends[1].vy;
            sc->offset.vz  = work->aimBeamContacts[0].point.vz - sc->ends[1].vz;
            dist           = SquareRoot0(sc->offset.vx * sc->offset.vx + sc->offset.vy * sc->offset.vy + sc->offset.vz * sc->offset.vz);
            sc->ends[0].vz = dist;
            if ((work->aimBeamContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == 0x10000) {
                sc->ends[0].vz = dist + 0x12C;
            }
            Gp_ClearRec18Occupied(work->aimBeamContacts);
        } else {
            sc->ends[0].vz = 10000;
        }
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&sc->ends[0]);
        gte_rtv0();
        gte_stlvnl(&sc->offset);
        sc->ends[0].vx = coord->workm.t[0] + sc->offset.vx;
        sc->ends[0].vy = coord->workm.t[1] + sc->offset.vy;
        sc->ends[0].vz = coord->workm.t[2] + sc->offset.vz;
        for (i = 0; i < 2; i++) {
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_ldv0(&sc->ends[i]);
            gte_rtps();
            gte_stsxy(&sc->screenXY);
            gte_stszotz(&sc->depth);
            work->beamScreenX[i] = sc->screenXY;
            work->beamScreenY[i] = sc->screenXY >> 16;
            work->beamDepth[i]   = sc->depth;
        }
        golemKnightBishopDrawAimBeam(arg0);
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopAimScratch));
}
