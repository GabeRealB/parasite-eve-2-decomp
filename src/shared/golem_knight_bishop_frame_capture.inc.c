/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Queues scene refraction capture at a coordinate's projected depth plus bias.
///
/// `coord` is borrowed for this call and composed before projecting its origin.
/// `depthBias` counts ordering-table tags after the quarter-Z result is divided
/// by 16; callers use 12. A negative GTE flag substitutes depth zero before the
/// bias. The biased slot must fit the current ordering table (0..1055 for the
/// normal table); capture does not clamp it. The instance must be bound through
/// `FRAME_CAPTURE_QUEUE`.
static void _golemKnightBishopQueueFrameCapture(GfxCoord* coord, s32 depthBias)
{
    enum {
        GOLEM_KNIGHT_BISHOP_CAPTURE_DEPTH_SHIFT = 4,
    };
    ActorOriginDepthScratch* block;
    SVECTOR*                 origin;

    block            = SCRATCH_STACK_RESERVE_BLOCK(ActorOriginDepthScratch);
    block->origin.vx = 0;
    block->origin.vy = 0;
    block->origin.vz = 0;
    actorRenderComposeCoord(coord);
    origin = &block->origin;
    gte_SetRotMatrix(&coord->workm);
    gte_SetTransMatrix(&coord->workm);
    gte_ldv0(origin);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stdp(&block->depthCue);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> GOLEM_KNIGHT_BISHOP_CAPTURE_DEPTH_SHIFT) + depthBias;
    FRAME_CAPTURE_QUEUE(block->otz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorOriginDepthScratch);
}
