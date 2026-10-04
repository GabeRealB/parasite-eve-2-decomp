/* Part of the stride walk library; see stride_walk.h. */

/// Per-frame state of the walker: relights the model from a point 0x320 above
/// its root, runs the animation update, then turns the head toward the player
/// by the work block's `turnWeight`, which rises 0x200 a frame to
/// `STRIDE_WALK_TURN_WEIGHT_FULL` while `turnMode` is `STRIDE_WALK_TURN_PLAYER`
/// and falls 0x200 a frame to 0 otherwise, and draws the shadow.
void strideWalkFrame(Enemy* enemy, Task* task)
{
    StrideWalkWork* work;
    GfxCoord*       coord;
    TmdObject*      obj;
    VECTOR          vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = task->work;
    actorRenderComposeCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    strideWalkUpdate(task);
    if (work->turnMode == STRIDE_WALK_TURN_PLAYER) {
        work->turnWeight += 0x200;
        if (work->turnWeight > STRIDE_WALK_TURN_WEIGHT_FULL) {
            work->turnWeight = STRIDE_WALK_TURN_WEIGHT_FULL;
        }
    } else {
        work->turnWeight -= 0x200;
        if (work->turnWeight < 0) {
            work->turnWeight = 0;
        }
    }
    func_800B0928(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x200, 0x100, work->turnWeight);
    walkerDrawShadow(task);
}
