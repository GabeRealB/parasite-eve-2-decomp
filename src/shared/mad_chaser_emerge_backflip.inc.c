/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Levels the landed backflip, reverses heading and launches the forward hop.
///
/// Requires `work == task->work` and `root == task->extra.tmd->coords`. Borrows
/// that live storage; the caller applies animation and rebuilds rotation after
/// this phase change.
static __inline__ void _madChaserEmergeBackflipLaunchHop(Task* task, MadChaserWork* work, GfxCoord* root)
{
    enum {
        MAD_CHASER_EMERGE_BACKFLIP_HOP_CLIP  = 17,
        MAD_CHASER_EMERGE_LANDING_Y          = -60,
        MAD_CHASER_EMERGE_BACKFLIP_HALF_TURN = ACTOR_TRANSFORM_ANGLE_TURN / 2
    };
    MadChaserWork* requestWork;

    work->shadowHidden       = 0;
    work->stateFrames        = 0;
    root->coord.t[1]         = MAD_CHASER_EMERGE_LANDING_Y;
    work->rotation.vx        = 0;
    work->rotation.vz        = 0;
    work->rotation.vy       += MAD_CHASER_EMERGE_BACKFLIP_HALF_TURN;
    requestWork              = task->work;
    requestWork->animRate    = ANIMATION_RATE_ONE;
    requestWork->animId      = MAD_CHASER_EMERGE_BACKFLIP_HOP_CLIP;
    requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    work->moveAccel          = 0;
    work->moveSpeed          = -0x6E;
    work->state++;
}

/// Finishes the backward entry flip and launches a forward recovery hop.
///
/// Requires live work/model storage in emerge behavior 1. Moves backward 140
/// parent-coordinate units per updating frame while easing pitch toward a half
/// turn; vertical acceleration grows by two each call. Crossing Y > 0 restores
/// shadows, clears the counter, places Y at -60, levels pitch/roll and adds a
/// half turn to heading. Resets clip 17 at normal rate, launches at -110 Y units
/// per frame and enters behavior 2. Motion and rotation narrow to s16; the caller
/// ticks animation, rebuilds rotation and resolves collision.
static void _madChaserEmergeBackflip(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_BACKFLIP_HALF_TURN      = ACTOR_TRANSFORM_ANGLE_TURN / 2,
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
    work->rotation.vx                    += (MAD_CHASER_EMERGE_BACKFLIP_HALF_TURN - work->rotation.vx) >> 3;
    root->coord.t[1]                     += work->moveSpeed;
    work->moveAccel                      += 2;
    work->moveSpeed                      += work->moveAccel;
    // Landing launches the next move rather than ending emergence.
    if (root->coord.t[1] > 0) {
        _madChaserEmergeBackflipLaunchHop(task, work, root);
    }
}
