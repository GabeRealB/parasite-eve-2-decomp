/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Levels the landed high flip and selects its final emerge clip.
///
/// Requires `work == task->work` and `root == task->extra.tmd->coords`. Borrows
/// that live storage; the caller applies animation and rebuilds rotation after
/// this phase change.
static __inline__ void _madChaserEmergeFlipOverFinishLanding(Task* task, MadChaserWork* work, GfxCoord* root)
{
    enum {
        MAD_CHASER_EMERGE_FLIP_LANDING_CLIP = 17,
        MAD_CHASER_EMERGE_LANDING_Y         = -60,
        MAD_CHASER_EMERGE_FLIP_HALF_TURN    = ACTOR_TRANSFORM_ANGLE_TURN / 2
    };
    MadChaserWork* requestWork;

    root->coord.t[1]         = MAD_CHASER_EMERGE_LANDING_Y;
    work->rotation.vx        = 0;
    work->rotation.vz        = 0;
    work->rotation.vy       += MAD_CHASER_EMERGE_FLIP_HALF_TURN;
    requestWork              = task->work;
    requestWork->animRate    = ANIMATION_RATE_ONE;
    requestWork->animId      = MAD_CHASER_EMERGE_FLIP_LANDING_CLIP;
    requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    work->stateFrames        = 0;
    work->state++;
}

/// Lands the high entry hop while flipping backward into the final emerge step.
///
/// Requires live work, enemy/model storage in emerge behavior 8 with a cleared
/// frame counter. Eases pitch toward a half turn and starts positional character
/// entries 9 then 3 on signed frame 1. Moves backward 90 parent-coordinate units
/// per updating frame; vertical acceleration grows by four. Crossing Y > 0
/// places Y at -60, levels pitch/roll, adds a half turn to heading, resets clip
/// 17 at normal rate, clears the counter and enters behavior 9. Motion/rotation
/// narrow to s16. Both sounds sample the already-composed origin before motion;
/// origin projection scratch/GTE requirements apply. The caller owns playback,
/// rotation rebuild and collision; the model and work remain live.
static void _madChaserEmergeFlipOver(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_FLIP_HALF_TURN          = ACTOR_TRANSFORM_ANGLE_TURN / 2,
        MAD_CHASER_EMERGE_HOP_SOUND               = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 9),
        MAD_CHASER_EMERGE_FLIP_SOUND              = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 3),
        MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT    = 8,
        MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS    = 4,
        MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS = 16,
    };
    MadChaserWork* work;
    GfxCoord*      root;
    s32            hopSoundId;
    s32            hopAudioPan;
    s32            flipSoundId;
    s32            flipAudioPan;
    s16            moveHeading;
    s16            stepDistance;

    work = task->work;
    root = task->extra.tmd->coords;
    work->stateFrames++;
    work->rotation.vx += (MAD_CHASER_EMERGE_FLIP_HALF_TURN - work->rotation.vx) >> 3;
    if ((s16)work->stateFrames == 1) {
        hopSoundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT) | MAD_CHASER_EMERGE_HOP_SOUND;
        hopAudioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(hopSoundId, hopAudioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        flipSoundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT) | MAD_CHASER_EMERGE_FLIP_SOUND;
        flipAudioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(flipSoundId, flipAudioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    stepDistance                          = -0x5A;
    moveHeading                           = work->rotation.vy;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    root->coord.t[1]                     += work->moveSpeed;
    work->moveAccel                      += 4;
    work->moveSpeed                      += work->moveAccel;
    // Complete the turn before handing control to the final emerge step.
    if (root->coord.t[1] > 0) {
        _madChaserEmergeFlipOverFinishLanding(task, work, root);
    }
}
