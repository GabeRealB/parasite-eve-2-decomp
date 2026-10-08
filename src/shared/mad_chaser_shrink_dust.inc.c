/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Shrinks the ordinary corpse on Y while starting its burn effect.
///
/// Requires live enemy/model/work in ordinary-death behavior 5, with the saved
/// root transform, Q12 Y scale and zeroed stateFrames prepared by death entry.
/// Subtracts 1/64 from the u16 scale each updating frame and rescales a fresh
/// copy of the saved matrix. At count 4 starts three burn bursts at the root;
/// at 16 selects black colour, and after 32 hides the model and advances behavior.
/// Frame comparisons interpret the wrapping u16 counter as s16. Storage stays
/// live; the death dispatcher dirties the root and updates colour afterward.
static void _madChaserShrinkWithBurn(Task* task)
{
    enum {
        MAD_CHASER_DEATH_SHRINK_SCALE_STEP = ONE / 64,
        MAD_CHASER_DEATH_BURN_START_FRAME  = 4,
        MAD_CHASER_DEATH_BURN_BURSTS       = 3,
        MAD_CHASER_DEATH_BLACK_FRAME       = 16,
        MAD_CHASER_DEATH_HIDE_AFTER_FRAME  = 32
    };
    MadChaserWork* work;
    TmdObject*     model;
    GfxCoord*      root;
    VECTOR         scale;
    MATRIX         shrinkMatrix;
    SVECTOR        originOffset;

    work  = task->work;
    model = task->extra.tmd;
    root  = model->coords;
    // Rescale the saved pose so rounding never accumulates across frames.
    work->shrinkScaleY -= MAD_CHASER_DEATH_SHRINK_SCALE_STEP;
    scale.vx            = ONE;
    scale.vy            = (s16)work->shrinkScaleY;
    scale.vz            = ONE;
    root->coord         = work->savedRootMtx;
    gfxSetRotIdentity(&shrinkMatrix);
    ScaleMatrix(&shrinkMatrix, &scale);
    MulMatrix(&root->coord, &shrinkMatrix);
    if ((s16)++work->stateFrames == MAD_CHASER_DEATH_BURN_START_FRAME) {
        originOffset.vx = 0;
        originOffset.vy = 0;
        originOffset.vz = 0;
        effectSpawn(EFFECT_CORPSE_BURN, root, MAD_CHASER_DEATH_BURN_BURSTS, &originOffset);
    }
    if ((s16)work->stateFrames == MAD_CHASER_DEATH_BLACK_FRAME) {
        worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
    }
    if ((s16)work->stateFrames > MAD_CHASER_DEATH_HIDE_AFTER_FRAME) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work->state++;
    }
}
