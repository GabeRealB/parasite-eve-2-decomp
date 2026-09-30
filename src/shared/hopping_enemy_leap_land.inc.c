/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Plays sound 4 on the first frame; once the hit flags are set, draws a
/// 0x5A..0xD9 cooldown into `field_44A` and moves the state machine to
/// state 3.
void hopperLeapLand(Task* arg0)
{
    Actor341700Work* work;
    Actor341700Work* work2;
    s32              soundId;
    s32              pan;
    u32              rand;

    work            = (Actor341700Work*)arg0->work;
    work->field_438 = 0;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)gpGetObjDepth(arg0->extra.tmd->coords));
    }
    if (hopperAnimEnded(arg0) != 0) {
        rand             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState      = rand;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work2            = (Actor341700Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}
