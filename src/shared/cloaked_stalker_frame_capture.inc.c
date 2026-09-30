/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Projects the origin of `arg0` to find its ordering-table depth, adds
/// `arg1`, and queues the frame-buffer pass `frameCaptureQueue` there
/// (at depth `arg1` when the projection fails).
void stalkerQueueFrameCapture(GfxCoord* arg0, s32 arg1)
{
    u8*                  head;
    ActorProjectScratch* block;
    SVECTOR*             vec;

    head                                      = SCRATCH_STACK_CURSOR(u8);
    block                                     = (ActorProjectScratch*)(head - sizeof(ActorProjectScratch));
    SCRATCH_STACK_CURSOR(ActorProjectScratch) = block;
    block->vec.vx                             = 0;
    block->vec.vy                             = 0;
    block->vec.vz                             = 0;
    Gp_UpdateCoord(arg0);
    vec = &block->vec;
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&block->sxy);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + arg1;
    frameCaptureQueue(block->otz);
    SCRATCH_STACK_RELEASE_BYTES(0x18);
}
