/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Plays sound 2, releases its alert hold, unlinks the target node and
/// advances. Both command-death tables use it; the second entry includes the
/// fragment again under its own name.
void hopperDeathCryUnlink(Task* arg0)
{
    Actor341700Work* work;
    Enemy*           enemy;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (Actor341700Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.hopperAlertOwner & SCENE_COMBAT_HOPPER_OWNER_MASK) == (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        Gp_StateF0.hopperAlertOwner = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}
