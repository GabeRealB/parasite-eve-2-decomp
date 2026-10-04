/* Part of the walker library; see walker.h. */

/// Per-frame state of a walker: refreshes the model root, relights the model
/// from a point 0x320 above it, runs the walker's update and draws its shadow.
static void walkerFrame(Enemy* enemy, Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     pos;

    obj   = task->extra.tmd;
    coord = obj->coords;
    actorRenderComposeCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    walkerUpdate(task);
    walkerDrawShadow(task);
}
