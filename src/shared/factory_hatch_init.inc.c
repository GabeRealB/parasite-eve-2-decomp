/* Part of the factory lift library; see factory_lift.h. */

/// State 0 of the room's cutscene task: allocates the cutscene work block into
/// `Task::work` and dresses the task's model after the factory model in
/// `Task::spawnArg2`. Both bits of the model's flags follow the factory
/// model's, the root coordinate is seeded with a fixed offset under the
/// factory model's coordinate, and the factory model's light and colour
/// matrices are shared. A nibble of 1 in game flag 0x4E means the scene is
/// already on, which parks the cutscene state at 0xFF and turns the root
/// rotation -0x300 about X. The factory task then adopts this one, which steps
/// on.
void factoryHatchInit(Task* task)
{
    Task*             cap      = task->spawnArg2.pointer;
    TmdObject*        model    = task->extra.tmd;
    TmdObject*        capModel = cap->extra.tmd;
    GfxCoord*         coord    = model->coords;
    GfxCoord*         capCoord = capModel->coords;
    FactoryHatchWork* work     = memCalloc(sizeof(FactoryHatchWork), 0);
    u16               flags;

    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work   = work;
    flags        = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    model->flags = flags;
    if (!(capModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags = flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(capModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        Tmd_AllocBuffers(model);
    } else {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    model->otOffset   = -1;
    coord->coord.t[1] = -0x316;
    coord->parent     = capCoord;
    coord->coord.t[0] = 0;
    coord->coord.t[2] = -0x5FA;
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_HATCH_OPEN) == 1) {
        // Not a handler slot: the dispatcher has no entry for this value.
        work->state = 0xFF;
        RotMatrixX(-0x300, &coord->coord);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx     = capModel->lightMtx;
    model->colorMtx     = capModel->colorMtx;
    taskReparent(cap, task);
    task->state += 1;
}
