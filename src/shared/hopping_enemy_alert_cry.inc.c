/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Requests animation 9, clears the frame counter, advances the sub-state
/// and, while the enemy has HP left, plays sound 2.
void hopperAlertCry(Task* arg0)
{
    Actor341700Work* work;
    Enemy*           enemy;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    enemy           = (Enemy*)arg0->spawnArg2.pointer;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 9;
    work->field_414 = 1;
    work->field_412 = 0;
    work->field_422++;
    if (enemy->hp > 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0002;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
}
