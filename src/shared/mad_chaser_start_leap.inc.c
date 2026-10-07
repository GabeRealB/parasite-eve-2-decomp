/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the leap windup and saves the height to which it will land.
///
/// Requires live work and root coordinates in the world frame under the view.
/// Saves root Y narrowed to s16, blends clip 8 over eight normal-rate frames,
/// clears the frame counter/acceleration and seeds vertical speed at -300 parent
/// units per callback. Marks hasLeaped, clears busy/anchored and advances the
/// sub-state. Animation playback and the subsequent motion belong to the caller.
static void _madChaserStartLeap(Task* task)
{
    enum {
        MAD_CHASER_LEAP_WINDUP_CLIP            = 8,
        MAD_CHASER_LEAP_WINDUP_BLEND_FRAMES    = MAD_CHASER_LEAP_WINDUP_CLIP,
        MAD_CHASER_LEAP_INITIAL_VERTICAL_SPEED = -300,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s16            requestValue;

    work                  = task->work;
    work->moveStartPos.vy = task->extra.tmd->coords->coord.t[1];
    requestWork           = task->work;
    // Reuse the request value to retain the initializer's emitted store sequence.
    requestValue                 = MAD_CHASER_LEAP_WINDUP_BLEND_FRAMES;
    requestWork->animBlendFrames = requestValue;
    requestWork->animId          = requestValue;
    requestWork->animRate        = ANIMATION_RATE_ONE;
    requestValue                 = MAD_CHASER_ANIM_REQUEST_BLEND;
    requestWork->animRequest     = requestValue;
    work->stateFrames            = 0;
    work->moveAccel              = 0;
    work->moveSpeed              = MAD_CHASER_LEAP_INITIAL_VERTICAL_SPEED;
    work->hasLeaped              = requestValue;
    work->busy                   = 0;
    work->anchored               = 0;
    work->subState               = work->subState + 1;
}
