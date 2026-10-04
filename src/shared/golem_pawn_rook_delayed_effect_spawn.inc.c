/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Spawn state of the child task driven by `Actor02300_Fn03BA8`: parents the
/// child's root coordinate to part 7 of the enemy's model, points the child's
/// model at the enemy's light and colour matrices and seeds the enemy's
/// `swordTrailDelay` countdown the child's tick drains, then advances to state 1.
/// `arg0` is the spawn context every state handler takes and is unused here.
void golemPawnRookDelayedEffectSpawn(Enemy* arg0, Task* task)
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

    coord->composeStamp   = GRAPHICS_COORD_DIRTY;
    coord->parent         = &parentCoords[7];
    obj->lightMtx         = &work->lightMtx;
    obj->colorMtx         = &work->colorMtx;
    obj->flags            = 0;
    task->state           = 1;
    work->swordTrailDelay = 0xA;
}
