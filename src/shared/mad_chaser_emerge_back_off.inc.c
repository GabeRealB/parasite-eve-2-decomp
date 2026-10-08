/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the entry-hop cue at the root's already-composed origin.
///
/// Requires a live Enemy/model; tags character-bank entry 9 with placement
/// index 0..15 in sound-id bits 8..15. The cached root-to-view transform supplies
/// pan (-16..15) and attenuation depth (-128..127), both narrowed to s8 before
/// the request. Requires projection scratch/GTE setup; retains no storage.
static __inline__ void _madChaserEmergeBackOffPlayEntrySound(Task* task)
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
