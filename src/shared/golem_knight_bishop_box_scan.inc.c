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

/// The region scan the golem starts in. Step 0 walks the `regionCount`
/// entries at `regions`. A `GOLEM_KNIGHT_BISHOP_REGION_CIRCLE` holding the
/// player places `targetPos` 0x5AA behind them, grid-enables both probe
/// bodies and goes to step 1; a `GOLEM_KNIGHT_BISHOP_REGION_BOX` holding the
/// player starts the box approach with its index in `boxRegion`. Step 1
/// starts the grab unless `probeContacts` reports the spot blocked, switches
/// the probes off again and returns to step 0.
void golemKnightBishopBoxScanSeq(Task* arg0)
{
    u8*                                  head;
    _GolemKnightBishopRegionScanScratch* sc;
    GolemKnightBishopWork*               work;
    GfxCoord*                            coord;
    s32                                  i;

    head                     = SCRATCH_STACK_CURSOR(u8);
    work                     = arg0->work;
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(_GolemKnightBishopRegionScanScratch);
    sc                       = (_GolemKnightBishopRegionScanScratch*)(head - sizeof(_GolemKnightBishopRegionScanScratch));
    switch (work->step) {
        case 0:
            for (i = 0; i < work->regionCount; i++) {
                switch (work->regions[i].kind) {
                    case GOLEM_KNIGHT_BISHOP_REGION_CIRCLE:
                        sc->offset.vx = work->regions[i].x - gPlayerStatus.coordMtx->t[0];
                        sc->offset.vz = work->regions[i].z - gPlayerStatus.coordMtx->t[2];
                        if (SquareRoot0(sc->offset.vx * sc->offset.vx + sc->offset.vz * sc->offset.vz) < work->regions[i].param.radius) {
                            work->step         = 1;
                            coord              = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                            work->targetYaw    = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                            sc->localOffset.vx = 0;
                            sc->localOffset.vy = 0;
                            sc->localOffset.vz = -0x5AA;
                            gte_SetRotMatrix(&coord->coord);
                            gte_ldv0(&sc->localOffset);
                            gte_rtv0();
                            gte_stlvnl(&sc->offset);
                            work->targetPos.vx = gPlayerStatus.coordMtx->t[0] + sc->offset.vx;
                            work->targetPos.vy = gPlayerStatus.coordMtx->t[1];
                            SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopRegionScanScratch));
                            work->targetPos.vz         = gPlayerStatus.coordMtx->t[2] + sc->offset.vz;
                            work->pathProbeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                            work->spotProbeBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
                            return;
                        }
                        break;
                    case GOLEM_KNIGHT_BISHOP_REGION_BOX:
                        if (work->regions[i].minX < gPlayerStatus.coordMtx->t[0] &&
                            gPlayerStatus.coordMtx->t[0] < work->regions[i].maxX &&
                            gPlayerStatus.coordMtx->t[2] < work->regions[i].maxZ &&
                            work->regions[i].minZ < gPlayerStatus.coordMtx->t[2]) {
                            work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
                            work->step       = 0;
                            work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_BOX_APPROACH;
                            work->boxRegion  = i;
                            SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopRegionScanScratch));
                            return;
                        }
                        break;
                }
            }
            break;
        case 1:
            if (work->probeContacts[0].key.value == 0) {
                work->sequence   = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
                work->lastAttack = GOLEM_KNIGHT_BISHOP_SEQUENCE_GRAB;
            }
            work->step                 = 0;
            work->pathProbeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            work->spotProbeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            Gp_ClearRec18Occupied(work->probeContacts);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(_GolemKnightBishopRegionScanScratch));
}
