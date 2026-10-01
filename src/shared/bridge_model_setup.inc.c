/* Part of the bridge model library; see bridge_model.h. */

/// First state of the bridge model task: allocates its one-word work block
/// (killing the task if that fails), parks the model at (-0x23F0, 0x12C, -0xAF0)
/// parented to the room's view coordinate system, and advances the state.
static void bridgeModelSetup(Task* task)
{
    TmdObject*       extra;
    GfxCoord*        coord;
    BridgeModelWork* work;

    extra = task->extra.tmd;
    coord = extra->coords;
    work  = memCalloc(sizeof(BridgeModelWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work          = work;
    work->field_0       = 0;
    extra->flags        = 0;
    coord->parent       = &gGfxViewCoord;
    coord->coord.t[0]   = -0x23F0;
    coord->coord.t[1]   = 0x12C;
    coord->coord.t[2]   = -0xAF0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    task->state++;
}
