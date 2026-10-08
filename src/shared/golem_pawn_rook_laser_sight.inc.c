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

/// Updates the launcher's root-space laser probe and draws its contact-limited sight.
///
/// actor is a live Grenade Launcher GOLEM body task. Part 7 supplies the muzzle;
/// the probe's far end uses a fixed aim with angles in 4096 units per turn.
/// A zero burst timer arms the probe without drawing or consuming contacts.
/// Otherwise the previous probe contact limits the drawn reach, with 1000 game
/// units of overshoot for a player or companion body, and its contact is cleared.
/// Borrows scratch storage for the call and leaves the collision body enabled.
static void _golemPawnRookAimLaserSight(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_LASER_MUZZLE_PART      = 7,
        GOLEM_PAWN_ROOK_LASER_MUZZLE_Y         = 100,
        GOLEM_PAWN_ROOK_LASER_MUZZLE_Z         = -100,
        GOLEM_PAWN_ROOK_LASER_AIM_PITCH        = -5,
        GOLEM_PAWN_ROOK_LASER_AIM_YAW          = -5,
        GOLEM_PAWN_ROOK_LASER_FAR_Y            = -1300,
        GOLEM_PAWN_ROOK_LASER_REACH            = 10000,
        GOLEM_PAWN_ROOK_LASER_PLAYER_OVERSHOOT = 1000,
    };
    _GolemPawnRookLaserSightScratch* scratch;
    GolemPawnRookWork*               work;
    GfxCoord*                        bodyCoords;

    scratch                                                    = SCRATCH_STACK_RESERVE_BLOCK(_GolemPawnRookLaserSightScratch);
    bodyCoords                                                 = actor->extra.tmd->coords;
    work                                                       = actor->work;
    bodyCoords[0].composeStamp                                 = GRAPHICS_COORD_DIRTY;
    bodyCoords[GOLEM_PAWN_ROOK_LASER_MUZZLE_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&bodyCoords[GOLEM_PAWN_ROOK_LASER_MUZZLE_PART]);
    // Carry the animated launcher muzzle from part 7 into body-root space.
    gfxMakeRelativeTransform(&bodyCoords->workm, &bodyCoords[GOLEM_PAWN_ROOK_LASER_MUZZLE_PART].workm, &scratch->matrix);
    scratch->muzzle.vy = GOLEM_PAWN_ROOK_LASER_MUZZLE_Y;
    scratch->muzzle.vx = 0;
    scratch->muzzle.vz = GOLEM_PAWN_ROOK_LASER_MUZZLE_Z;
    gte_ApplyMatrix(&scratch->matrix, &scratch->muzzle, &scratch->offset);
    work->laserCapsule.ends[1].vx = scratch->matrix.t[0] + scratch->offset.vx;
    work->laserCapsule.ends[1].vy = scratch->matrix.t[1] + scratch->offset.vy;
    work->laserCapsule.ends[1].vz = scratch->matrix.t[2] + scratch->offset.vz;
    scratch->aim.vx               = GOLEM_PAWN_ROOK_LASER_AIM_PITCH;
    scratch->aim.vy               = GOLEM_PAWN_ROOK_LASER_AIM_YAW;
    scratch->aim.vz               = 0;
    RotMatrix(&scratch->aim, &scratch->matrix);
    scratch->aim.vx = 0;
    scratch->aim.vy = GOLEM_PAWN_ROOK_LASER_FAR_Y;
    scratch->aim.vz = GOLEM_PAWN_ROOK_LASER_REACH;
    gte_ApplyMatrix(&scratch->matrix, &scratch->aim, &scratch->offset);
    work->laserCapsule.ends[0].vx = scratch->offset.vx;
    work->laserCapsule.ends[0].vy = scratch->offset.vy;
    work->laserCapsule.ends[0].vz = scratch->offset.vz;
    work->laserBody.flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    if (work->timer == 0) {
        SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookLaserSightScratch);
        return;
    }
    // Measure the previous probe contact in the root matrix's composition frame.
    gte_SetRotMatrix(&bodyCoords->workm);
    scratch->muzzle.vx = work->laserCapsule.ends[1].vx;
    scratch->muzzle.vy = work->laserCapsule.ends[1].vy;
    scratch->muzzle.vz = work->laserCapsule.ends[1].vz;
    gte_ldv0(&scratch->muzzle);
    gte_rtv0();
    gte_stlvnl(&scratch->offset);
    scratch->muzzle.vx = scratch->offset.vx + bodyCoords->workm.t[0];
    scratch->muzzle.vy = scratch->offset.vy + bodyCoords->workm.t[1];
    scratch->muzzle.vz = scratch->offset.vz + bodyCoords->workm.t[2];
    scratch->aim.vx    = work->laserCapsule.ends[0].vx;
    scratch->aim.vy    = work->laserCapsule.ends[0].vy;
    if (worldCollisionFindContactIndex(work->laserContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
        scratch->offset.vx = work->laserContacts[0].point.vx - scratch->muzzle.vx;
        scratch->offset.vy = work->laserContacts[0].point.vy - scratch->muzzle.vy;
        scratch->offset.vz = work->laserContacts[0].point.vz - scratch->muzzle.vz;
        scratch->aim.vz    = SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vy * scratch->offset.vy +
                                         scratch->offset.vz * scratch->offset.vz);
        if ((work->laserContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
            scratch->aim.vz += GOLEM_PAWN_ROOK_LASER_PLAYER_OVERSHOOT;
        }
    } else {
        scratch->aim.vz = GOLEM_PAWN_ROOK_LASER_REACH;
    }
    // Retain the probe's rotated X/Y, replace Z with reach, then rotate again for drawing.
    worldCollisionClearContacts(work->laserContacts);
    gte_ApplyMatrix(&scratch->matrix, &scratch->aim, &scratch->offset);
    scratch->aim.vx    = scratch->offset.vx;
    scratch->aim.vy    = scratch->offset.vy;
    scratch->aim.vz    = scratch->offset.vz;
    scratch->muzzle.vx = work->laserCapsule.ends[1].vx;
    scratch->muzzle.vy = work->laserCapsule.ends[1].vy;
    scratch->muzzle.vz = work->laserCapsule.ends[1].vz;
    _golemPawnRookDrawLaserBeam(actor, &scratch->aim, &scratch->muzzle);
    SCRATCH_STACK_RELEASE_BLOCK(_GolemPawnRookLaserSightScratch);
}
