/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Plays sound 2, releases its alert hold, unlinks the target node and
/// advances. Both command-death tables use it; the second entry includes the
/// fragment again under its own name.
void hopperDeathCryUnlink(Task* arg0)
{
    Actor341700Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2.pointer;
    work  = (Actor341700Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}
