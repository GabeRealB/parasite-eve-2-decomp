/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the entry-hop cue at the root's already-composed origin.
///
/// Requires a live Enemy/model; tags character-bank entry 9 with placement
/// index 0..15 in sound-id bits 8..15. The cached root-to-view transform supplies
/// pan (-16..15) and attenuation depth (-128..127), both narrowed to s8 before
/// the request. Requires projection scratch/GTE setup; retains no storage.
static __inline__ void _madChaserEmergeHopBackPlayEntrySound(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_HOP_SOUND            = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 9),
        MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT = 8
    };
    Enemy* enemy = task->spawnArg2.pointer;
    s32    soundId;
    s32    audioPan;

    soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT) | MAD_CHASER_EMERGE_HOP_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Lands the low backward recovery hop after the entry arc.
///
/// Requires live work, enemy/model storage in emerge behavior 5 with a cleared
/// frame counter. Starts positional character-bank entry 9 on signed frame 1,
/// moves backward 80 parent-coordinate units per updating frame and eases pitch
/// toward zero. Vertical acceleration grows by four; motion fields narrow to
/// s16. Crossing Y > 0 places Y at -60, clears the counter and enters back-off
/// behavior 6. Audio reads the already-composed root origin before movement and
/// requires origin projection scratch/GTE setup. The caller ticks animation,
/// rebuilds rotation and resolves collision; task-owned storage stays live.
static void _madChaserEmergeHopBack(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS    = 4,
        MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS = 16,
        MAD_CHASER_EMERGE_LANDING_Y               = -60
    };
    MadChaserWork* work;
    GfxCoord*      root;
    s16            moveHeading;
    s16            stepDistance;

    work = task->work;
    root = task->extra.tmd->coords;
    work->stateFrames++;
    work->rotation.vx += -work->rotation.vx >> 5;
    if ((s16)work->stateFrames == 1) {
        _madChaserEmergeHopBackPlayEntrySound(task);
    }
    stepDistance                          = -0x50;
    moveHeading                           = work->rotation.vy;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    root->coord.t[1]                     += work->moveSpeed;
    work->moveAccel                      += 4;
    work->moveSpeed                      += work->moveAccel;
    if (root->coord.t[1] > 0) {
        root->coord.t[1]  = MAD_CHASER_EMERGE_LANDING_Y;
        work->stateFrames = 0;
        work->state++;
    }
}
