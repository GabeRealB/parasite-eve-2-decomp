/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Records the horizontal range and relative bearing of the nearer player actor.
///
/// Requires live Mad Chaser work/model and any present player models in the
/// same root-parent frame. Always snapshots the full-width root XYZ for collision
/// rollback. If the primary player exists, narrows each candidate XYZ offset to
/// s16 game units before measuring X/Z distance; ties keep the primary player.
/// Stores that offset and distance (narrowed to s16) and a 0..4095 bearing
/// relative to the work heading. Uses SDK normalization/GTE state; horizontal
/// squared sums must fit signed 32 bits. An absent primary leaves prior tracking
/// values intact even when the companion exists. Does not turn or move the model.
static void _madChaserTrackPlayer(Task* task)
{
    /// Saves signed-halfword XYZ and narrows a horizontal distance to s16.
    ///
    /// workBlock is a live MadChaserWork pointer, offsetVector is an SVECTOR
    /// lvalue, and horizontalDistance is in game units. The first two arguments
    /// are evaluated repeatedly and must be stable, disjoint and free of side
    /// effects; the distance is evaluated once. Captures no caller identifiers.
    /// Confined to this function, before normalization overwrites the offset.
#define MAD_CHASER_SAVE_PLAYER_TRACKING(workBlock, offsetVector, horizontalDistance) \
    do {                                                                             \
        (workBlock)->toPlayer.vx = (offsetVector).vx;                                \
        (workBlock)->toPlayer.vy = (offsetVector).vy;                                \
        (workBlock)->toPlayer.vz = (offsetVector).vz;                                \
        (workBlock)->playerDist  = (horizontalDistance);                             \
    } while (0)

    MadChaserWork* work;
    GfxCoord*      root;
    GfxCoord*      playerRoot;
    Task*          playerTask;
    SVECTOR        nearestOffset;
    SVECTOR        companionOffset;
    s32            nearestDistance;
    s32            companionDistance;

    work                 = task->work;
    root                 = task->extra.tmd->coords;
    playerTask           = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    work->prevRootPos.vx = root->coord.t[0];
    work->prevRootPos.vy = root->coord.t[1];
    work->prevRootPos.vz = root->coord.t[2];
    if (playerTask != NULL) {
        playerRoot       = playerTask->extra.tmd->coords;
        nearestOffset.vx = playerRoot->coord.t[0] - root->coord.t[0];
        nearestOffset.vy = playerRoot->coord.t[1] - root->coord.t[1];
        nearestOffset.vz = playerRoot->coord.t[2] - root->coord.t[2];
        nearestDistance  = SquareRoot0(nearestOffset.vx * nearestOffset.vx + nearestOffset.vz * nearestOffset.vz);
        if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] != NULL) {
            playerRoot         = gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]->extra.tmd->coords;
            companionOffset.vx = playerRoot->coord.t[0] - root->coord.t[0];
            companionOffset.vy = playerRoot->coord.t[1] - root->coord.t[1];
            companionOffset.vz = playerRoot->coord.t[2] - root->coord.t[2];
            companionDistance  = SquareRoot0(companionOffset.vx * companionOffset.vx + companionOffset.vz * companionOffset.vz);
            if (companionDistance < nearestDistance) {
                nearestDistance  = companionDistance;
                nearestOffset.vx = companionOffset.vx;
                nearestOffset.vy = companionOffset.vy;
                nearestOffset.vz = companionOffset.vz;
            }
        }
        // Save the displacement before normalization changes it to a direction.
        MAD_CHASER_SAVE_PLAYER_TRACKING(work, nearestOffset, nearestDistance);
        VectorNormalSS(&nearestOffset, &nearestOffset);
        work->playerBearing = (ratan2(nearestOffset.vx, nearestOffset.vz) - work->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
    }
#undef MAD_CHASER_SAVE_PLAYER_TRACKING
}
