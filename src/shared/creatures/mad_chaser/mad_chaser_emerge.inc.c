/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Launches the low backward hop from the landed entry arc.
///
/// Requires `work == task->work` and `root == task->extra.tmd->coords`. Borrows
/// that live storage at the Y > 0 crossing of emerge behavior 4. Restores limb
/// shadows, levels pitch/roll and clamps parent Y to -60; resets clip 12 at
/// normal rate and launches at -110 Y units per update with zero acceleration.
/// Clears the u16 frame counter and advances behavior to 5. Motion narrows to
/// s16; the caller applies animation and rebuilds rotation after this change.
static inline void _madChaserEmergeArcBackLaunchHop(Task* task, MadChaserWork* work, GfxCoord* root)
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

/// Starts the entry-hop cue at the root's already-composed origin.
///
/// Requires a live Enemy/model; tags character-bank entry 9 with placement
/// index 0..15 in sound-id bits 8..15. The cached root-to-view transform supplies
/// pan (-16..15) and attenuation depth (-128..127), both narrowed to s8 before
/// the request. Requires projection scratch/GTE setup; retains no storage.
static inline void _madChaserEmergeHopBackPlayEntrySound(Task* task)
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

/// Starts the entry-hop cue at the root's already-composed origin.
///
/// Requires a live Enemy/model; tags character-bank entry 9 with placement
/// index 0..15 in sound-id bits 8..15. The cached root-to-view transform supplies
/// pan (-16..15) and attenuation depth (-128..127), both narrowed to s8 before
/// the request. Requires projection scratch/GTE setup; retains no storage.
static inline void _madChaserEmergeBackOffPlayEntrySound(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_BACK_OFF_SOUND       = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 9),
        MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT = 8
    };
    Enemy* enemy = task->spawnArg2.pointer;
    s32    soundId;
    s32    audioPan;

    soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT) | MAD_CHASER_EMERGE_BACK_OFF_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Backs away until the recovery clip reports a boundary, jump or held pose.
///
/// Requires live work, enemy/model storage in emerge behavior 6 with initialized
/// animation slots and a cleared frame counter. Starts positional character-bank
/// entry 9 on signed frame 1 and moves backward 20 parent-coordinate units per
/// updating frame. Slot 1's status re-enables room-grid tests and enters combat
/// walk (task state 3, behavior 3, sub-state 0). The intermediate task-entry reset
/// is retained. Audio samples the already-composed origin before motion and
/// requires origin projection scratch/GTE setup; the caller ticks animation,
/// rebuilds rotation and resolves collision. Storage and frame count stay live.
static void _madChaserEmergeBackOff(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS    = 4,
        MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS = 16
    };
    MadChaserWork* work;
    s16            moveHeading;
    s16            stepDistance;

    work = task->work;
    if ((s16)++work->stateFrames == 1) {
        _madChaserEmergeBackOffPlayEntrySound(task);
    }
    stepDistance                          = -0x14;
    moveHeading                           = work->rotation.vy;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Restore grid collision only after the recovery clip reaches its boundary.
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
    }
}

/// Launches the high flip-over hop while retaining the landed arc rotation.
///
/// Requires `work == task->work` and `root == task->extra.tmd->coords`. Borrows
/// that live storage at the Y > 0 crossing of emerge behavior 7. Restores limb
/// shadows and clamps parent Y to -60 while retaining pitch/heading/roll.
/// Resets clip 12 at normal rate and launches at -300 Y units per update with
/// zero acceleration. Clears the u16 frame counter and enters behavior 8;
/// the caller applies animation and rebuilds rotation after this change.
static inline void _madChaserEmergeHighArcLaunchFlip(Task* task, MadChaserWork* work, GfxCoord* root)
{
    enum {
        MAD_CHASER_EMERGE_HIGH_ARC_FLIP_CLIP    = 12,
        MAD_CHASER_EMERGE_LANDING_Y             = -60,
        MAD_CHASER_EMERGE_HIGH_ARC_FLIP_Y_SPEED = -300
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
    work->moveSpeed          = MAD_CHASER_EMERGE_HIGH_ARC_FLIP_Y_SPEED;
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

/// Levels the landed high flip and selects its final emerge clip.
///
/// Requires `work == task->work` and `root == task->extra.tmd->coords`. Borrows
/// that live storage at the Y > 0 crossing of emerge behavior 8. Clamps parent
/// Y to -60, levels pitch/roll and adds a half turn to the s16 heading. Resets
/// clip 17 at normal rate, clears the u16 frame counter and enters behavior 9.
/// Retains motion and shadow state; the caller applies animation and rebuilds
/// rotation after this change.
static inline void _madChaserEmergeFlipOverFinishLanding(Task* task, MadChaserWork* work, GfxCoord* root)
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
