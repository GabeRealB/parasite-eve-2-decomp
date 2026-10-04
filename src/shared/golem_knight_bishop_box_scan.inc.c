/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Sequence 0xB, the region scan. In state 0 it walks the `field_6FA` regions at
/// `field_6B4`: a `GOLEM_KNIGHT_BISHOP_REGION_CIRCLE` whose `param.radius` holds
/// the player's planar offset from its centre (`x`, `z`) moves to state 1 and parks the
/// target position 0x5AA behind the player, enabling grid tests through
/// `field_5BA` and `field_5DA`; a `GOLEM_KNIGHT_BISHOP_REGION_BOX` holding the
/// player starts sequence 3 with `field_70E` at 3 and its index in `field_708`. State 1 enters sequence 1
/// (and `field_70E` 1) unless the first record is occupied, clears the target
/// grid enables and the record, and drops back to state 0.
void golemKnightBishopBoxScanSeq(Task* arg0)
{
    u8*                          head;
    GolemKnightBishopBoxScratch* sc;
    GolemKnightBishopWork*       work;
    GfxCoord*                    coord;
    s32                          i;

    head                     = SCRATCH_STACK_CURSOR(u8);
    work                     = arg0->work;
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(GolemKnightBishopBoxScratch);
    sc                       = (GolemKnightBishopBoxScratch*)(head - sizeof(GolemKnightBishopBoxScratch));
    switch (work->field_6CE) {
        case 0:
            for (i = 0; i < work->field_6FA; i++) {
                switch (work->field_6B4[i].kind) {
                    case GOLEM_KNIGHT_BISHOP_REGION_CIRCLE:
                        sc->out.vx = work->field_6B4[i].x - gPlayerStatus.coordMtx->t[0];
                        sc->out.vz = work->field_6B4[i].z - gPlayerStatus.coordMtx->t[2];
                        if (SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < work->field_6B4[i].param.radius) {
                            work->field_6CE = 1;
                            coord           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                            work->field_6E6 = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                            sc->in.vx       = 0;
                            sc->in.vy       = 0;
                            sc->in.vz       = -0x5AA;
                            gte_SetRotMatrix(&coord->coord);
                            gte_ldv0(&sc->in);
                            gte_rtv0();
                            gte_stlvnl(&sc->out);
                            work->field_6A4 = gPlayerStatus.coordMtx->t[0] + sc->out.vx;
                            work->field_6A8 = gPlayerStatus.coordMtx->t[1];
                            SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopBoxScratch));
                            work->field_6AC  = gPlayerStatus.coordMtx->t[2] + sc->out.vz;
                            work->field_5BA |= WORLD_COLLISION_BODY_GRID_ENABLED;
                            work->field_5DA |= WORLD_COLLISION_BODY_GRID_ENABLED;
                            return;
                        }
                        break;
                    case GOLEM_KNIGHT_BISHOP_REGION_BOX:
                        if (work->field_6B4[i].minX < gPlayerStatus.coordMtx->t[0] &&
                            gPlayerStatus.coordMtx->t[0] < work->field_6B4[i].maxX &&
                            gPlayerStatus.coordMtx->t[2] < work->field_6B4[i].maxZ &&
                            work->field_6B4[i].minZ < gPlayerStatus.coordMtx->t[2]) {
                            work->field_6CC = 3;
                            work->field_6CE = 0;
                            work->field_70E = 3;
                            work->field_708 = i;
                            SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopBoxScratch));
                            return;
                        }
                        break;
                }
            }
            break;
        case 1:
            if (work->field_5F4.key.value == 0) {
                work->field_6CC = 1;
                work->field_70E = 1;
            }
            work->field_6CE  = 0;
            work->field_5BA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            work->field_5DA &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            Gp_ClearRec18Occupied(&work->field_5F4);
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopBoxScratch));
}
