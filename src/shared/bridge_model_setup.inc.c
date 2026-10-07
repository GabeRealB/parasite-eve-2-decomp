/* Part of the bridge model library; see bridge_model.h. */

/// Sets up the fixed translucent plane shared by the bridge and promenade rooms.
///
/// Requires a live TMD body and root coordinate. Allocates task-owned work on
/// the primary heap, enables drawing and places the root at (-9200, 300, -2800)
/// world units under the view coordinate. Allocation failure kills the task;
/// success advances to its room-specific visibility state. The plane's visual
/// role beyond this geometry is unproven.
static void _bridgeModelSetup(Task* task)
{
    enum { BRIDGE_MODEL_WORLD_X = -9200,
           BRIDGE_MODEL_WORLD_Y = 300,
           BRIDGE_MODEL_WORLD_Z = -2800 };
    TmdObject*       model;
    GfxCoord*        coord;
    BridgeModelWork* work;

    model = task->extra.tmd;
    coord = model->coords;
    work  = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work          = work;
    work->field_0       = 0;
    model->flags        = 0;
    coord->parent       = &gGfxViewCoord;
    coord->coord.t[0]   = BRIDGE_MODEL_WORLD_X;
    coord->coord.t[1]   = BRIDGE_MODEL_WORLD_Y;
    coord->coord.t[2]   = BRIDGE_MODEL_WORLD_Z;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state++;
}
