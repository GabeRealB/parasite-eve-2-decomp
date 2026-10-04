/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Spawn state of the child task driven by `Actor02300_Fn03CE8`: parents the
/// child's root coordinate to part 11 of the enemy's model, points the child's
/// model at the enemy's light and colour matrices and advances to state 1.
/// `arg0` is the spawn context every state handler takes and is unused here.
void golemPawnRookBurstPartSpawn(Enemy* arg0, Task* task)
{
    Task*              parent;
    TmdObject*         obj;
    GolemPawnRookWork* work;
    GfxCoord*          coord;
    GfxCoord*          parentCoords;

    parent       = task->parent;
    obj          = task->extra.tmd;
    parentCoords = parent->extra.tmd->coords;
    coord        = obj->coords;
    work         = parent->work;

    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->parent       = &parentCoords[11];
    obj->lightMtx       = &work->lightMtx;
    obj->flags          = 0;
    obj->colorMtx       = &work->colorMtx;
    task->state         = 1;
}
