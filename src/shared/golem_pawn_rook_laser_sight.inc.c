/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Scratch-stack block the laser sight is aimed in.
///
/// Reserved for the length of the call. The sight runs from the launcher's
/// muzzle along a fixed aim; the two short vectors follow its two ends through
/// the spaces the steps work in.
typedef struct {
    MATRIX  matrix; // part 7's transform relative to the root, which carries the muzzle into root space; then the rotation built from the aim angles
    VECTOR  offset; // result of each GTE rotation; also the first laser contact's position less the muzzle's, whose length is the beam's reach
    SVECTOR aim;    // the aim's Euler angles (0x1000 to the turn); then the sight's far end, before and after the aim rotation, in root space
    SVECTOR muzzle; // the sight's near end: its offset along part 7's axes, then its composed position for measuring the contact, then its root-space position for the beam
} _GolemPawnRookLaserSightScratch;
STATIC_ASSERT_SIZEOF(_GolemPawnRookLaserSightScratch, 0x40);

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
    _GolemPawnRookLaserSightScratch* scratch;
    GolemPawnRookWork*               work;
    GfxCoord*                        self;

    scratch              = SCRATCH_STACK_RESERVE_BLOCK(_GolemPawnRookLaserSightScratch);
    self                 = arg0->extra.tmd->coords;
    work                 = arg0->work;
    self[0].composeStamp = GRAPHICS_COORD_DIRTY;
    self[7].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&self[7]);
    gfxMakeRelativeTransform(&self->workm, &self[7].workm, &scratch->matrix);
    scratch->muzzle.vy = 100;
    scratch->muzzle.vx = 0;
    scratch->muzzle.vz = -100;
    gte_SetRotMatrix(&scratch->matrix);
    gte_ldv0(&scratch->muzzle);
    gte_rtv0();
    gte_stlvnl(&scratch->offset);
    work->laserCapsule.ends[1].vx = scratch->matrix.t[0] + scratch->offset.vx;
    work->laserCapsule.ends[1].vy = scratch->matrix.t[1] + scratch->offset.vy;
    work->laserCapsule.ends[1].vz = scratch->matrix.t[2] + scratch->offset.vz;
    scratch->aim.vx               = -5;
    scratch->aim.vy               = -5;
    scratch->aim.vz               = 0;
    RotMatrix(&scratch->aim, &scratch->matrix);
    scratch->aim.vx = 0;
    scratch->aim.vy = -0x514;
    scratch->aim.vz = 10000;
    gte_SetRotMatrix(&scratch->matrix);
    gte_ldv0(&scratch->aim);
    gte_rtv0();
    gte_stlvnl(&scratch->offset);
    work->laserCapsule.ends[0].vx = scratch->offset.vx;
    work->laserCapsule.ends[0].vy = scratch->offset.vy;
    work->laserCapsule.ends[0].vz = scratch->offset.vz;
    work->laserBody.flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (work->timer == 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookLaserSightScratch);
        return;
    }
    gte_SetRotMatrix(&self->workm);
    scratch->muzzle.vx = work->laserCapsule.ends[1].vx;
    scratch->muzzle.vy = work->laserCapsule.ends[1].vy;
    scratch->muzzle.vz = work->laserCapsule.ends[1].vz;
    gte_ldv0(&scratch->muzzle);
    gte_rtv0();
    gte_stlvnl(&scratch->offset);
    scratch->muzzle.vx = scratch->offset.vx + self->workm.t[0];
    scratch->muzzle.vy = scratch->offset.vy + self->workm.t[1];
    scratch->muzzle.vz = scratch->offset.vz + self->workm.t[2];
    scratch->aim.vx    = work->laserCapsule.ends[0].vx;
    scratch->aim.vy    = work->laserCapsule.ends[0].vy;
    if (worldCollisionFindContactIndex(work->laserContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        scratch->offset.vx = work->laserContacts[0].point.vx - scratch->muzzle.vx;
        scratch->offset.vy = work->laserContacts[0].point.vy - scratch->muzzle.vy;
        scratch->offset.vz = work->laserContacts[0].point.vz - scratch->muzzle.vz;
        scratch->aim.vz    = SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vy * scratch->offset.vy +
                                         scratch->offset.vz * scratch->offset.vz);
        if ((work->laserContacts[0].key.value & 0xFFFF0000) == 0x10000) {
            scratch->aim.vz += 1000;
        }
    } else {
        scratch->aim.vz = 10000;
    }
    worldCollisionClearContacts(work->laserContacts);
    gte_SetRotMatrix(&scratch->matrix);
    gte_ldv0(&scratch->aim);
    gte_rtv0();
    gte_stlvnl(&scratch->offset);
    scratch->aim.vx    = scratch->offset.vx;
    scratch->aim.vy    = scratch->offset.vy;
    scratch->aim.vz    = scratch->offset.vz;
    scratch->muzzle.vx = work->laserCapsule.ends[1].vx;
    scratch->muzzle.vy = work->laserCapsule.ends[1].vy;
    scratch->muzzle.vz = work->laserCapsule.ends[1].vz;
    golemPawnRookDrawLaserBeam(arg0, &scratch->aim, &scratch->muzzle);
    SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookLaserSightScratch);
}
