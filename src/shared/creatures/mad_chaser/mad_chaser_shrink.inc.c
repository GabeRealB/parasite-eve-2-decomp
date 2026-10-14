/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Restores the saved root pose and applies this frame's smaller local Y scale.
///
/// Borrows live work/root belonging to the same enemy. Subtracts 1/64 from the
/// wrapping u16 Q12 scale, then interprets it as s16 for the SDK multiplication.
/// Copies the complete saved matrix before scaling its basis, retaining the
/// saved translation. The caller dirties coordinate composition.
static __inline__ void _madChaserScaleShrinkRoot(MadChaserWork* work, GfxCoord* root)
{
    enum { MAD_CHASER_SHRINK_SCALE_STEP = ONE / 64 };
    VECTOR scale;
    MATRIX shrinkMatrix;

    work->shrinkScaleY -= MAD_CHASER_SHRINK_SCALE_STEP;
    scale.vx            = ONE;
    scale.vy            = (s16)work->shrinkScaleY;
    scale.vz            = ONE;
    root->coord         = work->savedRootMtx;
    gfxSetRotIdentity(&shrinkMatrix);
    ScaleMatrix(&shrinkMatrix, &scale);
    MulMatrix(&root->coord, &shrinkMatrix);
}

/// Squashes the scripted-death body vertically, then hides it for timed despawn.
///
/// Requires live task-owned work/model in shrink-death behavior 5, a saved root
/// matrix, Q12 shrinkScaleY initialized to 1.0 and stateFrames reset to zero.
/// Subtracts 1/64 from the wrapping u16 scale each updating frame, interpreting
/// it as s16 for scaling. Turns black on frame 16 and hides on frame 33, clears
/// the counter and enters behavior 6. The counter comparisons also use s16.
/// The caller updates colour and dirties composition; all storage remains live.
static void _madChaserShrink(Task* task)
{
    enum {
        MAD_CHASER_SHRINK_BLACK_FRAME      = 16,
        MAD_CHASER_SHRINK_HIDE_AFTER_FRAME = 32
    };
    MadChaserWork* work;
    TmdObject*     model;
    GfxCoord*      root;

    work  = task->work;
    model = task->extra.tmd;
    root  = model->coords;
    // Rescale the saved pose so rounding never accumulates across frames.
    _madChaserScaleShrinkRoot(work, root);
    if ((s16)++work->stateFrames == MAD_CHASER_SHRINK_BLACK_FRAME) {
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if ((s16)work->stateFrames > MAD_CHASER_SHRINK_HIDE_AFTER_FRAME) {
        model->flags     |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->stateFrames = 0;
        work->state++;
    }
}
