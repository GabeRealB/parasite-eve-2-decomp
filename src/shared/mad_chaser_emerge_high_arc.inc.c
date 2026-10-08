/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Launches the high flip-over hop while retaining the landed arc rotation.
///
/// Requires `work == task->work` and `root == task->extra.tmd->coords`. Borrows
/// that live storage; the caller applies animation and rebuilds rotation after
/// this phase change.
static __inline__ void _madChaserEmergeHighArcLaunchFlip(Task* task, MadChaserWork* work, GfxCoord* root)
{
    enum {
        MAD_CHASER_EMERGE_HIGH_ARC_FLIP_CLIP = 12,
        MAD_CHASER_EMERGE_LANDING_Y          = -60
    };
    MadChaserWork* requestWork;

    work->shadowHidden       = 0;
    work->stateFrames        = 0;
    root->coord.t[1]         = MAD_CHASER_EMERGE_LANDING_Y;
    requestWork              = task->work;
    requestWork->animRate    = ANIMATION_RATE_ONE;
    requestWork->animId      = MAD_CHASER_EMERGE_HIGH_ARC_FLIP_CLIP;
    requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    work->moveAccel          = 0;
    work->moveSpeed          = -0x12C;
    work->state++;
}

/// Finishes the backward entry arc and launches the high flip-over path.
///
/// Requires live work/model storage in emerge behavior 7. Moves backward 140
/// parent-coordinate units per updating frame while easing pitch toward one
/// eighth turn; vertical acceleration grows by two each call. Crossing Y > 0
/// restores shadows, clears the counter and places Y at -60, retaining pitch
/// and roll. Resets clip 12 at normal rate, launches at -300 Y units per frame
/// and enters behavior 8. Motion fields narrow to s16; the caller ticks
/// animation, rebuilds rotation and resolves collision.
static void _madChaserEmergeHighArc(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_HIGH_ARC_PITCH          = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS    = 4,
        MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS = 16,
    };
    MadChaserWork* work;
    s16            moveHeading;
    GfxCoord*      root;
    s32            stepDistance;
    s32            directionXQ16;

    work                                  = task->work;
    moveHeading                           = work->rotation.vy;
    root                                  = task->extra.tmd->coords;
    directionXQ16                         = rsin(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS;
    stepDistance                          = -0x8C;
    task->extra.tmd->coords->coord.t[0]  += (directionXQ16 * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->rotation.vx                    += (MAD_CHASER_EMERGE_HIGH_ARC_PITCH - work->rotation.vx) >> 5;
    root->coord.t[1]                     += work->moveSpeed;
    work->moveAccel                      += 2;
    work->moveSpeed                      += work->moveAccel;
    // Landing launches the next move rather than ending emergence.
    if (root->coord.t[1] > 0) {
        _madChaserEmergeHighArcLaunchFlip(task, work, root);
    }
}
