/* Part of the factory lift library; see factory_lift.h. */

/// State 0 of the room's factory model: allocate the work block, seed it from
/// the progress nibble, point the model's coordinate at the seeded position
/// and the light/color matrices at the block's own, then pick the spawn table
/// for this session variant and hand the model to its own state machine.
///
/// The two spawn tables are passed straight to `Task_SpawnFromTable` from each
/// arm rather than through a variable: the argument is then a bare symbol, so
/// the `lui`/`addiu` pair is built in `$a0` itself and `jump2` merges the two
/// arms' identical tails back into one call.
void factoryLiftInit(Task* task)
{
    FactoryLiftWork* work;
    GfxCoord*        coord;
    TmdObject*       obj;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(FactoryLiftWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work     = work;
    work->position = gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION);
    work->yawStep  = -1;
    work->yStep    = -1;
    obj->flags    &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    if (work->position & FACTORY_LIFT_POSITION_TURNED) {
        work->yaw.word = FACTORY_LIFT_YAW_TURNED;
        // Passes the whole 16.16 word, where the per-frame handlers pass
        // its integer half.
        RotMatrixY(FACTORY_LIFT_YAW_TURNED, &coord->coord);
    }
    if (work->position & FACTORY_LIFT_POSITION_RAISED) {
        work->y.word = FACTORY_LIFT_Y_RAISED;
    } else {
        work->y.word = 0;
    }
    coord->coord.t[0] = 0xE4C;
    coord->coord.t[1] = work->y.halves.integer;
    coord->coord.t[2] = 0x1AAE;
    factoryLiftBindLighting(task);
    factoryLiftSyncCollision(task, 1, 0);
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        Task_SpawnFromTable(gFactoryDaySpawnTable, 7, 0, task);
    } else {
        Task_SpawnFromTable(gFactoryNightSpawnTable, 7, 0, task);
    }
    task->exitCallback  = factoryLiftExit;
    task->killCountdown = 0;
    task->state++;
}
