/* Part of the stride walk library; see stride_walk.h. */

/// Per-tick state 1 of this actor: faces the model toward the `gameGetPtrSlot(3)`
/// task. The root coordinate of the model is updated, a copy of its translation
/// lifted by 0x320 is used as the look-at point, and the work block's
/// `turnWeight` rate is stepped +0x200 or -0x200 per tick depending on
/// `turnUp`, clamped to 0x1000 and 0 respectively.
void strideWalkFrame(Enemy* enemy, Task* task)
{
    Actor161500Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;
    VECTOR           vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    work  = (Actor161500Work*)task->work;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    strideWalkUpdate(task);
    if (work->turnUp == 1) {
        work->turnWeight += 0x200;
        if (work->turnWeight > 0x1000) {
            work->turnWeight = 0x1000;
        }
    } else {
        work->turnWeight -= 0x200;
        if (work->turnWeight < 0) {
            work->turnWeight = 0;
        }
    }
    func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, work->turnWeight);
    walkerDrawShadow(task);
}
