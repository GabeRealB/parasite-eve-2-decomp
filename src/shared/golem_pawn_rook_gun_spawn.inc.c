/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Spawn state of the model child hung off the actor: parents the child's
/// root coordinate to part 7 of the actor's model, points the child's model
/// at the actor's light and colour matrices and advances to state 1.
void golemPawnRookGunSpawn(Enemy* arg0, Task* task)
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
    coord->parent       = &parentCoords[7];
    obj->lightMtx       = &work->lightMtx;
    obj->flags          = 0;
    obj->colorMtx       = &work->colorMtx;
    task->state         = 1;
}
