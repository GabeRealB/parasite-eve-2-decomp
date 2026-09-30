/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Runs the actor's branch sequence. State 0 puts the slot set on animation
/// 0xD or 0x11, whichever `field_6D2` selects, and parks the state at 1 or 2 to
/// match. States 1 and 2 queue the actor's cue at frame 0x2C / 0x19 and, once
/// `field_6C4` reaches 0x42 / 0x31, move to state 3 with an LCG-rolled
/// `field_6D4` countdown; states 3 and 4 then alternate on that countdown.
void stalkerKneelSeq(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              snd;
    s32              anim;
    u32              random;
    s16              timer;

    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->field_6D2 == 0) {
                work->field_6C0 = 0xD;
                work->field_6CE = 1;
                work->field_6F0 = 1;
                work->field_490 = -0xA7;
            } else {
                work->field_6C0 = 0x11;
                work->field_6CE = 2;
                work->field_6F0 = 2;
                work->field_490 = 0x109;
            }
            work->field_498  = 0x15E;
            work->field_714  = 1;
            work->field_6DA  = 7;
            work->field_6F2  = 2;
            work->field_6C8  = 0;
            work->field_49A |= 0x4000;
            work->field_502 &= 0xBFFF;
            break;
        case 1:
            if (work->field_6C4 == 0x2C) {
                snd = gStalkerAnimCues[work->field_712 + 8] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (work->field_6C4 >= 0x42) {
                work->field_6C0 = 0x10;
                work->field_6CE = 3;
                work->field_6F2 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_6D4 = (random >> 16) & 0x3F;
            }
            if (work->field_714 == 1) {
                work->field_714 = 2;
            }
            break;
        case 2:
            if (work->field_6C4 == 0x19) {
                snd = gStalkerAnimCues[work->field_712 + 8] | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
            }
            if (work->field_6C4 >= 0x31) {
                work->field_6C0 = 0x14;
                work->field_6CE = 3;
                work->field_6F2 = 0;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_6D4 = (random >> 16) & 0x3F;
            }
            if (work->field_714 == 1) {
                work->field_714 = 2;
            }
            break;
        case 3:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                anim = 0x13;
                if (work->field_6F0 == 1) {
                    anim = 0xF;
                }
                work->field_6D4 = 0xA;
                work->field_6C0 = anim;
                work->field_6CE = 4;
            }
            break;
        case 4:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                anim = 0x14;
                if (work->field_6F0 == 1) {
                    anim = 0x10;
                }
                work->field_6C0 = anim;
                work->field_6CE = 3;
                random          = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState     = random;
                work->field_6D4 = (random >> 16) & 0x3F;
            }
            break;
    }
}
