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
    sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 2), 0xF);
    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
    worldTargetUnlinkNode(&enemy->node);
    work->state = work->state + 1;
}
