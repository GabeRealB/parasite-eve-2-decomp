/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the entry-hop cue at the root's already-composed origin.
///
/// Requires a live Enemy/model; tags character-bank entry 9 with placement
/// index 0..15 in sound-id bits 8..15. The cached root-to-view transform supplies
/// pan (-16..15) and attenuation depth (-128..127), both narrowed to s8 before
/// the request. Requires projection scratch/GTE setup; retains no storage.
static __inline__ void _madChaserEmergeHopForwardPlayEntrySound(Task* task)
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

/// Lands the forward recovery hop after the entry backflip.
///
/// Requires live work, enemy/model storage in emerge behavior 2 with a cleared
/// frame counter. Starts positional character-bank entry 9 on signed frame 1,
/// then moves forward 80 parent-coordinate units each updating frame. Vertical
/// acceleration grows by four; speed/acceleration narrow to s16. Crossing Y > 0
/// places Y at -60, clears the counter and enters creep behavior 3. Audio reads
/// the root's already-composed origin before movement and requires the origin
/// projection scratch/GTE setup. Playback, rotation and collision belong to
/// the caller; all task-owned storage remains live.
static void _madChaserEmergeHopForward(Task* task)
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
    if ((s16)++work->stateFrames == 1) {
        _madChaserEmergeHopForwardPlayEntrySound(task);
    }
    stepDistance                          = 0x50;
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
