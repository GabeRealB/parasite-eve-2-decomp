/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Converts the root coordinate's world matrix into the frame of part 7 and
/// parks the (0, 100, -100) offset rotated through it, plus its translation,
/// in `laserCapsule.ends[1]`; then stores the (0, -0x514, 10000) vector rotated by
/// the (-5, -5, 0) matrix in `laserCapsule.ends[0]` and raises the fifth body
/// object's 0xC000 flags. While `timer` is non-zero, the parked point is
/// taken back to world space, the distance to the first `laserContacts` hit (10000
/// with none, plus 1000 for a kind-0x1 hit) replaces the vector's depth, and
/// the rotated result and the parked point go to `golemPawnRookDrawLaserBeam`.
void golemPawnRookAimLaserSight(Task* arg0)
{
    GolemPawnRookAimScratch* scratch;
    GolemPawnRookWork*       work;
    GfxCoord*                self;

    scratch              = (GolemPawnRookAimScratch*)SCRATCH_STACK_RESERVE_BYTES(0x40);
    self                 = arg0->extra.tmd->coords;
    work                 = arg0->work;
    self[0].composeStamp = GRAPHICS_COORD_DIRTY;
    self[7].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&self[7]);
    gfxMakeRelativeTransform(&self->workm, &self[7].workm, &scratch->mtx);
    scratch->vec.vy = 100;
    scratch->vec.vx = 0;
    scratch->vec.vz = -100;
    gte_SetRotMatrix(&scratch->mtx);
    gte_ldv0(&scratch->vec);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    work->laserCapsule.ends[1].vx = scratch->mtx.t[0] + scratch->pos.vx;
    work->laserCapsule.ends[1].vy = scratch->mtx.t[1] + scratch->pos.vy;
    work->laserCapsule.ends[1].vz = scratch->mtx.t[2] + scratch->pos.vz;
    scratch->rot.vx               = -5;
    scratch->rot.vy               = -5;
    scratch->rot.vz               = 0;
    RotMatrix(&scratch->rot, &scratch->mtx);
    scratch->rot.vx = 0;
    scratch->rot.vy = -0x514;
    scratch->rot.vz = 10000;
    gte_SetRotMatrix(&scratch->mtx);
    gte_ldv0(&scratch->rot);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    work->laserCapsule.ends[0].vx = scratch->pos.vx;
    work->laserCapsule.ends[0].vy = scratch->pos.vy;
    work->laserCapsule.ends[0].vz = scratch->pos.vz;
    work->laserBody.flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (work->timer == 0) {
        SCRATCH_STACK_RELEASE_BYTES(0x40);
        return;
    }
    gte_SetRotMatrix(&self->workm);
    scratch->vec.vx = work->laserCapsule.ends[1].vx;
    scratch->vec.vy = work->laserCapsule.ends[1].vy;
    scratch->vec.vz = work->laserCapsule.ends[1].vz;
    gte_ldv0(&scratch->vec);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    scratch->vec.vx = scratch->pos.vx + self->workm.t[0];
    scratch->vec.vy = scratch->pos.vy + self->workm.t[1];
    scratch->vec.vz = scratch->pos.vz + self->workm.t[2];
    scratch->rot.vx = work->laserCapsule.ends[0].vx;
    scratch->rot.vy = work->laserCapsule.ends[0].vy;
    if (Gp_FindRec18(work->laserContacts, 0) != 0) {
        scratch->pos.vx = work->laserContacts[0].point.vx - scratch->vec.vx;
        scratch->pos.vy = work->laserContacts[0].point.vy - scratch->vec.vy;
        scratch->pos.vz = work->laserContacts[0].point.vz - scratch->vec.vz;
        scratch->rot.vz = SquareRoot0(scratch->pos.vx * scratch->pos.vx + scratch->pos.vy * scratch->pos.vy +
                                      scratch->pos.vz * scratch->pos.vz);
        if ((work->laserContacts[0].key.value & 0xFFFF0000) == 0x10000) {
            scratch->rot.vz += 1000;
        }
    } else {
        scratch->rot.vz = 10000;
    }
    Gp_ClearRec18Occupied(work->laserContacts);
    gte_SetRotMatrix(&scratch->mtx);
    gte_ldv0(&scratch->rot);
    gte_rtv0();
    gte_stlvnl(&scratch->pos);
    scratch->rot.vx = scratch->pos.vx;
    scratch->rot.vy = scratch->pos.vy;
    scratch->rot.vz = scratch->pos.vz;
    scratch->vec.vx = work->laserCapsule.ends[1].vx;
    scratch->vec.vy = work->laserCapsule.ends[1].vy;
    scratch->vec.vz = work->laserCapsule.ends[1].vz;
    golemPawnRookDrawLaserBeam(arg0, &scratch->rot, &scratch->vec);
    SCRATCH_STACK_RELEASE_BYTES(0x40);
}
