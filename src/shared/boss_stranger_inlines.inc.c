/* Part of the library; see boss_stranger.h. Inline helpers the fragments use. */

/// The walker's per-tick body, open on the scratch frame `bossStrangerTick`
/// hands it. State 1 heads straight for the player matrix's translation, using
/// `playerId` as the one-based player selector; state 2 walks `nav`'s
/// `nodeOrder` and re-plans whenever the state or one of the node bytes
/// changed, and state 3 follows the patrol route. `speed` then ramps towards
/// `speedTarget` by at most `speedStep` a frame; while it is non-zero it
/// scales (`GPF`) the normalised facing column of the model matrix into the
/// per-frame world step, which is added to the coordinate's translation and
/// kept in `moveStep`. `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen`
/// zeroes the step instead. Written as an inline so the two scratch-head
/// accesses inside one frame stay absolute; see
/// `func_acropolis_bridge_8018532C` in `acropolis_bridge_12.c`, the same body.
static __inline__ void bossStrangerStep(BossStrangerWalker* walker, u8* head,
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
        case BOSS_STRANGER_WALKER_IDLE:
            break;
        case BOSS_STRANGER_WALKER_CHASE:
            cfg                            = &gPlayerStatus + (walker->playerId - 1);
            pos                            = (SVECTOR3*)(head - 0x24);
            ((SVECTOR3*)(head - 0x24))->vx = (u16)cfg->coordMtx->t[0];
            pos->vy                        = (u16)cfg->coordMtx->t[1];
            pos->vz                        = (u16)cfg->coordMtx->t[2];
            break;
        case BOSS_STRANGER_WALKER_CLOSE:
            SCRATCH_STACK_RESERVE_BYTES(4);
            walker->actorNode = bossStrangerNodeNearestActor(walker, 1);
            walker->selfNode  = bossStrangerNodeNearestSelf(walker);
            if (walker->prevState != walker->state || walker->selfNode != walker->prevSelfNode ||
                walker->actorNode != walker->prevActorNode) {
                bossStrangerPlanToward(walker, 1);
                walker->node = walker->nav->nodeOrder[walker->cursor];
            }
            walker->prevState     = walker->state;
            walker->prevSelfNode  = walker->selfNode;
            walker->prevActorNode = walker->actorNode;
            if (bossStrangerArrived(walker) != 0) {
                walker->cursor += (u8)walker->orderStep;
                walker->node    = walker->nav->nodeOrder[walker->cursor];
                SCRATCH_STACK_RELEASE_BYTES(4);
            }
            break;
        case BOSS_STRANGER_WALKER_PATROL:
            bossStrangerFollowRoute(walker, (SVECTOR3*)(head - 0x24));
            break;
    }
    bossStrangerTurnToward(walker, &block->pos);

    cur    = walker->speedTarget;
    target = walker->speed;
    if (cur != target) {
        diff  = cur - target;
        sdiff = diff;
        if (sdiff > walker->speedStep) {
            result = target + walker->speedStep;
        } else if (sdiff < -walker->speedStep) {
            result = target - walker->speedStep;
        } else {
            result = target + diff;
        }
        walker->speed = result;
    }

    coord = walker->coord;
    speed = walker->speed;
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
            gfxReadMatrixZAxis(&coord->coord, sv);
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
    if (walker->skipGround == 0) {
        bossStrangerApplyGroundStep(walker);
    }
    if (walker->skipAvoid == 0) {
        bossStrangerAvoidContacts(walker);
    }
}
