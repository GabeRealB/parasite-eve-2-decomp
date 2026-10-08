/* Part of the Mad Chaser library; see mad_chaser.h. */

#ifndef SRC_SHARED_MAD_CHASER_CREEP_HELPERS
#define SRC_SHARED_MAD_CHASER_CREEP_HELPERS

/// Starts the creep cue at the model root's already-composed origin.
///
/// Requires a live enemy/model and the origin-audio projection's scratch/GTE
/// setup. Tags character-bank entry 9 with the enemy's four-bit placement index.
/// Borrows task storage; the sound bank must remain loaded through playback.
static __inline__ void _madChaserEmergeCreepPlayEntrySound(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_CREEP_SOUND          = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 9),
        MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT = 8
    };
    Enemy* enemy = task->spawnArg2.pointer;
    s32    soundId;
    s32    audioPan;

    soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT) | MAD_CHASER_EMERGE_CREEP_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

#endif

#ifndef MAD_CHASER_EMERGE_CREEP_HANDLER
/// Selects the static void(Task*) callback defined by this fragment inclusion.
///
/// Defaults to emerge behavior 3; each of the three Mad Chaser carriers binds
/// its second inclusion to behavior 9. Declare the selected identifier static
/// in the carrier's prologue before its dispatch table. The binding is cleared
/// after each inclusion. It evaluates no arguments, captures no runtime values
/// and uses no stringification or token pasting.
#define MAD_CHASER_EMERGE_CREEP_HANDLER _madChaserEmergeCreep3
#endif

/// Creeps forward during emergence until animation status permits combat alert.
///
/// Requires live Mad Chaser work, enemy/model storage and initialized slot 1 in
/// emerge behavior 3 or 9. Moves X/Z along the heading by 20 parent-coordinate
/// units per call, retaining Y; angles use 4096 units per turn. The wrapping
/// u16 counter starts the positional creep cue whenever its increment is one.
/// Audio samples the composed origin before motion and requires the projection
/// scratch/GTE setup. A slot-1 boundary, control jump or held pose enables grid
/// collision and enters combat alert with sub-state zero. The caller ticks
/// animation, rebuilds rotation and applies contacts afterwards. Animation
/// requests, counters and task-owned storage are retained by the transition.
static void MAD_CHASER_EMERGE_CREEP_HANDLER(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_CREEP_DISTANCE          = 20,
        MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS    = 4,
        MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS = 16
    };
    MadChaserWork* work;
    s16            moveHeading;
    s16            stepDistance;

    work = task->work;
    // Sample the composed origin before advancing the local translation.
    if ((s16)++work->stateFrames == 1) {
        _madChaserEmergeCreepPlayEntrySound(task);
    }
    stepDistance                          = MAD_CHASER_EMERGE_CREEP_DISTANCE;
    moveHeading                           = work->rotation.vy;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS) * stepDistance) >> MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Entry motion ignores the grid until the animation allows combat to begin.
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_ALERT);
    }
}

#undef MAD_CHASER_EMERGE_CREEP_HANDLER
