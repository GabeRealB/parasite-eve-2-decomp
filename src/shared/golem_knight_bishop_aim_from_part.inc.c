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

/// Updates the collision probe and projected red beam from model part 4.
///
/// `task` owns live GOLEM work and composed model coordinates. A positive aim
/// countdown enables the capsule; drawing starts after its first countdown tick
/// and also runs on the final zero tick. Contact distance is measured in world
/// units and narrowed to s16; a player-body contact extends it by 300 units.
/// The scratch block remains reserved while the beam drawer borrows the results.
static void _golemKnightBishopAimFromPart(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_AIM_PART_X           = -40,
        GOLEM_KNIGHT_BISHOP_AIM_PART_Y           = -120,
        GOLEM_KNIGHT_BISHOP_AIM_PART_Z           = 220,
        GOLEM_KNIGHT_BISHOP_AIM_PLAYER_EXTENSION = 300,
    };
    _GolemKnightBishopAimScratch* scratch;
    GolemKnightBishopWork*        work;
    GfxCoord*                     root;
    GfxCoord*                     beamPart;
    s32                           endpointIndex;
    s16                           contactDistance;

    root                   = task->extra.tmd->coords;
    scratch                = SCRATCH_STACK_RESERVE_BLOCK(_GolemKnightBishopAimScratch);
    work                   = task->work;
    beamPart               = &root[4];
    root->composeStamp     = GRAPHICS_COORD_DIRTY;
    beamPart->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(beamPart);
    // Express the part offset in root space for the live collision capsule.
    if (work->auxTimer > 0) {
        gfxMakeRelativeTransform(&root->workm, &beamPart->workm, &scratch->partInRoot);
        scratch->ends[1].vx = GOLEM_KNIGHT_BISHOP_AIM_PART_X;
        scratch->ends[1].vy = GOLEM_KNIGHT_BISHOP_AIM_PART_Y;
        scratch->ends[1].vz = GOLEM_KNIGHT_BISHOP_AIM_PART_Z;
        gte_SetRotMatrix(&scratch->partInRoot);
        gte_ldv0(&scratch->ends[1]);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        work->aimBeamCapsule.ends[1].vx = scratch->partInRoot.t[0] + scratch->offset.vx;
        work->aimBeamCapsule.ends[1].vy = scratch->partInRoot.t[1] + scratch->offset.vy;
        work->aimBeamCapsule.ends[1].vz = scratch->partInRoot.t[2] + scratch->offset.vz;
        work->aimBeamBody.flags        |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->aimBeamBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    }
    // Consume the previous collision pass, then project both world-space ends.
    if (work->auxTimer < GOLEM_KNIGHT_BISHOP_AIM_TIME - 1) {
        scratch->ends[1].vx = GOLEM_KNIGHT_BISHOP_AIM_PART_X;
        scratch->ends[1].vy = GOLEM_KNIGHT_BISHOP_AIM_PART_Y;
        scratch->ends[1].vz = GOLEM_KNIGHT_BISHOP_AIM_PART_Z;
        gte_SetRotMatrix(&beamPart->workm);
        gte_ldv0(&scratch->ends[1]);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        scratch->ends[1].vx = beamPart->workm.t[0] + scratch->offset.vx;
        scratch->ends[1].vy = beamPart->workm.t[1] + scratch->offset.vy;
        scratch->ends[1].vz = beamPart->workm.t[2] + scratch->offset.vz;
        scratch->ends[0].vx = 0;
        scratch->ends[0].vy = GOLEM_KNIGHT_BISHOP_AIM_ROOT_Y;
        if (worldCollisionFindContactIndex(work->aimBeamContacts, WORLD_COLLISION_FIND_ANY_KEY) != 0) {
            scratch->offset.vx  = work->aimBeamContacts[0].point.vx - scratch->ends[1].vx;
            scratch->offset.vy  = work->aimBeamContacts[0].point.vy - scratch->ends[1].vy;
            scratch->offset.vz  = work->aimBeamContacts[0].point.vz - scratch->ends[1].vz;
            contactDistance     = SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vy * scratch->offset.vy + scratch->offset.vz * scratch->offset.vz);
            scratch->ends[0].vz = contactDistance;
            if ((work->aimBeamContacts[0].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) == WORLD_COLLISION_CONTACT_PLAYER_BODY) {
                scratch->ends[0].vz = contactDistance + GOLEM_KNIGHT_BISHOP_AIM_PLAYER_EXTENSION;
            }
            worldCollisionClearContacts(work->aimBeamContacts);
        } else {
            scratch->ends[0].vz = GOLEM_KNIGHT_BISHOP_AIM_RANGE;
        }
        gte_SetRotMatrix(&root->workm);
        gte_ldv0(&scratch->ends[0]);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        scratch->ends[0].vx = root->workm.t[0] + scratch->offset.vx;
        scratch->ends[0].vy = root->workm.t[1] + scratch->offset.vy;
        scratch->ends[0].vz = root->workm.t[2] + scratch->offset.vz;
        for (endpointIndex = 0; endpointIndex < ARRAY_SIZE(scratch->ends); endpointIndex++) {
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_ldv0(&scratch->ends[endpointIndex]);
            gte_rtps();
            gte_stsxy(&scratch->screenXY);
            gte_stszotz(&scratch->depth);
            work->beamScreenX[endpointIndex] = scratch->screenXY;
            work->beamScreenY[endpointIndex] = scratch->screenXY >> 16;
            work->beamDepth[endpointIndex]   = scratch->depth;
        }
        _golemKnightBishopDrawAimBeam(task);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_GolemKnightBishopAimScratch);
}
