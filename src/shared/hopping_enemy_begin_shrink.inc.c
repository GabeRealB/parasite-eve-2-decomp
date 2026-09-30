/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Starts the death shrink, the same body as `hopperBeginDeath`:
/// detaches the records, unlinks the three hit bodies, sets the Y scale to
/// 1.0, saves the root matrix, sets light mode 1 and advances the state.
void hopperBeginShrink(Task* task)
{
    Enemy*           enemy = (Enemy*)task->spawnArg2.pointer;
    Actor341700Work* work  = (Actor341700Work*)task->work;
    GfxCoord*        coord = task->extra.tmd->coords;
    Actor341700Work* objWork;

    enemy->recs = 0;

    objWork = (Actor341700Work*)task->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430    = 0x1000;
    work->savedRootMtx = coord->coord;

    Gp_SetLightMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);

    work->field_412 = 0;
    work->field_420++;
}
