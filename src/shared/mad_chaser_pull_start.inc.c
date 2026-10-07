/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Begins a scripted pull toward joint 3 of the room's placed actor 0.
///
/// Requires live Mad Chaser work/model and that placed actor's coordinate chain.
/// Stops the global alert cry while retaining release, saves the root's local
/// XYZ narrowed to s16, clears frame/motion counters and advances the sub-state.
/// Transforms the joint origin up to the excluded view node, narrowing XYZ at
/// every parent. A parentless chain leaves pullPoint zero. Both actors must use
/// the world frame under the view as their root parent; GTE state is overwritten.
static void _madChaserPullStart(Task* task)
{
    enum { MAD_CHASER_PULL_POINT_PLACED_ACTOR = 0,
           MAD_CHASER_PULL_POINT_JOINT        = 3 };
    MadChaserWork* work;
    GfxCoord*      root;
    SVECTOR*       pullPoint;

    work = task->work;
    root = task->extra.tmd->coords;
    sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    work->moveStartPos.vx = root->coord.t[0];
    work->moveStartPos.vy = root->coord.t[1];
    work->moveStartPos.vz = root->coord.t[2];
    work->stateFrames     = 0;
    work->moveAccel       = 0;
    work->moveSpeed       = 0;
    work->subState++;
    // An incomplete parent chain retains the zero pull point.
    pullPoint     = &work->pullPoint;
    pullPoint->vx = pullPoint->vy = pullPoint->vz = 0;
    _actorRenderTransformLocalPointToWorld(&sceneFindPlacedActor(MAD_CHASER_PULL_POINT_PLACED_ACTOR)->extra.tmd->coords[MAD_CHASER_PULL_POINT_JOINT], pullPoint);
}
