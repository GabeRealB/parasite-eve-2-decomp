/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Projects the origin of `arg0` to find its ordering-table depth, adds
/// `arg1`, and queues the frame-buffer pass `frameCaptureQueue` there
/// (at depth `arg1` when the projection fails).
void golemKnightBishopQueueFrameCapture(GfxCoord* arg0, s32 arg1)
{
    u8*                      head;
    ActorOriginDepthScratch* block;
    SVECTOR*                 vec;

    head                                          = SCRATCH_STACK_CURSOR(u8);
    block                                         = (ActorOriginDepthScratch*)(head - sizeof(ActorOriginDepthScratch));
    SCRATCH_STACK_CURSOR(ActorOriginDepthScratch) = block;
    block->origin.vx                              = 0;
    block->origin.vy                              = 0;
    block->origin.vz                              = 0;
    actorRenderComposeCoord(arg0);
    vec = &block->origin;
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stdp(&block->depthCue);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + arg1;
    frameCaptureQueue(block->otz);
    SCRATCH_STACK_RELEASE_BLOCK(ActorOriginDepthScratch);
}
