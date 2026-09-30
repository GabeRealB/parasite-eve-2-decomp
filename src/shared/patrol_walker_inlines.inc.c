/* Part of the library; see patrol_walker.h. Inline helpers the fragments use. */

/// The walker's per-tick body, open on the scratch frame `patrolWalkerTick`
/// hands it. State 1 heads straight for the player matrix's translation, using
/// `field_6E` as the one-based player selector; state 2 re-runs patrol steering and
/// re-reads `nav`'s byte table at `cursor` whenever the step or one of the
/// three node bytes changed, and state 3 follows the patrol route proper. The
/// scalar at `field_5E` then ramps towards `field_5C` by `field_60` a frame;
/// while it is non-zero it scales (`GPF`) the normalised facing column of the
/// model matrix into the per-frame world step, which is added to the
/// coordinate's translation and kept in `moveStep`. `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen` (a global
/// freeze flag) zeroes the step instead. Written as an inline so the two
/// scratch-head accesses inside one frame stay absolute; see
/// `func_acropolis_bridge_8018532C` in `acropolis_bridge_12.c`, the same body.
static __inline__ void patrolWalkerStep(OverlayWalker* walker, u8* head,
                                        OverlayWalkerTickScratch* block)
{
    u8*           head2;
    SVECTOR3*     pos;
    PlayerStatus* cfg;
    SVECTOR*      sv;
    SVECTOR*      gsv;
    SVECTOR*      step;
    GfxCoord*     coord;
    s16           sdiff;
    s32           diff;
    s16           speed;
    s32           cur;
    s32           target;
    s32           result;

    switch (walker->state) {
        case 0:
            break;
        case 1:
            cfg                            = &gPlayerStatus + (walker->field_6E - 1);
            pos                            = (SVECTOR3*)(head - 0x24);
            ((SVECTOR3*)(head - 0x24))->vx = (u16)cfg->coordMtx->t[0];
            pos->vy                        = (u16)cfg->coordMtx->t[1];
            pos->vz                        = (u16)cfg->coordMtx->t[2];
            break;
        case 2:
            SCRATCH_STACK_RESERVE_BYTES(4);
            walker->field_6F = patrolNodeNearestActor(walker, 1);
            walker->field_70 = patrolNodeNearestSelf(walker);
            if (walker->field_69 != walker->state || walker->field_70 != walker->field_72 ||
                walker->field_6F != walker->field_71) {
                patrolPlanToward(walker, 1);
                walker->node = walker->nav->field_4[walker->cursor];
            }
            walker->field_69 = walker->state;
            walker->field_72 = walker->field_70;
            walker->field_71 = walker->field_6F;
            if (patrolArrived(walker) != 0) {
                walker->cursor += (u8)walker->field_73;
                walker->node    = walker->nav->field_4[walker->cursor];
                SCRATCH_STACK_RELEASE_BYTES(4);
            }
            break;
        case 3:
            patrolFollowRoute(walker, (SVECTOR3*)(head - 0x24));
            break;
    }
    patrolTurnToward(walker, &block->pos);

    cur    = walker->field_5C;
    target = walker->field_5E;
    if (cur != target) {
        diff  = cur - target;
        sdiff = diff;
        if (sdiff > walker->field_60) {
            result = target + walker->field_60;
        } else if (sdiff < -walker->field_60) {
            result = target - walker->field_60;
        } else {
            result = target + diff;
        }
        walker->field_5E = result;
    }

    coord = walker->coord;
    speed = walker->field_5E;
    step  = &walker->moveStep;
    if (gMcSaveData[0].state.actorsFrozen == 1) {
        step->vz            = 0;
        step->vy            = 0;
        walker->moveStep.vx = 0;
    } else {
        head2                    = SCRATCH_STACK_CURSOR(u8);
        sv                       = (SVECTOR*)(head2 - 8);
        SCRATCH_STACK_CURSOR(u8) = (u8*)sv;
        gsv                      = sv;
        if (speed != 0) {
            Gfx_MatrixCol2(&coord->coord, sv);
            VectorNormalSS(sv, sv);
            gte_lddp(speed);
            gte_ldsv(gsv);
            gte_gpf12();
            gte_stsv(gsv);
            coord->coord.t[0]  += ((SVECTOR*)(head2 - 8))->vx;
            coord->coord.t[1]  += sv->vy;
            coord->coord.t[2]  += sv->vz;
            walker->moveStep    = *(SVECTOR*)(head2 - 8);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
        }
        SCRATCH_STACK_RELEASE_BYTES(8);
    }
    if (walker->field_6C == 0) {
        patrolApplyGroundStep(walker);
    }
    if (walker->field_6D == 0) {
        patrolAvoidContacts(walker);
    }
}
