/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Launches the low backward hop from the landed entry arc.
///
/// Requires `work == task->work` and `root == task->extra.tmd->coords`. Borrows
/// that live storage at the Y > 0 crossing of emerge behavior 4. Restores limb
/// shadows, levels pitch/roll and clamps parent Y to -60; resets clip 12 at
/// normal rate and launches at -110 Y units per update with zero acceleration.
/// Clears the u16 frame counter and advances behavior to 5. Motion narrows to
/// s16; the caller applies animation and rebuilds rotation after this change.
static __inline__ void _madChaserEmergeArcBackLaunchHop(Task* task, MadChaserWork* work, GfxCoord* root)
{
    enum {
        MAD_CHASER_EMERGE_ARC_HOP_CLIP    = 12,
        MAD_CHASER_EMERGE_ARC_LANDING_Y   = -60,
        MAD_CHASER_EMERGE_ARC_HOP_Y_SPEED = -110
    };
    MadChaserWork* requestWork;

    work->shadowHidden       = 0;
    work->stateFrames        = 0;
    root->coord.t[1]         = MAD_CHASER_EMERGE_ARC_LANDING_Y;
    work->rotation.vx        = 0;
    work->rotation.vz        = 0;
    requestWork              = task->work;
    requestWork->animRate    = ANIMATION_RATE_ONE;
    requestWork->animId      = MAD_CHASER_EMERGE_ARC_HOP_CLIP;
    requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    work->moveAccel          = 0;
    work->moveSpeed          = MAD_CHASER_EMERGE_ARC_HOP_Y_SPEED;
    work->state++;
}

/// Finishes the backward entry arc and launches its low backward hop.
///
/// Requires live work/model storage in emerge behavior 4. Moves backward 140
/// parent-coordinate units per updating frame while easing pitch toward one
/// eighth turn; vertical acceleration grows by two each call. Crossing Y > 0
/// restores shadows, resets the counter, levels pitch/roll and places Y at -60.
/// Resets clip 12 at normal rate, launches at -110 Y units per frame and enters
/// behavior 5. Motion fields narrow to s16; the caller ticks animation, rebuilds
/// rotation and resolves collision. All task-owned storage remains live.
static void _madChaserEmergeArcBack(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_ARC_PITCH                   = ACTOR_TRANSFORM_ANGLE_TURN / 8,
        MAD_CHASER_EMERGE_ARC_DIRECTION_EXTRA_BITS    = 4,
        MAD_CHASER_EMERGE_ARC_DIRECTION_FRACTION_BITS = 16
    };
    MadChaserWork* work;
    s16            moveHeading;
    GfxCoord*      root;
    s32            stepDistance;
    s32            directionXQ16;

    work                                  = task->work;
    moveHeading                           = work->rotation.vy;
    root                                  = task->extra.tmd->coords;
    directionXQ16                         = rsin(moveHeading) << MAD_CHASER_EMERGE_ARC_DIRECTION_EXTRA_BITS;
    stepDistance                          = -0x8C;
    task->extra.tmd->coords->coord.t[0]  += (directionXQ16 * stepDistance) >> MAD_CHASER_EMERGE_ARC_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << MAD_CHASER_EMERGE_ARC_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_ARC_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->rotation.vx                    += (MAD_CHASER_EMERGE_ARC_PITCH - work->rotation.vx) >> 5;
    root->coord.t[1]                     += work->moveSpeed;
    work->moveAccel                      += 2;
    work->moveSpeed                      += work->moveAccel;
    if (root->coord.t[1] > 0) {
        _madChaserEmergeArcBackLaunchHop(task, work, root);
    }
}
