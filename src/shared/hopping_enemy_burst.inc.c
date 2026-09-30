/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Final dead state. It frees the model's buffers and stops their auto-
/// allocation, bursts the body into gib effects (hopperSpawnGibs) and releases
/// the battle state. It clears the enemy's contact records, unlinks the three
/// collision bodies and parks the task in state 5 with both state indices
/// reset.
void hopperBurst(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    TmdObject*       model;
    Enemy*           enemy;

    model = arg0->extra.tmd;
    enemy = (Enemy*)arg0->spawnArg2.pointer;
    Tmd_FreeBuffers(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    hopperSpawnGibs(arg0);
    Gp_ReleaseStateF0Add(arg0, 0);
    enemy->recs = 0;
    work        = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&work->obj_2AC);
    Gp_UnlinkObj(&work->obj_2CC);
    Gp_UnlinkObj(&work->obj_3AC);
    work2            = (Actor341700Work*)arg0->work;
    arg0->state      = 5;
    work2->field_420 = 0;
    work2->field_422 = 0;
}
