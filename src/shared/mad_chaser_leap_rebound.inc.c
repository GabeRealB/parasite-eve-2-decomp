/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Moves the leap rebound back along its locked heading until landing.
///
/// Requires live work/model storage and the launch height in moveStartPos.vy.
/// Moves 200 parent-coordinate units per callback along leapHeading; Y and the
/// grid sphere follow moveSpeed, with acceleration growing by 14 each callback.
/// Both motion fields narrow to s16. At the saved height, clamps root Y, clears
/// the grid's local Y offset and frame counter, requests clip 19 blended over
/// two normal-rate frames and advances to rebound landing. The caller ticks it.
static void _madChaserLeapRebound(Task* task)
{
    enum {
        MAD_CHASER_REBOUND_STEP_DISTANCE     = 200,
        MAD_CHASER_REBOUND_ACCEL_INCREMENT   = 14,
        MAD_CHASER_REBOUND_LAND_BLEND_FRAMES = 2,
        MAD_CHASER_REBOUND_LAND_CLIP         = 19,
    };
    MadChaserWork* work;
    s16            reboundHeading;
    GfxCoord*      root;
    MadChaserWork* requestWork;
    s32            stepDistance;
    s32            scaledSine;

    work                                  = task->work;
    reboundHeading                        = work->leapHeading;
    root                                  = task->extra.tmd->coords;
    scaledSine                            = rsin(reboundHeading) << 4;
    stepDistance                          = MAD_CHASER_REBOUND_STEP_DISTANCE;
    task->extra.tmd->coords->coord.t[0]  += (scaledSine * stepDistance) >> 16;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(reboundHeading) << 4) * stepDistance) >> 16;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Move the collision sphere with the vertical arc before testing landing.
    root->coord.t[1]      += work->moveSpeed;
    work->gridBody.pos.vy += work->moveSpeed;
    work->moveAccel       += MAD_CHASER_REBOUND_ACCEL_INCREMENT;
    work->moveSpeed       += work->moveAccel;
    if (root->coord.t[1] >= work->moveStartPos.vy) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_REBOUND_LAND_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_REBOUND_LAND_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        root->coord.t[1]             = work->moveStartPos.vy;
        work->gridBody.pos.vy        = 0;
        work->stateFrames            = 0;
        work->subState++;
    }
}
