/* Part of the model placement library; see model_placement.h. */

/// Setup state of the child task whose state table is
/// `D_actor_113100_80131E30`: hides the child's model, then mirrors the parent's
/// (`spawnArg2`) `TMD_OBJECT_SKIP_ACTIVE_DRAW` and `TMD_OBJECT_SKIP_AUTO_BUFFER`
/// as the tick state does. It draws
/// the model at order-table offset -2, hangs the child's root coordinate off the
/// parent's part `spawnArg1`, shares the parent's light and colour matrices,
/// reparents the task under the parent and steps to the next state.
void modelPlacementAttachChild(Task* task)
{
    Task*      parent;
    TmdObject* obj;
    TmdObject* parentObj;
    GfxCoord*  coords;
    GfxCoord*  root;

    parent      = task->spawnArg2.pointer;
    obj         = task->extra.tmd;
    parentObj   = parent->extra.tmd;
    coords      = parentObj->coords;
    obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    root        = obj->coords;
    if (!(parentObj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        obj->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (!(parentObj->flags & TMD_OBJECT_SKIP_AUTO_BUFFER)) {
        obj->flags &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
        tmdAllocPrimitiveBuffer(obj);
    } else {
        obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    obj->otOffset      = -2;
    coords            += task->spawnArg1.value;
    root->composeStamp = GRAPHICS_COORD_DIRTY;
    root->parent       = coords;
    obj->lightMtx      = parentObj->lightMtx;
    obj->colorMtx      = parentObj->colorMtx;
    taskReparent(parent, task);
    task->state++;
}
