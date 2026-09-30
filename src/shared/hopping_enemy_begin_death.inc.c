/* Part of the hopping enemy library; see hopping_enemy.h. */

/// First step of the death squash: drops the enemy's contact records, unlinks
/// the three collision spheres and snapshots the root matrix with the Y scale
/// `field_430` at 0x1000, then switches the model to light mode 1, clears the
/// frame counter and advances the state.
void hopperBeginDeath(Task* arg0)
{
    GfxCoord*        coord = arg0->extra.tmd->coords;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    Actor341700Work* work  = (Actor341700Work*)arg0->work;
    Actor341700Work* objWork;

    enemy->recs = 0;

    objWork = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430    = 0x1000;
    work->savedRootMtx = coord->coord;

    Gp_SetLightMode(arg0->spawnArg2.pointer, 1);

    work->field_412 = 0;
    work->field_420++;
}
