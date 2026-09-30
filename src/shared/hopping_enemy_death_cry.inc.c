/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Death cry: plays sound 2, releases this enemy's `Gp_StateF0` hold and
/// unlinks the enemy node. A pending request 4 hides the model and jumps to
/// state 7; otherwise the state advances.
void hopperDeathCry(Task* arg0)
{
    GpEnemy*         enemy;
    Actor341700Work* work;
    TmdObject*       model;
    Actor341700Work* work2;

    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    model = arg0->extra.tmd;
    work  = (Actor341700Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x402C0002, 0xF);
    hopperSetAlertHold(arg0, 0);
    Gp_UnlinkNode(&enemy->node);
    if (work->field_448 == 4) {
        work->field_412  = 0;
        model->flags     = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 7;
        work2->field_422 = 0;
        return;
    }
    work->field_420 = work->field_420 + 1;
}
