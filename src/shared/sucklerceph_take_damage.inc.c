/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Damage reaction of the first enemy: `arg1` comes off its HP and goes
/// through `func_800DA6E8`. A depleted enemy is killed through
/// `sucklercephKill` and put into its death state with a five-frame
/// countdown. A live one plays the hurt sound from the set `variant` picks,
/// re-arms `field_2CC`, and while animation 1 plays latches `wakeRequested`.
void sucklercephTakeDamage(Task* arg0, s32 arg1)
{
    SucklercephWork* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              anim;
    s32              soundId;

    enemy      = arg0->spawnArg2.pointer;
    obj        = arg0->extra.tmd;
    coord      = obj->coords;
    work       = arg0->work;
    enemy->hp -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp < 0) {
        sucklercephKill(arg0, 0);
        arg0->state         = 2;
        arg0->killCountdown = 5;
        work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
        return;
    }
    if (work->variant != 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000A;
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    } else {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0002;
        sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
    }
    anim            = work->animId;
    work->field_2CC = 0xF;
    if (anim == 1) {
        work->wakeRequested = anim;
    }
}
