/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Scratch-stack block of the region scan's circle test.
///
/// Reserved for the length of the call. A box region is tested without it.
typedef struct {
    VECTOR  offset;         // the circle's centre less the player's position, x and z only; then the spot behind the player as an offset along the world axes
    byte    field_10[0x10]; // never accessed; role unproven
    SVECTOR localOffset;    // that spot in the player's frame: 0x5AA behind them
} _GolemKnightBishopRegionScanScratch;
STATIC_ASSERT_SIZEOF(_GolemKnightBishopRegionScanScratch, 0x28);

/// Waits for the player to enter a room region and starts its attack.
///
/// `task` owns a live GOLEM work block and a borrowed `regionCount`-entry table.
/// Circles propose a grab behind the player, then wait one collision pass to
/// check the probes; boxes immediately select their post approach. Circle
/// radii and box edges are exclusive, in world units. Reserves 40 scratch bytes.
static void _golemKnightBishopRegionScanSeq(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_REGION_SCAN_SEARCH   = 0,
        GOLEM_KNIGHT_BISHOP_REGION_SCAN_PROBE    = 1,
        GOLEM_KNIGHT_BISHOP_REGION_SCAN_YAW_MASK = 0xFFF,
    };
    _GolemKnightBishopRegionScanScratch* scratch;
    GolemKnightBishopWork*               work;
    GfxCoord*                            playerRoot;
    s32                                  regionIndex;

    work    = task->work;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GolemKnightBishopRegionScanScratch);
    switch (work->step) {
        case GOLEM_KNIGHT_BISHOP_REGION_SCAN_SEARCH:
            for (regionIndex = 0; regionIndex < work->regionCount; regionIndex++) {
                switch (work->regions[regionIndex].kind) {
                    case GOLEM_KNIGHT_BISHOP_REGION_CIRCLE:
                        scratch->offset.vx = work->regions[regionIndex].x - gPlayerStatus.coordMtx->t[0];
                        scratch->offset.vz = work->regions[regionIndex].z - gPlayerStatus.coordMtx->t[2];
                        if (SquareRoot0(scratch->offset.vx * scratch->offset.vx + scratch->offset.vz * scratch->offset.vz) < work->regions[regionIndex].param.radius) {
                            work->step              = GOLEM_KNIGHT_BISHOP_REGION_SCAN_PROBE;
                            playerRoot              = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                            work->targetYaw         = ratan2(playerRoot->coord.m[0][2], playerRoot->coord.m[2][2]) & GOLEM_KNIGHT_BISHOP_REGION_SCAN_YAW_MASK;
                            scratch->localOffset.vx = 0;
                            scratch->localOffset.vy = 0;
                            scratch->localOffset.vz = -GOLEM_KNIGHT_BISHOP_GRAB_TARGET_DISTANCE;
                            gte_SetRotMatrix(&playerRoot->coord);
                            gte_ldv0(&scratch->localOffset);
                            gte_rtv0();
                            gte_stlvnl(&scratch->offset);
                            work->targetPos.vx = gPlayerStatus.coordMtx->t[0] + scratch->offset.vx;
                            work->targetPos.vy = gPlayerStatus.coordMtx->t[1];
                            SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopRegionScanScratch));
                            // The retained order reads Z after release, before any nested reservation.
                            work->targetPos.vz         = gPlayerStatus.coordMtx->t[2] + scratch->offset.vz;
                            work->pathProbeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                            work->spotProbeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                            return;
                        }
                        break;
                    case GOLEM_KNIGHT_BISHOP_REGION_BOX:
                        if (GOLEM_KNIGHT_BISHOP_PLAYER_INSIDE_BOX(&work->regions[regionIndex], gPlayerStatus.coordMtx)) {
                            work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
                            work->step       = 0;
                            work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
                            work->boxRegion  = regionIndex;
                            SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopRegionScanScratch));
                            return;
                        }
                        break;
                }
            }
            break;
        case GOLEM_KNIGHT_BISHOP_REGION_SCAN_PROBE:
            // The probes now contain the collision pass's answer for the proposed grab.
            if (work->probeContacts[0].key.value == 0) {
                work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
                work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
            }
            work->step                 = 0;
            work->pathProbeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            work->spotProbeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            worldCollisionClearContacts(work->probeContacts);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopRegionScanScratch));
}
