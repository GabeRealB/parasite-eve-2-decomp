/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the death shrink, the same body as `madChaserBeginDeath`:
/// detaches the records, unlinks the three hit bodies, sets the Y scale to
/// 1.0, saves the root matrix, sets light mode 1 and advances the state.
void madChaserBeginShrink(Task* task)
{
    Enemy*         enemy = (Enemy*)task->spawnArg2.pointer;
    MadChaserWork* work  = (MadChaserWork*)task->work;
    GfxCoord*      coord = task->extra.tmd->coords;
    MadChaserWork* objWork;

    enemy->recs = 0;

    objWork = (MadChaserWork*)task->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430    = 0x1000;
    work->savedRootMtx = coord->coord;

    Gp_SetLightMode(task->spawnArg2.pointer, ENEMY_COLOR_WEIGHTED);

    work->field_412 = 0;
    work->field_420++;
}
