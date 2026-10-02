/* Part of the factory lift library; see factory_lift.h. */

/// Runs the factory model for the bit of game flag 0x49 the task last saw: bit
/// 1 picks the first handler pair and bit 0 the second of the pair, the frame
/// counter at `FactoryLiftWork::field_14` is bumped, and the model's coordinate
/// is rebuilt and handed to `func_800D7A9C` together with its translation.
void factoryLiftUpdate(Task* task)
{
    GfxCoord*        coord;
    FactoryLiftWork* work;
    TmdObject*       obj;
    s32              flag;
    s32              prev;

    /* The model pointer is read twice on purpose: the second read is what
       leaves the target's `move s4, v0` copy. */
    coord = task->extra.tmd->coords;
    work  = (FactoryLiftWork*)task->work;
    obj   = task->extra.tmd;
    flag  = GameFlag_GetNibble(GAME_FLAG_FACTORY_LIFT_POSITION);
    prev  = work->field_0;
    if (flag != prev) {
        if ((flag ^ prev) & 1) {
            work->field_16 = 0;
        }
        if ((flag ^ work->field_0) & 2) {
            work->field_17 = 0;
        }
        work->field_0  = flag;
        work->field_14 = 0;
    }
    if (flag & 2) {
        factoryLiftRaise(task);
        if (flag & 1) {
            factoryLiftTurnOut(task);
        } else {
            factoryLiftTurnBack(task);
        }
    } else {
        factoryLiftLower(task);
        if (flag & 1) {
            factoryLiftJamTurnOut(task);
        } else {
            factoryLiftJamTurnBack(task);
        }
    }
    work->field_14++;
    factoryLiftSyncCollision(task, 0, flag & 1);
    Gp_UpdateCoord(coord);
    func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
}
