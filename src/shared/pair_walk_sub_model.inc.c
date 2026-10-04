/* Part of the pair walk library; see pair_walk.h. */

/// Per-frame callback of the actor's sub-model task, which the spawn handler
/// parents under the actor's own task. On the first frame it draws the
/// sub-model under the actor's `light` / `color` matrices and parents its root
/// coordinate to part 7 of the actor's model, then advances to state 1; from
/// then on it only marks the coordinate for recomputation each frame.
void pairWalkSubModelTask(Task* task)
{
    char          pad[0x10];
    Task*         parent = task->parent;
    TmdObject*    obj    = task->extra.tmd;
    GfxCoord*     coord  = obj->coords;
    GfxCoord*     sub    = &parent->extra.tmd->coords[7];
    PairWalkWork* work   = parent->work;

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
