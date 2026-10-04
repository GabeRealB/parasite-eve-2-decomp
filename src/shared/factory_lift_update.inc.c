/* Part of the factory lift library; see factory_lift.h. */

/// Runs the factory model for the bit of game flag 0x49 the task last saw: bit
/// 1 picks the first handler pair and bit 0 the second of the pair, the frame
/// counter at `FactoryLiftWork::moveFrames` is bumped, and the model's coordinate
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
    work  = task->work;
    obj   = task->extra.tmd;
    flag  = GameFlag_GetNibble(GAME_FLAG_FACTORY_LIFT_POSITION);
    prev  = work->position;
    if (flag != prev) {
        if ((flag ^ prev) & FACTORY_LIFT_POSITION_TURNED) {
            work->yawStep = 0;
        }
        if ((flag ^ work->position) & FACTORY_LIFT_POSITION_RAISED) {
            work->yStep = 0;
        }
        work->position   = flag;
        work->moveFrames = 0;
    }
    if (flag & FACTORY_LIFT_POSITION_RAISED) {
        factoryLiftRaise(task);
        if (flag & FACTORY_LIFT_POSITION_TURNED) {
            factoryLiftTurnOut(task);
        } else {
            factoryLiftTurnBack(task);
        }
    } else {
        factoryLiftLower(task);
        if (flag & FACTORY_LIFT_POSITION_TURNED) {
            factoryLiftJamTurnOut(task);
        } else {
            factoryLiftJamTurnBack(task);
        }
    }
    work->moveFrames++;
    factoryLiftSyncCollision(task, 0, flag & FACTORY_LIFT_POSITION_TURNED);
    actorRenderComposeCoord(coord);
    func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
}
