/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Plays sound 2, releases `Gp_StateF0`'s hold if it is this enemy's,
/// unlinks the enemy node, detaches its records and unlinks its three hit
/// bodies, hides the model and advances the state.
void hopperVanish(Task* arg0)
{
    Actor341700Work* work2;
    Actor341700Work* work;
    Enemy*           enemy;
    TmdObject*       model;

    work            = (Actor341700Work*)arg0->work;
    enemy           = (Enemy*)arg0->spawnArg2.pointer;
    model           = arg0->extra.tmd;
    work->field_412 = 0;
    SndEvt_EnqueueType7(0x402C0002, 1);
    if ((Gp_StateF0.field_1F & 0xF) == (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    enemy->recs = 0;
    work2       = (Actor341700Work*)arg0->work;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    model->flags    = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    work->field_420 = work->field_420 + 1;
}
