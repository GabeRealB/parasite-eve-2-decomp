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
    work  = memCalloc(0x58, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work     = work;
    work->field_0  = GameFlag_GetNibble(0x49);
    work->field_16 = -1;
    work->field_17 = -1;
    obj->flags    &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    if (work->field_0 & 1) {
        work->field_10.word = 0x4000000;
        RotMatrixY(0x4000000, &coord->coord);
    }
    if (work->field_0 & 2) {
        work->field_C.word = 0xFDC60000;
    } else {
        work->field_C.word = 0;
    }
    coord->coord.t[0] = 0xE4C;
    coord->coord.t[1] = work->field_C.halves.integer;
    coord->coord.t[2] = 0x1AAE;
    factoryLiftBindLighting(task);
    factoryLiftSyncCollision(task, 1, 0);
    if (gGameSession->location.loc.stage == 2) {
        Task_SpawnFromTable(gFactoryDaySpawnTable, 7, 0, task);
    } else {
        Task_SpawnFromTable(gFactoryNightSpawnTable, 7, 0, task);
    }
    task->exitCallback  = factoryLiftExit;
    task->killCountdown = 0;
    task->state++;
}
