/* Part of the burster library; see burster.h. */

/// Damage reaction of the first enemy: `arg1` comes off its HP and goes
/// through `func_800DA6E8`. A depleted enemy is killed through
/// `bursterKill` and put into its death state with a five-frame
/// countdown. A live one plays the hurt sound from the set `field_2D6` picks,
/// re-arms `field_2CC`, and while animation 1 plays latches `field_2D8`.
void bursterTakeDamage(Task* arg0, s32 arg1)
{
    Actor104600Work* work;
    Enemy*           enemy;
    TmdObject*       obj;
    GfxCoord*        coord;
    s32              anim;
    s32              soundId;

    enemy      = arg0->spawnArg2.pointer;
    obj        = arg0->extra.tmd;
    coord      = obj->coords;
    work       = (Actor104600Work*)arg0->work;
    enemy->hp -= arg1;
    func_800DA6E8(&enemy->node, arg1, 0);
    if (enemy->hp < 0) {
        bursterKill(arg0, 0);
        arg0->state         = 2;
        arg0->killCountdown = 5;
        work->field_2B4     = 0;
        return;
    }
    if (work->field_2D6 != 0) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4046000A;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    } else {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0002;
        SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
    }
    anim            = work->field_2B8;
    work->field_2CC = 0xF;
    if (anim == 1) {
        work->field_2D8 = anim;
    }
}
