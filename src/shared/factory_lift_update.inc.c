/* Part of the factory lift library; see factory_lift.h. */

/// Advances the lift's independent height and yaw movements and room collision.
///
/// Requires initialized lift work/model and the room-owned panel-task slot in
/// `spawnArg2.pointer`. Changed position bits restart their motion selectors
/// and frame counter. Lowered turns jam and roll back their request. Collision
/// selects its template from the request read before those handlers, so the
/// completion/skip frame retains the canceled footprint with restored yaw;
/// the next update selects the restored footprint. Composes the root and samples
/// room lighting after both movements and collision have been applied.
static void _factoryLiftUpdate(Task* task)
{
    GfxCoord*        coord;
    FactoryLiftWork* work;
    TmdObject*       liftModel;
    s32              requestedPosition;
    s32              previousPosition;

    coord             = task->extra.tmd->coords;
    work              = task->work;
    liftModel         = task->extra.tmd;
    requestedPosition = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION);
    previousPosition  = work->position;
    if (requestedPosition != previousPosition) {
        if ((requestedPosition ^ previousPosition) & FACTORY_LIFT_POSITION_TURNED) {
            work->yawStep = FACTORY_LIFT_STEP_RESET;
        }
        if ((requestedPosition ^ work->position) & FACTORY_LIFT_POSITION_RAISED) {
            work->yStep = FACTORY_LIFT_STEP_RESET;
        }
        work->position   = requestedPosition;
        work->moveFrames = 0;
    }
    if (requestedPosition & FACTORY_LIFT_POSITION_RAISED) {
        _factoryLiftRaise(task);
        if (requestedPosition & FACTORY_LIFT_POSITION_TURNED) {
            _factoryLiftTurnOut(task);
        } else {
            _factoryLiftTurnBack(task);
        }
    } else {
        _factoryLiftLower(task);
        if (requestedPosition & FACTORY_LIFT_POSITION_TURNED) {
            _factoryLiftJamTurnOut(task);
        } else {
            _factoryLiftJamTurnBack(task);
        }
    }
    work->moveFrames++;
    // Use this frame's original request even if a jam handler just rolled it back.
    _factoryLiftSyncCollision(task, 0, requestedPosition & FACTORY_LIFT_POSITION_TURNED);
    actorRenderComposeCoord(coord);
    worldCoordSetModelLighting(liftModel, coord->workm.t, 0, 3);
}
