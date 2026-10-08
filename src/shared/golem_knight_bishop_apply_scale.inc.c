/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Rebuilds the feint's root transform with its current per-axis scale.
///
/// `task` owns a live GOLEM model and work block. The saved unscaled transform
/// supplies the translation and rotation; `scale` uses 4096 for unity. One
/// scratch-stack matrix is reserved and released, without composing the root.
static void _golemKnightBishopApplyScale(Task* task)
{
    void**                 cursorSlot;
    MATRIX*                scaleMatrix;
    GfxCoord*              root;
    GolemKnightBishopWork* work;

    cursorSlot  = SCRATCH_HEAD_ADDR;
    scaleMatrix = SCRATCH_PUSH_AT(cursorSlot, MATRIX);
    root        = &task->extra.tmd->coords[0];
    work        = task->work;

    root->coord = work->unscaledRootMtx;
    gfxSetRotIdentity(scaleMatrix);
    ScaleMatrix(scaleMatrix, &work->scale);
    MulMatrix(&root->coord, scaleMatrix);
    SCRATCH_POP_AT(cursorSlot, MATRIX);
}
