/* Part of the Mad Chaser library; see mad_chaser.h. */

/* MAD_CHASER_EMERGE_CREEP_HANDLER binds the definition's void(Task*) identifier.
 * Each carrier declares its behavior-9 instance static before inclusion.
 * The fragment clears the binding afterwards; with no binding, it retains
 * the behavior-3 entry.
 * This object-like binding captures no values and uses no # or ## operations.
 */
#ifndef MAD_CHASER_EMERGE_CREEP_HANDLER
#define MAD_CHASER_EMERGE_CREEP_HANDLER madChaserCreepUntilHit
#endif

/// Creeps out of the entry animation and hands control to combat's alert behavior.
///
/// Requires live Mad Chaser work, enemy/model storage and initialized slot 1.
/// Advances X/Z along the heading by 20 parent-coordinate units per update;
/// angles use 4096ths of a turn. Starts character-bank entry 9, tagged with the
/// enemy's 0..15 placement index, when the incremented 16-bit frame counter is
/// one. Audio samples the already-composed origin before motion and requires
/// the origin projection's scratch/GTE setup. A slot-1 boundary, control jump
/// or held pose enables grid collision and enters combat alert with sub-state
/// zero. The caller owns animation ticking, rotation rebuild and collision;
/// this handler retains the frame counter and all task-owned storage.
void MAD_CHASER_EMERGE_CREEP_HANDLER(Task* task)
{
    enum {
        MAD_CHASER_EMERGE_CREEP_DISTANCE          = 20,
        MAD_CHASER_EMERGE_CREEP_SOUND             = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 9),
        MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT    = 8,
        MAD_CHASER_EMERGE_DIRECTION_EXTRA_BITS    = 4,
        MAD_CHASER_EMERGE_DIRECTION_FRACTION_BITS = 16
    };
    MadChaserWork* work;
    Enemy*         enemy;
    s32            soundId;
    s32            audioPan;
    s16            moveHeading;
    s16            stepDistance;

    work = task->work;
    // Sample the composed origin before advancing the local translation.
    if ((s16)++work->stateFrames == 1) {
        enemy    = task->spawnArg2.pointer;
        soundId  = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_EMERGE_SOUND_INSTANCE_SHIFT) | MAD_CHASER_EMERGE_CREEP_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
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
