/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays sound 2, releases its alert hold, unlinks the target node and
/// advances. Both command-death tables use it; the second entry includes the
/// fragment again under its own name.
void madChaserDeathCryUnlink(Task* arg0)
{
    MadChaserWork* work;
    Enemy*         enemy;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    work  = (MadChaserWork*)arg0->work;
    SndEvt_EnqueueType7(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0002, 0xF);
    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}
