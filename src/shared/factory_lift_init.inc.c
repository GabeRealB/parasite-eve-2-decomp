/* Part of the factory lift library; see factory_lift.h. */

/// Initializes the lift model, its motion work, collision footprint and hatch.
///
/// Requires a spawned TMD root and live factory room/grid resources. Owns a
/// zeroed `FactoryLiftWork` until its exit callback; allocation failure kills
/// the task. Seeds height/yaw from the saved lift-position bits and starts both
/// motion selectors at -1 (rest). Placement is in whole room-coordinate units.
/// Setup applies the raised height but no quarter-turn: the full 16.16 yaw word
/// reduces to zero in `RotMatrixY`, and collision uses the unturned template.
/// The first frame update applies the seeded yaw and its requested footprint.
/// The spawned hatch borrows this lift task and its lighting matrices.
static void _factoryLiftInit(Task* task)
{
    enum { FACTORY_LIFT_ROOM_X            = 3660,
           FACTORY_LIFT_ROOM_Z            = 6830,
           FACTORY_LIFT_SETUP_REST        = -1,
           FACTORY_LIFT_HATCH_SPAWN_INDEX = 7 };
    FactoryLiftWork* work;
    GfxCoord*        coord;
    TmdObject*       liftModel;

    liftModel = task->extra.tmd;
    coord     = liftModel->coords;
    work      = memCalloc(sizeof(FactoryLiftWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = work;
    work->position    = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION);
    work->yawStep     = FACTORY_LIFT_SETUP_REST;
    work->yStep       = FACTORY_LIFT_SETUP_REST;
    liftModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    if (work->position & FACTORY_LIFT_POSITION_TURNED) {
        work->yaw.word = FACTORY_LIFT_YAW_TURNED;
        // The SDK masks this full 16.16 word to zero; retain the setup frame.
        RotMatrixY(FACTORY_LIFT_YAW_TURNED, &coord->coord);
    }
    if (work->position & FACTORY_LIFT_POSITION_RAISED) {
        work->y.word = FACTORY_LIFT_Y_RAISED;
    } else {
        work->y.word = 0;
    }
    coord->coord.t[0] = FACTORY_LIFT_ROOM_X;
    coord->coord.t[1] = work->y.halves.integer;
    coord->coord.t[2] = FACTORY_LIFT_ROOM_Z;
    _factoryLiftBindLighting(task);
    _factoryLiftSyncCollision(task, 1, 0);
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        taskSpawnFromTable(gFactoryDaySpawnTable, FACTORY_LIFT_HATCH_SPAWN_INDEX, 0, task);
    } else {
        taskSpawnFromTable(gFactoryNightSpawnTable, FACTORY_LIFT_HATCH_SPAWN_INDEX, 0, task);
    }
    task->exitCallback  = _factoryLiftExit;
    task->killCountdown = 0;
    task->state++;
}
