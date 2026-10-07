/* Part of the walker library; see walker.h. */

/* ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW is an object-like binding to a
 * declared static void(Task*) drawer. The call evaluates task once. */
#ifndef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW
#error "Bind ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW before including this fragment"
#endif

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
    worldCoordSetModelLighting(obj, &pos, 0, 3);
    walkerUpdate(task);
    ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW(task);
}
