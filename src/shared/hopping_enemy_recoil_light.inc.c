/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Light recoil: reads the playing animation's stance and requests 0xB
/// (upright, with a global sound 2) or 0x11, plays sound 3 and advances.
void hopperRecoilLight(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              soundId;
    s32              pan;

    work            = (Actor341700Work*)arg0->work;
    work->field_44F = gHopperAnimStance[work->field_418 - 1];
    if (work->field_44F == 1) {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xB;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work2            = (Actor341700Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0x11;
        work2->field_414 = 1;
    }
    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
    SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    work->field_422++;
}
