/* Part of the stride walk library; see stride_walk.h. */

/// Per-frame task of this actor's sub-model, with the sub-model's own
/// `TmdObject` in `Task::extra` and the actor as `Task::parent`. The first
/// frame lights the sub-model with the parent work block's `light` and `color`
/// and hangs its coordinate off the parent model's eighth coordinate; every
/// frame marks the coordinate dirty.
void strideWalkSubModelTask(Task* task)
{
    Task*           parent = task->parent;
    TmdObject*      obj    = task->extra.tmd;
    GfxCoord*       coord  = obj->coords;
    GfxCoord*       sub    = &parent->extra.tmd->coords[7];
    StrideWalkWork* work   = parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = &work->light;
            obj->colorMtx       = &work->color;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
