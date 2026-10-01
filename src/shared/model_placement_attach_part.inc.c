/* Part of the model placement library; see model_placement.h. */

/// Spawn state of the child in the first state table: chains this task's root
/// coordinate under the parent's part named by `spawnArg1`, takes the parent
/// model's light and colour matrices, reparents the task under the spawner
/// named by `spawnArg2` and advances to the next state.
void modelPlacementAttachPart(Task* task)
{
    Task*      parent;
    s32        part;
    TmdObject* extra;
    TmdObject* parentExtra;
    GfxCoord*  coord;
    GfxCoord*  dest;

    parent              = (Task*)task->spawnArg2.pointer;
    part                = task->spawnArg1.value;
    extra               = task->extra.tmd;
    parentExtra         = parent->extra.tmd;
    coord               = extra->coords;
    dest                = &parentExtra->coords[part];
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = dest;
    extra->lightMtx     = parentExtra->lightMtx;
    extra->colorMtx     = parentExtra->colorMtx;
    taskReparent(parent, task);
    task->state += 1;
}
