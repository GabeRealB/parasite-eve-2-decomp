/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Final dead state. It frees the model's buffers and stops their auto-
/// allocation, bursts the body into gib effects (madChaserSpawnGibs) and releases
/// the battle state. It clears the enemy's contact records, unlinks the three
/// collision bodies and parks the task in state 5 with both state indices
/// reset.
void madChaserBurst(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    TmdObject*     model;
    Enemy*         enemy;

    model = arg0->extra.tmd;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    tmdFreePrimitiveBuffer(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    madChaserSpawnGibs(arg0);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    work        = (MadChaserWork*)arg0->work;
    Gp_UnlinkObj(&work->pairBody);
    Gp_UnlinkObj(&work->gridBody);
    Gp_UnlinkObj(&work->attackBody);
    work2           = (MadChaserWork*)arg0->work;
    arg0->state     = 5;
    work2->state    = 0;
    work2->subState = 0;
}
