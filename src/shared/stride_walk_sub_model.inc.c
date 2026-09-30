/* Part of the stride walk library; see stride_walk.h. */

/// Per-frame task of this actor's sub-model, with the sub-model's own
/// `TmdObject` in `Task::extra` and the actor as `Task::parent`. The first
/// frame lights the sub-model with the matrix pair at the front of the
/// parent's work block and hangs its coordinate off the parent model's eighth
/// coordinate; every frame marks the coordinate dirty.
void strideWalkSubModelTask(Task* task)
{
    Task*      parent = task->parent;
    TmdObject* obj    = task->extra.tmd;
    GfxCoord*  coord  = obj->coords;
    GfxCoord*  sub    = &parent->extra.tmd->coords[7];
    MATRIX*    work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            obj->lightMtx       = work;
            obj->colorMtx       = work + 1;
            coord->parent       = sub;
            task->state++;
            break;
        case 1:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}
