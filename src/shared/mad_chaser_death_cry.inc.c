/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Death cry: plays sound 2, releases this enemy's `gSceneCombatState` hold and
/// unlinks the enemy node. A pending request 4 hides the model and jumps to
/// state 7; otherwise the state advances.
void madChaserDeathCry(Task* arg0)
{
    Enemy*         enemy;
    MadChaserWork* work;
    TmdObject*     model;
    MadChaserWork* work2;

    enemy = (Enemy*)arg0->spawnArg2.pointer;
    model = arg0->extra.tmd;
    work  = (MadChaserWork*)arg0->work;
    sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 2), 0xF);
    _madChaserSetAlertHold(arg0, 0);
    worldTargetUnlinkNode(&enemy->node);
    if (work->hitReaction == MAD_CHASER_HIT_REACTION_BLAST) {
        work->stateFrames = 0;
        model->flags      = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        work2             = (MadChaserWork*)arg0->work;
        work2->state      = 7;
        work2->subState   = 0;
        return;
    }
    work->state = work->state + 1;
}
