/* Part of the factory lift library; see factory_lift.h. */

/// Attaches the hatch model to its lift and initializes its swing work.
///
/// `task` must own a live TMD model and borrow the lift task in `spawnArg2`.
/// The hatch inherits the lift's draw flags and lighting, uses a fixed offset
/// in the lift root's frame and joins its teardown tree. Allocation failure
/// kills the hatch; success advances the task to its per-frame state.
/// Zeroed work begins in `FACTORY_HATCH_STATE_WATCH` with the hatch closed.
/// An already-set open flag instead stores 0xFF and turns the model open;
/// that selector has no handler, and this startup path remains unproven.
static void _factoryHatchInit(Task* task)
{
    enum { FACTORY_HATCH_INITIAL_OPEN_ANGLE = -768 };
    Task*             liftTask  = task->spawnArg2.pointer;
    TmdObject*        model     = task->extra.tmd;
    TmdObject*        liftModel = liftTask->extra.tmd;
    GfxCoord*         coord     = model->coords;
    GfxCoord*         liftCoord = liftModel->coords;
    FactoryHatchWork* work      = memCalloc(sizeof(FactoryHatchWork), 0);
    u16               flags;

    if (work == NULL) {
        taskKill(task);
        return;
    }
    // Inherit visibility and primitive-buffer policy from the lift.
    task->work   = work;
    flags        = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    model->flags = flags;
    if (!(liftModel->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        model->flags = flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(liftModel->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(model);
    } else {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    // Keep the hatch's offset and lighting in the lift's coordinate frame.
    model->otOffset   = -1;
    coord->coord.t[1] = -0x316;
    coord->parent     = liftCoord;
    coord->coord.t[0] = 0;
    coord->coord.t[2] = -0x5FA;
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_HATCH_OPEN) == 1) {
        // Not a handler slot: the dispatcher has no entry for this value.
        work->state = 0xFF;
        RotMatrixX(FACTORY_HATCH_INITIAL_OPEN_ANGLE, &coord->coord);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx     = liftModel->lightMtx;
    model->colorMtx     = liftModel->colorMtx;
    taskReparent(liftTask, task);
    task->state += 1;
}
